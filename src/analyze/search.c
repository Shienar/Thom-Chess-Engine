#include "analyze/search.h"
#include "debug.h"
#include "analyze/syzygy.h"
#include "analyze/cuckoo.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "pyrrhic/tbconfig.h"
#include "analyze/nnue/neuralnet.h"
#include "analyze/hce/hce.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int thread_count = 1;
int enable_ponder = 0;
int is_searching = 0;
int suppress_uci_messages = 0;

uint8_t ponder_locks_initialized = 0;
cond_var ponder_cond_var;
mutex ponder_lock;

//Global flag. Each context's abort flag will
//point to this with the exception of data generation
//since that involves multiple games from one process.
uint8_t abort_flag = 0;

volatile uint8_t is_ponder = 0;

const int min_aspiration_depth = 5;
const int stable_reduction_depth = 5;
const int reverse_futility_pruning_depth = 4;
const int futility_pruning_depth = 7;
const int nullmove_pruning_depth = 3;
const int tt_reduction_depth = 4;
const int singular_extension_depth = 7;

const int max_extensions = 6;

//ax^2 + b
int razoring_a = 122;
int razoring_b = 63;

int initial_aspiration_margin = 10;
float aspiration_margin_mult_factor = 1.5f;

int futility_margin = 70;
int futility_depth_margin = 50;

int reverse_futility_margin = 70;
int reverse_futility_margin_improving = 50;

int stable_eval_margin = 10;

//a * log(depth) * log(move_count) / b
float lmr_a = 0.649f;
float lmr_b = 3.363f;

//a * depth * depth + b
float lmp_a = 1.849f;
float lmp_b = 3.0f;
float lmp_improving_a = 1.434f;
float lmp_improving_b = 2.0f;

int lmr_table[MAX_PLY][MAX_MOVES] = {0};
int lmp_table[2][MAX_PLY] = {0};

void init_search_tables(void)
{
	for (int depth = 4; depth < MAX_PLY; depth++) {
		int count = 2.0f + 0.5f * depth * depth;

		for (int move_count = 1; move_count < MAX_MOVES; move_count++)
			if (move_count >= count)
				lmr_table[depth][move_count] = (int)(lmr_a + log(depth) * log(move_count) / lmr_b);
	}

	for (int depth = 0; depth < MAX_PLY; depth++) {
		lmp_table[0][depth] = lmp_a * depth * depth + lmp_b;
		lmp_table[1][depth] = lmp_improving_a * depth * depth + lmp_improving_b;
	}
}

//Draws get ignored. Naturally stops depth at checkmate/stalemate positions.
uint64_t perft(bitboard *board, int depth, int verbose)
{
	if (!depth)
		return 1;
	uint64_t nodes = 0;

	move move_list[MAX_MOVES];
	int count = generate_move_list(move_list, board, GET_ALL_MOVES);
	bitboard new_board;

	for (int index = 0; index < count; index++) {
		if (move_from_struct(board, &new_board, move_list[index], NULL))
			continue;

		uint64_t branch_nodes = perft(&new_board, depth - 1, 0);

		nodes += branch_nodes;
		if (verbose)
			printf("Move %s: nodes %" PRId64 "\n", get_move_square_name(move_list[index]), branch_nodes);
	}

	return nodes;
}

uint64_t iterator_perft(thread_context *context, int depth, int ply, int verbose)
{
	if (!depth)
		return 1;
	uint64_t nodes = 0;

	bitboard *board = &context->board_stack[ply];
	bitboard *new_board = &context->board_stack[ply + 1];
	move_iterator *iter = create_move_iterator(context, GET_ALL_MOVES, ply, NULL);

	move *current_move;

	while ((current_move = iterate_next_move(iter)) != NULL) {
		if (move_from_struct(board, new_board, *current_move, NULL))
			continue;

		uint64_t branch_nodes = iterator_perft(context, depth - 1, ply + 1, 0);

		nodes += branch_nodes;
		if (verbose)
			printf("Move %s: nodes %" PRId64 "\n", get_move_square_name(*current_move), branch_nodes);
	}

	return nodes;
}

int evaluate(thread_context *context, int ply)
{
	RECORD_SEARCH(context->evaluations++;);
	return (use_nnue) ? forward_propagate(context, ply) : hce_eval(&context->board_stack[ply]);
}

int should_abort_search(thread_context *context, int ply)
{
	return *context->abort_flag ||
		   (context->completed_depth && ply > 0 && !is_ponder &&
			(((context->counted_nodes & 1023) == 0 && clock() > context->hard_end_time) ||
			 context->counted_nodes >= (context->hard_max_nodes / thread_count)));
}

int quiescent_search(thread_context *context, int alpha, int beta, int ply, int pv_node)
{
	bitboard *cur_board = &context->board_stack[ply];

	if (should_abort_search(context, ply)) {
		*context->abort_flag = 1;
		return 0;
	}

	if (ply > 0 && alpha < 0 && is_upcoming_repetition(cur_board, context->repetitions, ply)) {
		alpha = 0;
		if (alpha >= beta)
			return alpha;
	}
	if (is_draw(cur_board, &context->repetitions) == VICTOR_DRAW)
		return (ply & 3) - 1;
	if (ply >= MAX_PLY - 1)
		return evaluate(context, ply);

	bitboard *next_board = &context->board_stack[ply + 1];

	if (pv_node)
		context->seldepth = _max(context->seldepth, ply);

	int lowest_bound = alpha;
	move *tt_move = NULL;
	move temp; //Copy in from TT instead of saving a ptr to a volatile TT slot.

	int best;
	uint8_t tt_hit;
	tt_entry entry = tt_get(cur_board, context->tt, &tt_hit, ply);

	if (tt_hit) {
		RECORD_SEARCH(context->tt_hits++;);
		if (entry.node_type == NODE_BOUND_EXACT ||
			(entry.node_type == NODE_BOUND_UPPER && entry.evaluation <= alpha) ||
			(entry.node_type == NODE_BOUND_LOWER && entry.evaluation >= beta)) {
				RECORD_SEARCH(context->tt_cutoffs++;);
				return entry.evaluation;
			}

		temp = entry.best_move;
		tt_move = &temp;
		best = entry.evaluation;
	} else {
		RECORD_SEARCH(context->tt_misses++;);
		best = evaluate(context, ply);
		best += get_correction_history_offset(context, cur_board);

		tt_entry shallow_entry = {
			.depth = 0,
			.node_type = NODE_BOUND_UNKNOWN,
			.evaluation = best
		};
		tt_set(context->tt, shallow_entry, cur_board->hash_code, ply);
	}

	//Stand Pat
	alpha = _max(alpha, best);
	if (alpha >= beta)
		return best;

	move_iterator *iter = create_move_iterator(context, cur_board->in_check ? GET_ALL_MOVES : GET_NOISY_MOVES, ply, tt_move);

	int valid_moves_visited = 0;
	move best_move = {0};

	if (iter) {
		move *current_move;

		while ((current_move = iterate_next_move(iter)) != NULL) {
			if (move_from_struct(cur_board, next_board, *current_move, &context->repetitions))
				continue;
			context->move_stack[ply] = *current_move;

			//SEE pruning
			if (!next_board->in_check && iter->move_scores[iter->selected_index] < 0)
				continue;

			valid_moves_visited++;
			tt_prefetch(context->tt, next_board->hash_code);
			context->is_acc_clean[ply + 1] = 0;
			context->last_clean_ply = _min(context->last_clean_ply, ply);

			context->counted_nodes++;
			RECORD_SEARCH(context->qs_nodes++;);

			int score = -quiescent_search(context, -beta, -alpha, ply + 1, pv_node);

			if (score > best) {
				best = score;
				best_move = *current_move;

				if (score > alpha)
					alpha = score;

				if (alpha >= beta)
					break;
			}
		}
		free(iter);
	}

	if (valid_moves_visited > 0) {
		RECORD_SEARCH(context->quiescent_searched_moves += valid_moves_visited;
					  context->quiescent_searched_positions++;);
		tt_entry shallow_entry = {
			.depth = 0,
			.node_type = (best >= beta) ? NODE_BOUND_LOWER : ((best > lowest_bound) ? NODE_BOUND_EXACT : NODE_BOUND_UPPER),
			.evaluation = best,
			.best_move = best_move
		};
		tt_set(context->tt, shallow_entry, cur_board->hash_code, ply);
	} else if (cur_board->in_check)
		return -SCORE_WIN + ply;

	return best;
}

int principal_variation_search(thread_context *context, int alpha, int beta, int depth, int ply, principal_variation *my_pv, int pv_node, int cut_node)
{
	assert(context);
	bitboard *cur_board = &context->board_stack[ply];

	my_pv->length = 0;
	principal_variation child_pv;

	depth = _min(depth, MAX_PLY - 1 - ply);

	move *tt_move = NULL;
	move temp; //Copy in from TT instead of saving a ptr to a volatile TT slot.

	move best_move = {0};

	int searched_quiet_indices[MAX_MOVES] = {0};
	int searched_quiet_count = 0;
	int should_skip_quiets = 0;

	int lowest_bound = alpha;

	if (should_abort_search(context, ply)) {
		*context->abort_flag = 1;
		return 0;
	}

	if (pv_node)
		context->seldepth = _max(context->seldepth, ply);

	if (alpha < 0 && is_upcoming_repetition(cur_board, context->repetitions, ply)) {
		alpha = 0;
		if (alpha >= beta)
			return alpha;
	}
	if (is_draw(cur_board, &context->repetitions) == VICTOR_DRAW)
		return (ply & 3) - 1;

	//Mate distance pruning for non-root nodes.
	if (ply != 0) {
		alpha = _max(alpha, -SCORE_WIN + ply);
		beta  = _min(beta,   SCORE_WIN - ply - 1);
		if (alpha >= beta)
			return alpha;
	}

	int score = 0;

	//Transposition table
	tt_entry new_tt_entry = {
		.depth = depth,
	};
	uint8_t tt_hit;
	tt_entry old_tt_entry = tt_get(cur_board, context->tt, &tt_hit, ply);

	if (tt_hit && context->excluded_move[ply].raw && old_tt_entry.best_move.raw == context->excluded_move[ply].raw)
		tt_hit = 0;

	if (tt_hit) {
		RECORD_SEARCH(context->tt_hits++;);
		if (old_tt_entry.depth >= depth &&
			(!pv_node || depth == 0) &&
			(cut_node || old_tt_entry.evaluation <= alpha)) {
			if (old_tt_entry.node_type == NODE_BOUND_EXACT ||
				(old_tt_entry.node_type == NODE_BOUND_UPPER && old_tt_entry.evaluation <= alpha) ||
				(old_tt_entry.node_type == NODE_BOUND_LOWER && old_tt_entry.evaluation >= beta)) {
					move m = (move)old_tt_entry.best_move;

					if (m.raw) {
						int current_piece = (cur_board->piece_arr[m.start_square]);
						int captured_piece = (cur_board->piece_arr[m.end_square]);

						if (captured_piece == EMPTY_PIECE && ISPAWN(current_piece)) {
							if (m.end_square == cur_board->ep_square)
								captured_piece = FLIP_COLOR(current_piece);
							else if (m.promote_to)
								captured_piece = current_piece;
						}
						//Direct checks are noisy, but they are a bit expensive to check here.
						if (!captured_piece) {
							update_killer_moves(context, m, ply);
							update_history_values((int16_t *)context->history_table[cur_board->turn],
												  ((PIECE(current_piece) / 2) * 64) + m.end_square,
												  NULL, 0, depth);
							update_continuation_history(context, m, ply);
						}
					}

					RECORD_SEARCH(context->tt_cutoffs++;);
					return old_tt_entry.evaluation;
				}
		}

		temp = old_tt_entry.best_move;
		tt_move = &temp;
		score = old_tt_entry.evaluation;
	}

	if (depth <= 0)
		return quiescent_search(context, alpha, beta, ply, pv_node);

	//Syzygy
	if (!pv_node && depth >= syzygyProbeDepth) {
		int result = get_syzygy_result(cur_board);

		if (result != -1) {
			if (result > 0)
				result -= ply;
			else if (result < 0)
				result += ply;

			new_tt_entry.node_type = NODE_BOUND_EXACT;
			new_tt_entry.evaluation = result;
			tt_set(context->tt, new_tt_entry, cur_board->hash_code, ply);
			return result;
		}
	}

	if (!tt_hit) {
		if (!(context->excluded_move[ply].raw && old_tt_entry.best_move.raw == context->excluded_move[ply].raw))
			RECORD_SEARCH(context->tt_misses++;);
		if (cur_board->in_check)
			score = -SCORE_WIN;
		else if (context->excluded_move[ply].raw)
			score = context->eval_history[ply];
		else {
			//TT Reductions
			//Try to generate some history values and a tt move using a shallow search, then continue with a reduced depth
			if (depth > tt_reduction_depth) {
				score = principal_variation_search(context, alpha, beta, 1, ply, &child_pv, pv_node, cut_node);

				old_tt_entry = tt_get(cur_board, context->tt, &tt_hit, ply);
				temp = old_tt_entry.best_move;
				tt_move = &temp;

				depth--;
				RECORD_SEARCH(context->tt_reductions++;);
			} else {
				score = evaluate(context, ply);
				score += get_correction_history_offset(context, cur_board);

				tt_entry shallow_entry = {
					.depth = 0,
					.node_type = NODE_BOUND_UNKNOWN,
					.evaluation = score
				};
				tt_set(context->tt, shallow_entry, cur_board->hash_code, ply);
			}
		}
	}

	if (ply >= MAX_PLY - 1)
		return score;

	bitboard *next_board = &context->board_stack[ply + 1];

	context->extensions[ply] = (ply == 0) ? 0 : context->extensions[ply - 1];

	//Improving/Worsening
	int static_score = score;

	context->eval_history[ply] = static_score;
	context->improving[ply] = !cur_board->in_check && (ply < 2 || score >= context->eval_history[ply - 2]);
	context->worsening[ply] = (context->improving[ply] || ply < 2) ? cur_board->in_check : context->worsening[ply - 2] + 1;

	context->killer_moves[ply + 1][0].raw = context->killer_moves[ply + 1][1].raw = 0;

	if (!pv_node && !cur_board->in_check && abs(score) < MIN_MATE_SCORE && !context->excluded_move[ply].raw) {
		//Stable Eval Reduction
		if (ply >= 2 && static_score >= beta && cut_node && depth > stable_reduction_depth && abs(context->eval_history[ply - 2] - static_score) < stable_eval_margin) {
			RECORD_SEARCH(context->stable_reductions++;);
			depth--;
		}

		//Worsening Reduction
		if (score <= alpha && cut_node && context->worsening[ply] > 2) {
			RECORD_SEARCH(context->worsening_reductions++;);
			depth--;
		}

		//Reverse Futility Pruning
		if (depth <= reverse_futility_pruning_depth) {
			int reduced_val = context->improving[ply] ? score - reverse_futility_margin_improving * depth : score - reverse_futility_margin * depth;

			if (reduced_val >= beta) {
				RECORD_SEARCH(context->rfp_prunes++;);
				return reduced_val;
			}
		}

		//Razoring
		if (score + razoring_a * depth * depth + razoring_b <= alpha) {
			int q_score = quiescent_search(context, alpha - 1, alpha, ply, pv_node);

			if (q_score < alpha) {
				RECORD_SEARCH(context->razoring_prunes++;);
				return q_score;
			} else {
				RECORD_SEARCH(context->failed_razoring_prunes++;);
			}
		}

		//Null move pruning
		if (score >= beta &&
		   depth >= nullmove_pruning_depth &&
		   cut_node &&
		   !(ply > 0 && context->move_stack[ply - 1].raw == 0) &&
		   (cur_board->pieces_all ^ (cur_board->pieces[WHITE_KING] | cur_board->pieces[BLACK_KING] | cur_board->pieces[WHITE_PAWN] | cur_board->pieces[BLACK_PAWN]))) {
			int r = 3 + depth / 4 + depth / 10;

			apply_null_move(cur_board, next_board, &context->repetitions);
			tt_prefetch(context->tt, next_board->hash_code);

			context->is_acc_clean[ply + 1] = 0;
			context->last_clean_ply = _min(context->last_clean_ply, 0);

			context->move_stack[ply].raw = 0;
			int null_score = -principal_variation_search(context, -beta, -beta + 1, depth - r, ply + 1, &child_pv, 0, !cut_node);

			if (null_score >= beta) {
				RECORD_SEARCH(context->nmp_prunes++;);
				if (null_score < MIN_MATE_SCORE)
					return null_score;
				else
					return beta;
			}
			else {
				RECORD_SEARCH(context->failed_nmp_prunes++;);
			}
		}
	}

	move_iterator *iter = create_move_iterator(context, GET_ALL_MOVES, ply, tt_move);
	int valid_moves_visited = 0;
	int best_score = -SCORE_WIN;

	if (iter) {
		move *current_move;

		while ((current_move = iterate_next_move(iter)) != NULL) {
			if (current_move->raw == context->excluded_move[ply].raw)
				continue;

			int move_score = iter->move_scores[iter->selected_index];
			int next_depth = depth - 1;

			int current_piece = (cur_board->piece_arr[current_move->start_square]);
			int captured_piece = (cur_board->piece_arr[current_move->end_square]);

			if (captured_piece == EMPTY_PIECE && ISPAWN(current_piece)) {
				//En passant
				if (current_move->end_square == cur_board->ep_square)
					captured_piece = FLIP_COLOR(current_piece);
				//Treat non-capture promotions as pawn captures.
				//This gives them a capture history to go along with their SEE move ordering.
				else if (current_move->promote_to)
					captured_piece = current_piece;
			}

			//Singular Extension
			if (tt_hit && depth >= singular_extension_depth && current_move->raw == tt_move->raw && old_tt_entry.depth >= depth - 3 &&
				old_tt_entry.node_type != NODE_BOUND_UPPER && !context->excluded_move[ply].raw && ply > 0) {
				RECORD_SEARCH(context->singular_searches++;);

				int s_beta = old_tt_entry.evaluation - 3 * depth;
				int s_depth = depth / 2 + 1;

				context->excluded_move[ply] = *tt_move;
				int singular_score = principal_variation_search(context, s_beta - 1, s_beta, s_depth, ply, &child_pv, 0, cut_node);

				context->excluded_move[ply].raw = 0;

				if (singular_score < s_beta) {
					if (!pv_node && singular_score + 67 * s_depth < s_beta) {
						RECORD_SEARCH(context->triple_extensions++;);
						next_depth += 3;
					} else if (!pv_node && singular_score + 32 * s_depth < s_beta) {
						RECORD_SEARCH(context->double_extensions++;);
						next_depth += 2;
					} else {
						RECORD_SEARCH(context->single_extensions++;);
						next_depth++;
					}
				}
				//Multicut pruning, but we just take the strong singular and assume that there's going to be more.
				else if (singular_score >= beta && abs(singular_score) < MIN_MATE_SCORE) {
					RECORD_SEARCH(context->multicut_prunes++;
									context->pvs_searched_moves++;
									context->pvs_searched_positions++;);
					return singular_score;
				}
				//Negative Extension
				else if (cut_node || old_tt_entry.evaluation >= beta) {
					RECORD_SEARCH(context->negative_extensions++;);
					next_depth -= 2;
				} else {
					RECORD_SEARCH(context->failed_extensions++;);
				}
			}

			//Quiet Move Pruning
			if (!should_skip_quiets && !cur_board->in_check) {
				//Late move pruning
				if (valid_moves_visited > lmp_table[context->improving[ply]][ply])
					should_skip_quiets = 1;

				//Futility Pruning
				int lmr_depth = _max(0, depth - lmr_table[depth][valid_moves_visited]);

				if (lmr_depth <= futility_pruning_depth && static_score + lmr_depth * futility_depth_margin + futility_margin <= alpha)
					should_skip_quiets = 1;
			}

			if (move_from_struct(cur_board, next_board, *current_move, &context->repetitions))
				continue;
			context->move_stack[ply] = *current_move;

			int is_quiet_move = !next_board->in_check && captured_piece == EMPTY_PIECE;

			if (is_quiet_move && should_skip_quiets && !pv_node && abs(best_score) < MIN_MATE_SCORE &&
				current_move->raw != iter->killer1_move.raw &&
				current_move->raw != iter->killer2_move.raw)
					continue;
			tt_prefetch(context->tt, next_board->hash_code);

			//Check extensions
			if (!pv_node && next_board->in_check)
				next_depth++;

			//Late move reduction
			if (!pv_node && is_quiet_move && !context->improving[ply])
				next_depth -= lmr_table[depth][valid_moves_visited];

			//SEE reduction, Low history reduction
			if (!pv_node && move_score < 0)
				next_depth -= 2;

			//Limit extensions
			int ext = next_depth - (depth - 1);

			if (ext > 0) {
				context->extensions[ply] += ext;
				if (context->extensions[ply] > max_extensions) {
					int overflow = context->extensions[ply] - max_extensions;

					context->extensions[ply] -= overflow;
					next_depth -= overflow;
					RECORD_SEARCH(context->extension_overflow += overflow;);
				}
			}

			context->is_acc_clean[ply + 1] = 0;
			context->last_clean_ply = _min(context->last_clean_ply, ply);

			context->counted_nodes++;
			RECORD_SEARCH(context->pvs_nodes++;);

			if (pv_node && (valid_moves_visited == 0 || score > alpha))
				score = -principal_variation_search(context, -beta, -alpha, next_depth, ply + 1, &child_pv, 1, 0);
			else {
				//Scout
				score = -principal_variation_search(context, -alpha - 1, -alpha, next_depth, ply + 1, &child_pv, 0, 1);

				//LMR Re-search
				if (score > alpha && next_depth < depth - 1) {
					next_depth = depth - 1;
					score = -principal_variation_search(context, -alpha - 1, -alpha, next_depth, ply + 1, &child_pv, 0, !cut_node);
				}

				//PVS Re-search
				if (score > alpha && pv_node)
					score = -principal_variation_search(context, -beta, -alpha, next_depth, ply + 1, &child_pv, 1, 0);
			}

			valid_moves_visited++;
			if (score > best_score) {
				best_score = score;
				best_move = *current_move;

				if (score > alpha) {
					alpha = score;

					//Save PV
					if (pv_node) {
						my_pv->line[0] = *current_move;
						my_pv->hash_codes[0] = cur_board->hash_code;
						memcpy(&my_pv->line[1], child_pv.line, child_pv.length * sizeof(move));
						memcpy(&my_pv->hash_codes[1], child_pv.hash_codes, child_pv.length * sizeof(uint64_t));
						my_pv->length = child_pv.length + 1;
					}
				}

				if (alpha >= beta) {
					if (is_quiet_move) {
						update_killer_moves(context, *current_move, ply);

						update_history_values((int16_t *)context->history_table[cur_board->turn],
											((PIECE(current_piece) / 2) * 64) + current_move->end_square,
											searched_quiet_indices, searched_quiet_count, depth);

						update_continuation_history(context, *current_move, ply);
					} else if (captured_piece != EMPTY_PIECE)
						apply_capture_history_bonus(&context->capture_history_table[current_piece / 2][current_move->end_square][captured_piece / 2], depth);

					break;
				}
			}

			if (is_quiet_move)
				searched_quiet_indices[searched_quiet_count++] = ((PIECE(current_piece) / 2) * 64) + current_move->end_square;
			else if (captured_piece != EMPTY_PIECE)
				apply_capture_history_penalty(&context->capture_history_table[current_piece / 2][current_move->end_square][captured_piece / 2], depth);
		}
		free(iter);
	}

	if (!iter || valid_moves_visited == 0) {
		//We know its not a (stale-)mate position if an excluded move exists at this ply.
		if (context->excluded_move[ply].raw)
			return alpha;
		else if (get_mate_result(cur_board) == VICTOR_DRAW)
			return 0;
		else
			return -SCORE_WIN + ply;
	}

	new_tt_entry.node_type = (best_score >= beta) ? NODE_BOUND_LOWER : ((best_score > lowest_bound) ? NODE_BOUND_EXACT : NODE_BOUND_UPPER);
	new_tt_entry.evaluation = best_score;
	new_tt_entry.best_move = best_move;
	tt_set(context->tt, new_tt_entry, cur_board->hash_code, ply);

	//Correction History
	if ((best_score >= beta || new_tt_entry.node_type == NODE_BOUND_EXACT) && abs(best_score) < MIN_MATE_SCORE && !context->excluded_move[ply].raw) {
		update_correction_history(&context->pawn_corrhist[cur_board->turn][cur_board->pawn_hash & (CORRHIST_SIZE - 1)],
									depth, best_score, static_score);
		update_correction_history(&context->nonpawn_corrhist[WHITE][cur_board->turn][cur_board->nonpawn_hash[WHITE] & (CORRHIST_SIZE - 1)],
									depth, best_score, static_score);
		update_correction_history(&context->nonpawn_corrhist[BLACK][cur_board->turn][cur_board->nonpawn_hash[BLACK] & (CORRHIST_SIZE - 1)],
									depth, best_score, static_score);
	}

	RECORD_SEARCH(context->pvs_searched_moves += valid_moves_visited;
				  context->pvs_searched_positions++;);

	return best_score;
}

void print_resulting_moves(move best_move, move ponder_move)
{
	if (suppress_uci_messages)
		return;

	printf("bestmove %s%s", get_move_square_name(best_move), promote_symbols[best_move.promote_to]);

	if (enable_ponder && ponder_move.raw)
		printf(" ponder %s%s", get_move_square_name(ponder_move), promote_symbols[ponder_move.promote_to]);

	printf("\n");
	fflush(stdout);
}

void aspiration_window(thread_context *context, int current_depth)
{
	//context->pv is used to save last stable pv line.
	//It is used for reporting and testing pv moves.
	//Don't corrupt it.

	principal_variation temp_pv = {0};

	memset(context->is_acc_clean, 0, MAX_PLY * sizeof(uint8_t));
	context->is_acc_clean[0] = 1;
	context->last_clean_ply = 0;
	int score;

	if (current_depth < min_aspiration_depth) {
		score = principal_variation_search(context, -SCORE_WIN, SCORE_WIN, current_depth, 0, &temp_pv, 1, 0);
		context->completed_depth = current_depth;
	} else {

		int aspiration_margin = initial_aspiration_margin;
		int alpha = context->score - aspiration_margin;
		int beta = context->score + aspiration_margin;

		while (*context->abort_flag == 0) {
			score = principal_variation_search(context, alpha, beta, current_depth, 0, &temp_pv, 1, 0);

			if (score <= alpha) {
				beta = (alpha + beta) / 2;
				alpha = score - aspiration_margin;
			} else if (score >= beta) {
				alpha = (alpha + beta) / 2;
				beta = score + aspiration_margin;
			} else
				break;

			aspiration_margin *= aspiration_margin_mult_factor;
		}
	}

	if (*context->abort_flag == 0) {
		context->score = score;
		context->completed_depth = current_depth;
		memcpy(&context->pv, &temp_pv, sizeof(principal_variation));
	}
}

THREAD_RETURN helper_thread_function(THREAD_PARAM param)
{
	thread_context *context = (thread_context *)param;

	context->seldepth = 0;
	context->completed_depth = 0;

	bitboard *board = &context->board_stack[0];
	move best_move = context->pv.line[0];

	if (use_nnue)
		update_accumulator_from_table(board, &context->accumulator_stack[0], context->refresh_table);

	int last_score = context->score;

	int consecutive_time_reductions = 0;

	for (int current_depth = 1; current_depth <= context->max_depth; current_depth += context->deepening_skip) {
		aspiration_window(context, current_depth);

		clock_t cur_time = 0;
		//Non-data generation check
		if (context->start_time) {
			cur_time = clock();
			if (context->hard_end_time - cur_time > 0.3 * (context->hard_end_time - context->start_time)) {
				if (consecutive_time_reductions < 5 && (best_move.raw == context->pv.line[0].raw || abs(context->score - last_score) < 5)) {
					context->soft_end_time -= 0.1 * (context->soft_end_time - context->start_time);
					consecutive_time_reductions++;
				} else {
					consecutive_time_reductions = 0;
					context->soft_end_time = context->hard_end_time;
				}
			}
		}

		best_move = context->pv.line[0];
		last_score = context->score;

		if (context->start_time && abs(context->score) > MIN_MATE_SCORE)
			context->soft_end_time -= 0.5 * (context->soft_end_time - context->start_time);

		if (!is_ponder && (*context->abort_flag || clock() > context->soft_end_time || context->counted_nodes >= context->soft_max_nodes / thread_count))
			break;
	}

	return 0;
}

void find_best_thread(thread_context *main_thread, thread_context *helper_threads, move *best_move, move *ponder_move)
{
	thread_context *best = main_thread;
	int best_depth = best->completed_depth;
	int best_score = best->score;
	int total_nodes = main_thread->counted_nodes;

	if (helper_threads) {
		for (int i = 0; i < thread_count - 1; i++) {
			total_nodes += helper_threads[i].counted_nodes;

			if (!helper_threads[i].pv.line[0].raw)
				continue;
			int cur_depth = helper_threads[i].completed_depth;
			int cur_score = helper_threads[i].score;

			if (cur_depth >= best_depth || cur_score >= MIN_MATE_SCORE) {
				best = &helper_threads[i];
				best_depth = best->completed_depth;
				best_score = best->score;
			}
		}
	}

	*best_move = best->pv.line[0];
	*ponder_move = best->pv.line[1]; //Invalid & is_ponder checks come later.

	if (suppress_uci_messages)
		return;

	int milliseconds = (double)(clock() - main_thread->start_time) / (CLOCKS_PER_SEC / 1000.0);

	milliseconds = _max(milliseconds, 1);
	int nps = total_nodes / (milliseconds / 1000.0);

	printf("info depth %d seldepth %d score ", best_depth, best->seldepth);

	int abs_score = abs(best_score);

	assert(abs_score <= SCORE_WIN);
	if (abs_score >= MIN_MATE_SCORE) {
		int mate_in_plies = SCORE_WIN - abs_score;
		int mate_in_moves = (mate_in_plies + 1) / 2;

		if (best_score < 0)
			mate_in_moves = -mate_in_moves;
		printf("mate %d ", mate_in_moves);
	} else
		printf("cp %d ", best_score);

	printf("nodes %d nps %d hashfull %d time %d", total_nodes, nps, get_tt_hashfull(best->tt), milliseconds);

	printf(" pv");
	for (int i = 0; i < best->pv.length; i++) {
		move m = best->pv.line[i];

		if (!m.raw)
			break;
		printf(" %s%s", get_move_square_name(m), promote_symbols[m.promote_to]);
	}

	printf("\n");
	fflush(stdout);
}

THREAD_RETURN calculate_best_move(THREAD_PARAM param)
{
	srand(time(NULL));

	thread_context *context = (thread_context *)param;
	*context->abort_flag = 0;
	memset(context->history_table, 0, sizeof(context->history_table));
	memset(context->capture_history_table, 0, sizeof(context->capture_history_table));
	memset(context->counter_move, 0, sizeof(context->counter_move));
	memset(context->followup_move, 0, sizeof(context->followup_move));
	RECORD_SEARCH(memset(context->raw_search_data, 0, sizeof(context->raw_search_data)););

	tt_age(context->tt);

	int max_depth = context->max_depth;

	move best_move = (move){0};
	move ponder_move = (move){0};
	int helper_thread_count = thread_count - 1;

	bitboard *board = &context->board_stack[0];

	context->counted_nodes = 0;
	context->seldepth = 0;
	context->completed_depth = 0;

	//Syzygy move recommendations
	filter_syzygy_moves(board, context->searched_moves);

	THREADTYPE *helper_threads = NULL;
	thread_context *helper_thread_context = NULL;

	if (helper_thread_count > 0) {
		helper_threads = calloc(helper_thread_count, sizeof(THREADTYPE));
		helper_thread_context = calloc(helper_thread_count, sizeof(thread_context));

		for (int i = 0; i < helper_thread_count; i++) {
			helper_thread_context[i].abort_flag = context->abort_flag;

			memcpy(&helper_thread_context[i].board_stack[0], &context->board_stack[0], sizeof(bitboard));
			helper_thread_context[i].repetitions.capacity = context->repetitions.capacity;
			helper_thread_context[i].repetitions.hash_codes = calloc(helper_thread_context[i].repetitions.capacity, sizeof(uint64_t));
			memcpy(helper_thread_context[i].repetitions.hash_codes, context->repetitions.hash_codes, helper_thread_context[i].repetitions.capacity * sizeof(uint64_t));

			helper_thread_context[i].start_time = context->start_time;
			helper_thread_context[i].hard_end_time = context->hard_end_time;
			helper_thread_context[i].soft_end_time = context->soft_end_time;
			helper_thread_context[i].max_depth = context->max_depth;
			helper_thread_context[i].hard_max_nodes = context->hard_max_nodes;
			helper_thread_context[i].soft_max_nodes = context->soft_max_nodes;
			helper_thread_context[i].deepening_skip = 1 + (i%3);
			helper_thread_context[i].tt = context->tt;

			memcpy(helper_thread_context[i].pawn_corrhist, context->pawn_corrhist, sizeof(context->pawn_corrhist));
			memcpy(helper_thread_context[i].nonpawn_corrhist, context->nonpawn_corrhist, sizeof(context->nonpawn_corrhist));

			if (use_nnue) {
				helper_thread_context[i].accumulator_stack = calloc(MAX_PLY + 1, sizeof(accumulator));
				helper_thread_context[i].refresh_table = calloc(1, sizeof(accumulator_refresh_table));
			}

			memcpy(helper_thread_context[i].searched_moves, context->searched_moves, 16 * sizeof(move));
			THREAD_START(helper_threads[i], helper_thread_function, &helper_thread_context[i]);
		}
	}

	uint64_t iteration_nodes[MAX_PLY] = {0};

	if (use_nnue)
		update_accumulator_from_table(board, &context->accumulator_stack[0], context->refresh_table);
	int last_score = 0;
	int consecutive_time_reductions = 0;

	for (int current_depth = 1; current_depth <= max_depth; current_depth++) {
		context->seldepth = 0;
		aspiration_window(context, current_depth);

		//Reduce soft time cap on stable searches.
		//Consider all searches within the first 30% of the search time as naturally unstable.
		clock_t cur_time = 0;

		if (context->start_time) {
			cur_time = clock();
			if (context->hard_end_time - cur_time > 0.3 * (context->hard_end_time - context->start_time)) {
				if (consecutive_time_reductions < 5 && (best_move.raw == context->pv.line[0].raw || abs(context->score - last_score) < 5)) {
					context->soft_end_time -= 0.1 * (context->soft_end_time - cur_time);
					consecutive_time_reductions++;
				} else {
					consecutive_time_reductions = 0;
					context->soft_end_time = context->hard_end_time;
				}
			}
		}

		best_move = context->pv.line[0];
		ponder_move = context->pv.line[1];
		last_score = context->score;

		if (!suppress_uci_messages) {
			int total_nodes = context->counted_nodes;

			total_nodes *= thread_count; //Unreliable, basically a best-guess until end of search.
			int milliseconds = (double) (clock() - context->start_time) / (CLOCKS_PER_SEC / 1000.0);

			milliseconds = _max(milliseconds, 1);
			int nps = total_nodes / (milliseconds / 1000.0);

			printf("info depth %d seldepth %d score ", current_depth, context->seldepth);

			int abs_score = abs(context->score);

			assert(abs_score <= SCORE_WIN);
			if (abs_score >= MIN_MATE_SCORE) {
				int mate_in_plies = SCORE_WIN - abs_score;
				int mate_in_moves = (mate_in_plies + 1) / 2;

				if (context->score < 0)
					mate_in_moves = -mate_in_moves;
				printf("mate %d ", mate_in_moves);
			} else {
				printf("cp %d ", context->score);
			}

			printf("nodes %d nps %d hashfull %d time %d", total_nodes, nps, get_tt_hashfull(context->tt), milliseconds);

			printf(" pv");
			for (int i = 0; i < context->pv.length; i++) {
				move m = context->pv.line[i];

				if (!m.raw)
					break;
				printf(" %s%s", get_move_square_name(m), promote_symbols[m.promote_to]);
			}

			printf("\n");
			fflush(stdout);
		}

		if (!is_ponder && (*context->abort_flag || clock() > context->soft_end_time || context->counted_nodes >= context->soft_max_nodes / thread_count))
			break;

		// Assume that the next depth will take more nodes than the current depth.
		// If counted_nodes + counted_nodes_this_depth > hard_max_nodes, we should be able to avoid having to waste time on a partial search of the next depth.
		// This is done as a speedup for data generation.
		iteration_nodes[current_depth - 1] = (current_depth > 0) ? context->counted_nodes - iteration_nodes[current_depth - 1] : context->counted_nodes;
		if (context->counted_nodes + iteration_nodes[current_depth - 1] > context->hard_max_nodes / thread_count)
			break;

		if (context->start_time && abs(context->score) >= MIN_MATE_SCORE)
			context->soft_end_time -= 0.5 * (context->soft_end_time - cur_time);
	}

	if (helper_thread_count > 0) {
		*context->abort_flag = 1;
		for (int i = 0; i < helper_thread_count; i++) {
			THREAD_WAIT(helper_threads[i]);
			free(helper_thread_context[i].repetitions.hash_codes);
			if (use_nnue) {
				free(helper_thread_context[i].accumulator_stack);
				free(helper_thread_context[i].refresh_table);
			}
		}
		find_best_thread(context, helper_thread_context, &best_move, &ponder_move);

		free(helper_threads);
		free(helper_thread_context);
	}

#ifdef SEARCHINFO
	double ebf = 0.0;
	int count = 0;

	for (int d = 1; d <= context->max_depth; d++) {
		if (iteration_nodes[d - 1] > 0) {
			ebf += (double) iteration_nodes[d] / iteration_nodes[d - 1];
			count++;
		} else
			break;
	}
	ebf /= count;
	double total_tt = context->tt_hits + context->tt_misses;
	char buf[32];

	printf("\n\033[1mSearch Statistics:\033[0m\n");
	printf("\t%-25s %18.2f\n", "EBF:", ebf);
	printf("\n");
	printf("\t%-25s %10s (%05.2f%%)\n", "PVS Nodes:", comma_delimit_number(buf, context->pvs_nodes), (100.0 * context->pvs_nodes) / context->counted_nodes);
	printf("\t%-25s %10s (%05.2f%%)\n", "Quiescent Nodes:", comma_delimit_number(buf, context->qs_nodes), (100.0 * context->qs_nodes) / context->counted_nodes);
	printf("\n");
	printf("\t%-25s %10s\n", "Total Nodes:", comma_delimit_number(buf, context->counted_nodes));
	printf("\n");
	printf("\t%-25s %18.2f\n", "PVS Avg Moves Searched:", (double) context->pvs_searched_moves / context->pvs_searched_positions);
	printf("\t%-25s %18.2f\n", "QS Avg Moves Searched:", (double) context->quiescent_searched_moves / context->quiescent_searched_positions);
	printf("\n");
	printf("\t%-25s %10s (%05.2f%%)\n", "TT Hits:", comma_delimit_number(buf, context->tt_hits), (100.0 * context->tt_hits) / total_tt);
	printf("\t%-25s %10s (%05.2f%%)\n", "TT Misses:", comma_delimit_number(buf, context->tt_misses), (100.0 * context->tt_misses) / total_tt);
	printf("\t%-25s %10s (%05.2f%%)\n", "TT Cutoffs:", comma_delimit_number(buf, context->tt_cutoffs), (100.0 * context->tt_cutoffs) / total_tt);
	printf("\t%-25s %10s\n", "TT Collisions:", comma_delimit_number(buf, context->tt->collisions));
	printf("\t%-25s %10s\n", "Shallow TT Collisions:", comma_delimit_number(buf, context->tt->shallow_collisions));
	printf("\t%-25s %10s\n", "TT Overwrites:", comma_delimit_number(buf, context->tt->overwrites));
	printf("\t%-25s %10s\n", "TT Fills:", comma_delimit_number(buf, context->tt->fills));
	printf("\n");
	printf("\t%-25s %18s\n", "Singular Searches:", comma_delimit_number(buf, context->singular_searches));
	printf("\t%-25s %18s (%05.2f%%)\n", "Single Extensions:", comma_delimit_number(buf, context->single_extensions), (100.0 * context->single_extensions) / context->singular_searches);
	printf("\t%-25s %18s (%05.2f%%)\n", "Double Extensions:", comma_delimit_number(buf, context->double_extensions), (100.0 * context->double_extensions) / context->singular_searches);
	printf("\t%-25s %18s (%05.2f%%)\n", "Triple Extensions:", comma_delimit_number(buf, context->triple_extensions), (100.0 * context->triple_extensions) / context->singular_searches);
	printf("\t%-25s %18s (%05.2f%%)\n", "Multicut Prunes:", comma_delimit_number(buf, context->multicut_prunes), (100.0 * context->multicut_prunes) / context->singular_searches);
	printf("\t%-25s %18s (%05.2f%%)\n", "Negative Extensions:", comma_delimit_number(buf, context->negative_extensions), (100.0 * context->negative_extensions) / context->singular_searches);
	printf("\t%-25s %18s (%05.2f%%)\n", "Failed Singulars:", comma_delimit_number(buf, context->failed_extensions), (100.0 * context->failed_extensions) / context->singular_searches);
	printf("\t%-25s %18s\n", "Extension Overflow:", comma_delimit_number(buf, context->extension_overflow));
	printf("\n");
	printf("\t%-25s %18s\n", "Evaluations:", comma_delimit_number(buf, context->evaluations));
	printf("\n");
	printf("\t%-25s %18s\n", "Stable Reductions:", comma_delimit_number(buf, context->stable_reductions));
	printf("\t%-25s %18s\n", "Worsening Reductions:", comma_delimit_number(buf, context->worsening_reductions));
	printf("\t%-25s %18s\n", "TT Reductions:", comma_delimit_number(buf, context->tt_reductions));
	printf("\t%-25s %18s\n", "RFP Prunes:", comma_delimit_number(buf, context->rfp_prunes));
	printf("\t%-25s %18s\n", "Razoring Prunes:", comma_delimit_number(buf, context->razoring_prunes));
	printf("\t%-25s %18s\n", "Failed Razorings:", comma_delimit_number(buf, context->failed_razoring_prunes));
	printf("\t%-25s %18s\n", "NMP Prunes:", comma_delimit_number(buf, context->nmp_prunes));
	printf("\t%-25s %18s\n", "Failed NMP Search:", comma_delimit_number(buf, context->failed_nmp_prunes));
#endif

	while (is_ponder) {
		WAIT_COND_VAR(ponder_cond_var, ponder_lock);
	}

	print_resulting_moves(best_move, ponder_move);
	is_searching = 0;

	memset(context->searched_moves, 0, sizeof(context->searched_moves));

	return 0;
}
