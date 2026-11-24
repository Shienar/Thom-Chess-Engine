#include "board/moves.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/magic.h"
#include "analyze/search.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

// All "" end up pointing to the same address.
//[0] amd [PIECE] indexes are the only ones that expect usage.
char *promote_symbols[16] = {
	[0] = "",
	[1] = "",
	[KNIGHT] = "n",
	[3] = "",
	[BISHOP] = "b",
	[5] = "",
	[ROOK]   = "r",
	[7] = "",
	[QUEEN]  = "q",
	[9] = "",
	[10] = "",
	[11] = "",
	[12] = "",
	[13] = "",
	[14] = "",
	[15] = "",
};

int generate_move_list(move *moves_list, bitboard *board, int gen_type)
{
	int size = 0;

	int ally_color = board->turn;
	int enemy_color = FLIP_COLOR(board->turn);

	uint64_t allies = board->pieces_side[ally_color];
	uint64_t enemies = board->pieces_side[enemy_color];
	uint64_t knights = board->pieces[KNIGHT | ally_color];
	uint64_t bishop = board->pieces[BISHOP | ally_color];
	uint64_t rook = board->pieces[ROOK | ally_color];
	uint64_t queen = board->pieces[QUEEN | ally_color];
	uint64_t king = board->pieces[KING | ally_color];

	int enemy_king_sq = board->king_square[enemy_color];
	uint64_t king_threats_pawn = pawn_attacks[enemy_color][board->king_square[enemy_color]];
	uint64_t king_threats_knight = knight_attacks[board->king_square[enemy_color]];
	uint64_t king_threats_bishop = bishop_moves(enemies, allies, enemy_king_sq);
	uint64_t king_threats_rook = rook_moves(enemies, allies, enemy_king_sq);

	uint64_t king_threats_queen = king_threats_bishop | king_threats_rook;

	uint64_t mask = 0;
	int piece = PAWN|(board->turn);

	if (ISWHITE(piece)) {
		mask = WHITE_PAWN_DOUBLEPUSH_MASK(board);
		if (gen_type == GET_NOISY_MOVES)
			mask &= king_threats_pawn;
		else if (gen_type == GET_QUIET_MOVES)
			mask &= ~king_threats_pawn;
		while (mask) {
			int end_square = __builtin_ctzll(mask);

			create_compact_move(&moves_list[size++], end_square - 16, end_square, 0);
			mask &= (mask - 1);
		}

		mask = WHITE_PAWN_PUSH_MASK(board);
		if (gen_type == GET_NOISY_MOVES)
			mask &= (king_threats_pawn | board_rank[7]);
		else if (gen_type == GET_QUIET_MOVES)
			mask &= ~(king_threats_pawn | board_rank[7]);
		while (mask) {
			int end_square = __builtin_ctzll(mask);
			int start_square = end_square - 8;

			if (end_square >= 56) {
				//Promotion
				create_compact_move(&moves_list[size++], start_square, end_square, KNIGHT);
				create_compact_move(&moves_list[size++], start_square, end_square, BISHOP);
				create_compact_move(&moves_list[size++], start_square, end_square, ROOK);
				create_compact_move(&moves_list[size++], start_square, end_square, QUEEN);
			} else {
				create_compact_move(&moves_list[size++], start_square, end_square, 0);
			}

			mask &= (mask - 1);
		}

		if (gen_type != GET_QUIET_MOVES) {
			mask = WHITE_PAWN_LEFTATTACKS(board);
			while (mask) {
				int end_square = __builtin_ctzll(mask);
				int start_square = end_square - 7;

				if (end_square >= 56) {
					//Promotion
					create_compact_move(&moves_list[size++], start_square, end_square, KNIGHT);
					create_compact_move(&moves_list[size++], start_square, end_square, BISHOP);
					create_compact_move(&moves_list[size++], start_square, end_square, ROOK);
					create_compact_move(&moves_list[size++], start_square, end_square, QUEEN);
				} else {
					create_compact_move(&moves_list[size++], start_square, end_square, 0);
				}

				mask &= (mask - 1);
			}

			mask = WHITE_PAWN_RIGHTATTACKS(board);
			while (mask) {
				int end_square = __builtin_ctzll(mask);
				int start_square = end_square - 9;

				if (end_square >= 56) {
					//Promotion
					create_compact_move(&moves_list[size++], start_square, end_square, KNIGHT);
					create_compact_move(&moves_list[size++], start_square, end_square, BISHOP);
					create_compact_move(&moves_list[size++], start_square, end_square, ROOK);
					create_compact_move(&moves_list[size++], start_square, end_square, QUEEN);
				} else {
					create_compact_move(&moves_list[size++], start_square, end_square, 0);
				}

				mask &= (mask - 1);
			}


			if (board->ep_square != NO_EP_SQUARE) {
				mask = EN_PASSANT_ATTACKERS_WHITE((1ull << (board->ep_square)), board);
				while (mask) {
					int start_square = __builtin_ctzll(mask);

					create_compact_move(&moves_list[size++], start_square, board->ep_square, 0);
					mask &= (mask - 1);
				}
			}
		}
	} else {
		mask = BLACK_PAWN_DOUBLEPUSH_MASK(board);
		if (gen_type == GET_NOISY_MOVES)
			mask &= king_threats_pawn;
		else if (gen_type == GET_QUIET_MOVES)
			mask &= ~king_threats_pawn;
		while (mask) {
			int end_square = __builtin_ctzll(mask);

			create_compact_move(&moves_list[size++], end_square + 16, end_square, 0);

			mask &= (mask - 1);
		}

		mask = BLACK_PAWN_PUSH_MASK(board);
		if (gen_type == GET_NOISY_MOVES)
			mask &= (king_threats_pawn | board_rank[0]);
		else if (gen_type == GET_QUIET_MOVES)
			mask &= ~(king_threats_pawn | board_rank[0]);
		while (mask) {
			int end_square = __builtin_ctzll(mask);
			int start_square = end_square + 8;

			if (end_square <= 7) {
				//Promotion
				create_compact_move(&moves_list[size++], start_square, end_square, KNIGHT);
				create_compact_move(&moves_list[size++], start_square, end_square, BISHOP);
				create_compact_move(&moves_list[size++], start_square, end_square, ROOK);
				create_compact_move(&moves_list[size++], start_square, end_square, QUEEN);
			} else {
				create_compact_move(&moves_list[size++], start_square, end_square, 0);
			}

			mask &= (mask - 1);
		}

		if (gen_type != GET_QUIET_MOVES) {
			mask = BLACK_PAWN_LEFTATTACKS(board);
			while (mask) {
				int end_square = __builtin_ctzll(mask);
				int start_square = end_square + 7;

				if (end_square <= 7) {
					//Promotion
					create_compact_move(&moves_list[size++], start_square, end_square, KNIGHT);
					create_compact_move(&moves_list[size++], start_square, end_square, BISHOP);
					create_compact_move(&moves_list[size++], start_square, end_square, ROOK);
					create_compact_move(&moves_list[size++], start_square, end_square, QUEEN);
				} else {
					create_compact_move(&moves_list[size++], start_square, end_square, 0);
				}

				mask &= (mask - 1);
			}

			mask = BLACK_PAWN_RIGHTATTACKS(board);
			while (mask) {
				int end_square = __builtin_ctzll(mask);
				int start_square = end_square + 9;

				if (end_square <= 7) {
					//Promotion
					create_compact_move(&moves_list[size++], start_square, end_square, KNIGHT);
					create_compact_move(&moves_list[size++], start_square, end_square, BISHOP);
					create_compact_move(&moves_list[size++], start_square, end_square, ROOK);
					create_compact_move(&moves_list[size++], start_square, end_square, QUEEN);
				} else {
					create_compact_move(&moves_list[size++], start_square, end_square, 0);
				}

				mask &= (mask - 1);
			}


			if (board->ep_square != NO_EP_SQUARE) {
				uint64_t ep_mask = (1ull << (board->ep_square));

				mask = EN_PASSANT_ATTACKERS_BLACK(ep_mask, board);
				while (mask) {
					int start_square = __builtin_ctzll(mask);

					create_compact_move(&moves_list[size++], start_square, board->ep_square, 0);
					mask &= (mask - 1);
				}
			}
		}
	}

	while (knights) {
		int start_square = __builtin_ctzll(knights);
		uint64_t mask = knight_moves(allies, start_square);

		if (gen_type == GET_NOISY_MOVES)
			mask &= (enemies | king_threats_knight);
		else if (gen_type == GET_QUIET_MOVES)
			mask &= ~(enemies | king_threats_knight);

		while (mask) {
			int end_square = __builtin_ctzll(mask);

			create_compact_move(&moves_list[size++], start_square, end_square, 0);
			mask &= (mask-1);
		}
		knights &= (knights-1);
	}

	while (bishop) {
		int start_square = __builtin_ctzll(bishop);
		uint64_t move_mask = bishop_moves(allies, enemies, start_square);

		if (gen_type == GET_NOISY_MOVES)
			move_mask &= (enemies | king_threats_bishop);
		else if (gen_type == GET_QUIET_MOVES)
			move_mask &= ~(enemies | king_threats_bishop);

		while (move_mask) {
			int end_square = __builtin_ctzll(move_mask);

			create_compact_move(&moves_list[size++], start_square, end_square, 0);
			move_mask &= (move_mask-1);
		}
		bishop &= (bishop-1);
	}

	while (rook) {
		int start_square = __builtin_ctzll(rook);
		uint64_t move_mask = rook_moves(allies, enemies, start_square);

		if (gen_type == GET_NOISY_MOVES)
			move_mask &= (enemies | king_threats_rook);
		else if (gen_type == GET_QUIET_MOVES)
			move_mask &= ~(enemies | king_threats_rook);

		while (move_mask) {
			int end_square = __builtin_ctzll(move_mask);

			create_compact_move(&moves_list[size++], start_square, end_square, 0);
			move_mask &= (move_mask-1);
		}
		rook &= (rook-1);
	}

	while (queen) {
		int start_square = __builtin_ctzll(queen);
		uint64_t move_mask = queen_moves(allies, enemies, start_square);

		if (gen_type == GET_NOISY_MOVES)
			move_mask &= (enemies | king_threats_queen);
		else if (gen_type == GET_QUIET_MOVES)
			move_mask &= ~(enemies | king_threats_queen);

		while (move_mask) {
			int end_square = __builtin_ctzll(move_mask);

			create_compact_move(&moves_list[size++], start_square, end_square, 0);
			move_mask &= (move_mask-1);
		}
		queen &= (queen-1);
	}

	if (king) {
		int start_square = __builtin_ctzll(king);
		uint64_t move_mask = king_moves(board, start_square, board->turn);

		if (gen_type == GET_NOISY_MOVES)
			move_mask &= enemies;
		else if (gen_type == GET_QUIET_MOVES)
			move_mask &= ~enemies;

		while (move_mask) {
			int end_square = __builtin_ctzll(move_mask);

			create_compact_move(&moves_list[size++], start_square, end_square, 0);
			move_mask &= (move_mask-1);
		}
	}

	return size;
}

const int piece_values_see[15] = {100, 100, 300, 300, 300, 300, 500, 500, 900, 900, 1e6, 1e6, 0, 0, 0};
int static_exchange_evaluation(bitboard *board, move m)
{

	int piece = (board->piece_arr[m.start_square]);
	int captured_piece = (board->piece_arr[m.end_square]);

	if (captured_piece == EMPTY_PIECE && ISPAWN(piece) && m.end_square == board->ep_square)
		captured_piece = FLIP_COLOR(piece);

	int side = COLOR(piece);

	//Handle the initial capture
	int gain = piece_values_see[captured_piece];

	if (m.promote_to) {
		piece = m.promote_to;
		gain += piece_values_see[m.promote_to] - piece_values_see[PAWN];
	}

	uint64_t occupied = board->pieces_all & ~(1ull << (m.start_square));
	uint64_t attackers = (pawn_attacks[WHITE][m.end_square] & board->pieces[BLACK_PAWN]) |
						 (pawn_attacks[BLACK][m.end_square] & board->pieces[WHITE_PAWN]) |
						 (knight_moves(0, m.end_square) & (board->pieces[WHITE_KNIGHT] | board->pieces[BLACK_KNIGHT])) |
						 (king_attacks[m.end_square] & (board->pieces[WHITE_KING] | board->pieces[BLACK_KING]));

	uint64_t bishopqueen = board->pieces[WHITE_BISHOP] | board->pieces[BLACK_BISHOP] | board->pieces[WHITE_QUEEN] | board->pieces[BLACK_QUEEN];
	uint64_t rookqueen = board->pieces[WHITE_ROOK] | board->pieces[BLACK_ROOK] | board->pieces[WHITE_QUEEN] | board->pieces[BLACK_QUEEN];

	attackers |= (bishop_moves(0, occupied, m.end_square) & bishopqueen) | (rook_moves(0, occupied, m.end_square) & rookqueen);
	attackers &= occupied;

	side = FLIP_COLOR(side);

	int attacker_square = 0;
	int attacker_piece;

	captured_piece = piece;

	while (1) {
		//side-relative gain
		gain = -gain;

		//side runs out of attackers
		if (!(board->pieces_side[side] & attackers))
			return side == board->turn ? gain : -gain;

		for (int pc = side; pc < PIECE_COUNT; pc += 2) {
			uint64_t attacking_pieces_of_type = attackers & board->pieces[pc];

			if (attacking_pieces_of_type) {
				attacker_piece = pc;
				attacker_square = __builtin_ctzll(attacking_pieces_of_type);
				break;
			}
		}

		gain += piece_values_see[captured_piece];
		captured_piece = attacker_piece;

		//side fails to find a capture that maintains a positive SEE.
		if (gain < 0)
			return (side == board->turn) ? gain : -gain;

		occupied &= ~(1ull << (attacker_square));
		attackers |= (bishop_moves(0, occupied, m.end_square) & bishopqueen) | (rook_moves(0, occupied, m.end_square) & rookqueen);
		attackers &= occupied;

		side = FLIP_COLOR(side);
	}
}

void score_moves(move_iterator *iter, int start_index, int count)
{
	bitboard *board = iter->board;
	thread_context *context = iter->context;

	for (int i = start_index; i < start_index + count; i++) {
		int current_piece = (board->piece_arr[iter->move_list[i].start_square]);
		int captured_piece = (board->piece_arr[iter->move_list[i].end_square]);
		int is_ep = 0;

		if (captured_piece == EMPTY_PIECE && ISPAWN(current_piece) && iter->move_list[i].end_square == board->ep_square) {
			captured_piece = FLIP_COLOR(current_piece);
			is_ep = 1;
		}
		int is_capture = captured_piece != EMPTY_PIECE || is_ep;

		if (iter->move_list[i].raw == iter->excluded_move.raw ||
			iter->move_list[i].raw == iter->tt_move.raw ||
			iter->move_list[i].raw == iter->killer1_move.raw ||
			iter->move_list[i].raw == iter->killer2_move.raw)
				iter->move_scores[i] = INT16_MIN;
		else if (is_capture || iter->move_list[i].promote_to) {
			int see_value = static_exchange_evaluation(board, iter->move_list[i]);
			int history_bonus = get_capture_history_offset(context, current_piece, iter->move_list[i].end_square, captured_piece);

			iter->move_scores[i] = ((see_value >= 0) ? 10000 : -10000) + see_value + history_bonus;
		} else {
			iter->move_scores[i] = get_history_value(context, board->turn, current_piece, iter->move_list[i].end_square);

			if (iter->countermove.raw && iter->move_list[i].raw == iter->countermove.raw)
				iter->move_scores[i] = _min(iter->move_scores[i] + COUNTERMOVE_BONUS, MAX_HISTORY_SCORE + 2);
			else if (iter->followup_move.raw && iter->move_list[i].raw == iter->followup_move.raw)
				iter->move_scores[i] = _min(iter->move_scores[i] + FOLLOWUPMOVE_BONUS, MAX_HISTORY_SCORE + 1);
		}
	}
}

int16_t select_next_best_move(move_iterator *iter, uint8_t start_index, uint8_t *searched_count, uint8_t max_count)
{
	if (*searched_count >= max_count)
		return -1;

	int best_index = -1;
	int max_score_remaining = INT16_MIN;

	best_index = -1;
	max_score_remaining = INT16_MIN;

	//Select highest remaining move from those not chosen yet.
	for (uint8_t j = start_index + *searched_count; j < max_count; j++) {
		if (iter->move_scores[j] > max_score_remaining) {
			best_index = j;
			max_score_remaining = iter->move_scores[j];
		}
	}

	if (best_index == -1)
		return -1;

	//Move it to the front of the list where it can be skipped over in subsequent iterations.
	if (best_index > start_index + *searched_count) {
		int16_t temp_score = iter->move_scores[best_index];

		iter->move_scores[best_index] = iter->move_scores[start_index + *searched_count];
		iter->move_scores[start_index + *searched_count] = temp_score;

		move temp = iter->move_list[start_index + *searched_count];

		iter->move_list[start_index + *searched_count] = iter->move_list[best_index];
		iter->move_list[best_index] = temp;
	}


	(*searched_count)++;
	return start_index + *searched_count - 1;
}

move_iterator *create_move_iterator(thread_context *context, int gen_type, int ply, move *tt_move)
{
	move_iterator *iter = calloc(1, sizeof(move_iterator));

	iter->type = gen_type;
	iter->stage = 0;
	iter->board = &context->board_stack[ply];
	iter->context = context;

	if (ply == 0 && context->searched_moves[0].raw) {
		memcpy(iter->move_list, context->searched_moves, MAX_REQUIRED_MOVES * sizeof(move));
		while (context->searched_moves[iter->count].raw)
			iter->count++;
		score_moves(iter, 0, iter->count);
		iter->stage = ITER_STAGE_REQUIRED;
	} else {
		iter->tt_move.raw = tt_move ? tt_move->raw : 0;

		iter->killer1_move = context->killer_moves[ply][0];
		iter->killer2_move = context->killer_moves[ply][1];

		iter->countermove  = get_counter_move(context, ply);
		iter->followup_move = get_followup_move(context, ply);

		iter->excluded_move.raw = context->excluded_move[ply].raw;
	}
	return iter;
}

int is_move_usable(bitboard *board, move m)
{
	uint64_t allies = board->pieces_side[board->turn];
	uint64_t enemies = board->pieces_side[FLIP_COLOR(board->turn)];

	uint8_t piece = (board->piece_arr[m.start_square]);

	if (piece == EMPTY_PIECE || COLOR(piece) != board->turn)
		return 0;

	uint64_t dest_mask = (1ull << (m.end_square));

	switch (PIECE(piece)) {
	case PAWN:
		//Captures (Including en passant)
		if ((dest_mask & pawn_attacks[COLOR(piece)][m.start_square])) {
			if (m.end_square == board->ep_square)
				return 1;

			int captured_piece = (board->piece_arr[m.end_square]);

			if (captured_piece == EMPTY_PIECE || COLOR(captured_piece) == COLOR(piece))
				return 0;
			else
				return 1;
		}

		//Can't use the original global board push mask generations, else moves tt moves like a2->c4 might
		//get interpreted as valid pawn moves.
		if (ISWHITE(piece)) {
			//Single pushes
			uint64_t single_push_mask = ((1ull << (m.start_square)) << 8)&(~board->pieces_all);

			if (dest_mask & single_push_mask)
				return 1;

			//Double pushes
			if (get_row(m.start_square) == 1 && (dest_mask & ((single_push_mask << 8)&(~board->pieces_all))))
				return 1;

			return 0;
		} else {
			//Single pushes
			uint64_t single_push_mask = ((1ull << (m.start_square)) >> 8)&(~board->pieces_all);

			if (dest_mask & single_push_mask)
				return 1;

			//Double pushes
			if (get_row(m.start_square) == 6 && (dest_mask & ((single_push_mask >> 8)&(~board->pieces_all))))
				return 1;

			return 0;
		}
		break;
	case KNIGHT:
		return (knight_moves(allies, m.start_square) & dest_mask) != 0;
	case BISHOP:
		return (bishop_moves(allies, enemies, m.start_square) & dest_mask) != 0;
	case ROOK:
		return (rook_moves(allies, enemies, m.start_square) & dest_mask) != 0;
	case QUEEN:
		return (queen_moves(allies, enemies, m.start_square) & dest_mask) != 0;
	case KING:
		return (king_moves(board, m.start_square, board->turn) & dest_mask) != 0;
	default:
		__builtin_unreachable();
		break;
	}

	return 0;
}

move *iterate_next_move(move_iterator *iter)
{
	move *returned_move = NULL;

	switch (iter->stage) {
	case ITER_STAGE_TT:
		if (iter->type == GET_QUIET_MOVES)
			iter->stage = ITER_STAGE_KILLER_1;
		else
			iter->stage = ITER_STAGE_GET_NOISY;

		if (iter->tt_move.raw && is_move_usable(iter->board, iter->tt_move)) {
			int current_piece = (iter->board->piece_arr[iter->tt_move.start_square]);
			int captured_piece = (iter->board->piece_arr[iter->tt_move.end_square]);
			int is_ep = 0;

			if (captured_piece == EMPTY_PIECE && ISPAWN(current_piece) && iter->tt_move.end_square == iter->board->ep_square) {
				captured_piece = FLIP_COLOR(current_piece);
				is_ep = 1;
			}
			int is_capture = captured_piece != EMPTY_PIECE || is_ep;

			if (iter->type == GET_ALL_MOVES || (iter->type == GET_NOISY_MOVES && is_capture) || (iter->type == GET_QUIET_MOVES && !is_capture)) {
				returned_move = &iter->tt_move;
				break;
			}
		} else if (iter->type == GET_QUIET_MOVES)
			goto quiet_start;
	case ITER_STAGE_GET_NOISY:
		iter->count = generate_move_list(iter->move_list, iter->board, GET_NOISY_MOVES);
		score_moves(iter, 0, iter->count);

		iter->stage = ITER_STAGE_GOOD_NOISY;
	case ITER_STAGE_GOOD_NOISY:
		iter->selected_index = select_next_best_move(iter, 0, &iter->visited_good, iter->count);
		if (iter->selected_index != -1) {
			if (iter->move_scores[iter->selected_index] >= 0) {
				returned_move = &iter->move_list[iter->selected_index];
				break;
			}

			iter->bad_capture_index = iter->selected_index;
			iter->bad_capture_count = iter->count;
		}

		if (iter->type == GET_NOISY_MOVES) {
			iter->stage = ITER_STAGE_BAD_NOISY;
			goto bad_noisy;
		} else
			iter->stage = ITER_STAGE_KILLER_1;

	case ITER_STAGE_KILLER_1:
quiet_start:
		iter->stage = ITER_STAGE_KILLER_2;
		if (iter->killer1_move.raw && iter->killer1_move.raw != iter->tt_move.raw && is_move_usable(iter->board, iter->killer1_move)) {
			returned_move = &iter->killer1_move;
			break;
		}
	case ITER_STAGE_KILLER_2:
		iter->stage = ITER_STAGE_GET_QUIETS;
		if (iter->killer2_move.raw && iter->killer2_move.raw != iter->tt_move.raw && is_move_usable(iter->board, iter->killer2_move)) {
			returned_move = &iter->killer2_move;
			break;
		}
	case ITER_STAGE_GET_QUIETS:
		iter->quiet_index = iter->count;
		uint8_t quiet_count = generate_move_list(&iter->move_list[iter->quiet_index], iter->board, GET_QUIET_MOVES);

		iter->count += quiet_count;
		score_moves(iter, iter->quiet_index, quiet_count);

		iter->stage = ITER_STAGE_QUIETS;
	case ITER_STAGE_QUIETS:
		iter->selected_index = select_next_best_move(iter, iter->quiet_index, &iter->visited_quiet, iter->count);
		if (iter->selected_index != -1) {
			returned_move = &iter->move_list[iter->selected_index];
			break;
		}
		if (iter->type == GET_QUIET_MOVES) {
			iter->stage = ITER_STAGE_DONE;
			break;
		}

		iter->stage = ITER_STAGE_BAD_NOISY;
	case ITER_STAGE_BAD_NOISY:
bad_noisy:
		iter->selected_index = select_next_best_move(iter, iter->bad_capture_index, &iter->visited_bad, iter->bad_capture_count);
		if (iter->selected_index != -1) {
			returned_move = &iter->move_list[iter->selected_index];
			break;
		}

		iter->stage = ITER_STAGE_DONE;
	case ITER_STAGE_DONE:
		break;
	case ITER_STAGE_REQUIRED:
		iter->selected_index = select_next_best_move(iter, 0, &iter->visited_good, iter->count);
		if (iter->selected_index != -1) {
			returned_move = &iter->move_list[iter->selected_index];
			break;
		}

		iter->stage = ITER_STAGE_DONE;
		break;
	default:
		__builtin_unreachable();
		break;
	}
	return returned_move;
}

int is_threatened(bitboard *board, int square, int defending_color)
{
	//Silently fail (instead of segfault) on corrupted binpack games.
	//Negligable impact on perft
	if (square < 0 || square > 63)
		return 0;

	int attacking_color = FLIP_COLOR(defending_color);

	if (pawn_attacks[defending_color][square] & board->pieces[PAWN | attacking_color])
		return 1;
	if (knight_attacks[square] & board->pieces[KNIGHT | attacking_color])
		return 1;
	if (king_attacks[square] & board->pieces[KING | attacking_color])
		return 1;

	uint64_t bishopqueen = board->pieces[BISHOP | attacking_color] | board->pieces[QUEEN | attacking_color];
	uint64_t rookqueen = board->pieces[ROOK | attacking_color] | board->pieces[QUEEN | attacking_color];

	uint64_t enemy_pieces = board->pieces_side[attacking_color];
	uint64_t ally_pieces = board->pieces_side[defending_color];

	if (bishop_moves(ally_pieces, enemy_pieces, square)&(bishopqueen))
		return 1;
	if (rook_moves(ally_pieces, enemy_pieces, square)&(rookqueen))
		return 1;

	return 0;
}

uint64_t pyrrhic_pawn_attacks(int square, int color)
{
	return pawn_attacks[FLIP_COLOR(color)][square];
}

uint64_t knight_moves(uint64_t ally_pieces, int square)
{
	return knight_attacks[square]&(~ally_pieces);
}

uint64_t bishop_moves(uint64_t ally_pieces, uint64_t enemy_pieces, int square)
{
	uint64_t occupancy = (ally_pieces|enemy_pieces) & bishop_magics[square].mask;
	return bishop_magics[square].attacks[(occupancy * bishop_magics[square].magic) >> bishop_magics[square].shift_offset] & (~ally_pieces);
}

uint64_t rook_moves(uint64_t ally_pieces, uint64_t enemy_pieces, int square)
{
	uint64_t occupancy = (ally_pieces|enemy_pieces) & rook_magics[square].mask;
	return rook_magics[square].attacks[(occupancy * rook_magics[square].magic) >> rook_magics[square].shift_offset] & (~ally_pieces);
}

uint64_t queen_moves(uint64_t ally_pieces, uint64_t enemy_pieces, int square)
{
	return rook_moves(ally_pieces, enemy_pieces, square)|bishop_moves(ally_pieces, enemy_pieces, square);
}

uint64_t pyrrhic_king_attacks(int square)
{
	return king_attacks[square];
}

uint64_t king_moves(bitboard *board, int square, int color)
{
	uint64_t returned_value = king_attacks[square];

	if (ISWHITE(color)) {
		returned_value &= (~board->pieces_side[WHITE]);

		if (board->can_kingside_castle_w && !board->in_check && !(board->pieces_all&0x60))
			returned_value |= 0x40;
		if (board->can_queenside_castle_w && !board->in_check && !(board->pieces_all&0xE))
			returned_value |= 0x4;

	} else {
		returned_value &= (~board->pieces_side[BLACK]);

		if (board->can_kingside_castle_b && !board->in_check && !(board->pieces_all&0x6000000000000000))
			returned_value |= 0x4000000000000000;
		if (board->can_queenside_castle_b && !board->in_check && !(board->pieces_all&0x0E00000000000000))
			returned_value |= 0x0400000000000000;
	}

	return returned_value;
}

int move_piece(bitboard *board, move compact_move, repetition_vector *repetitions)
{
	assert(board);
	assert(compact_move.start_square >= 0 && compact_move.start_square <= 63 && compact_move.end_square >= 0 && compact_move.end_square <= 63);

	//Clear the en passant hash early. clear the en passant square later.
	//En passant square is required for legal moves, en passant hash can change by the end of the function.
	board->hash_code ^= get_ep_hash(board);

	int piece = (board->piece_arr[compact_move.start_square]);
	int captured_piece = (board->piece_arr[compact_move.end_square]);

	switch (PIECE(piece)) {
	case PAWN:
		int difference = abs(compact_move.start_square - compact_move.end_square);

		if (difference == 8 || difference == 16) {
			if ((ISWHITE(piece) && compact_move.end_square > 55) || (ISBLACK(piece) && compact_move.end_square < 8)) {
				//Promotion
				if (compact_move.promote_to == 0)
					compact_move.promote_to = QUEEN;
				board_clear_square(board, compact_move.start_square);
				board_set(board, compact_move.end_square, (COLOR(piece)|compact_move.promote_to));
			} else {
				board_move_piece_quietly(board, compact_move.start_square, compact_move.end_square);
			}

			//Update en passant square if necessary.
			if (difference == 16) {
				if (ISWHITE(piece)) {
					board->ep_square = compact_move.end_square - 8;
					board->hash_code ^= get_ep_hash(board);
				} else {
					board->ep_square = compact_move.end_square + 8;
					board->hash_code ^= get_ep_hash(board);
				}
			}
		} else if (difference == 7 || difference == 9) {
			//Diagonal Capture

			//Check for en passant.
			if (compact_move.end_square == board->ep_square) {
				captured_piece = PAWN | (FLIP_COLOR(board->turn));
				if (ISWHITE(piece))
					board_clear_square(board, board->ep_square - 8);
				else
					board_clear_square(board, board->ep_square + 8);
			}

			if ((ISWHITE(piece) && compact_move.end_square > 55) || (ISBLACK(piece) && compact_move.end_square < 8)) {
				//Promotion
				if (compact_move.promote_to == 0)
					compact_move.promote_to = QUEEN;
				board_clear_square(board, compact_move.start_square);
				board_clear_square(board, compact_move.end_square);
				board_set(board, compact_move.end_square, (COLOR(piece)|compact_move.promote_to));
			} else {
				board_move_piece(board, compact_move.start_square, compact_move.end_square);
			}
		}
		break;
	case ROOK:
		if (compact_move.start_square == 7 && board->can_kingside_castle_w) {
			board->can_kingside_castle_w = 0;
			board->hash_code ^= zobrist_keys[768];
		} else if (compact_move.start_square == 0 && board->can_queenside_castle_w) {
			board->can_queenside_castle_w = 0;
			board->hash_code ^= zobrist_keys[769];
		} else if (compact_move.start_square == 0 && board->can_queenside_castle_w) {
			board->can_queenside_castle_w = 0;
			board->hash_code ^= zobrist_keys[769];
		} else if (compact_move.start_square == 63 && board->can_kingside_castle_b) {
			board->can_kingside_castle_b = 0;
			board->hash_code ^= zobrist_keys[770];
		} else if (compact_move.start_square == 63 && board->can_kingside_castle_b) {
			board->can_kingside_castle_b = 0;
			board->hash_code ^= zobrist_keys[770];
		} else if (compact_move.start_square == 56 && board->can_queenside_castle_b) {
			board->can_queenside_castle_b = 0;
			board->hash_code ^= zobrist_keys[771];
		}
	case KNIGHT:
	case BISHOP:
	case QUEEN:
		board_move_piece(board, compact_move.start_square, compact_move.end_square);
		break;
	case KING:
		//Attempting to castle through a threatened sqare.
		if ((compact_move.end_square - compact_move.start_square ==  2 && is_threatened(board, compact_move.start_square + 1, board->turn)) || //Kingside
			(compact_move.end_square - compact_move.start_square == -2 && is_threatened(board, compact_move.start_square - 1, board->turn)))   //Queenside
				return -1;

		if (ISBLACK(piece)) {
			board->king_square[BLACK] = compact_move.end_square;
			if (board->can_kingside_castle_b) {
				board->can_kingside_castle_b = 0;
				board->hash_code ^= zobrist_keys[770];
			}
			if (board->can_queenside_castle_b) {
				board->can_queenside_castle_b = 0;
				board->hash_code ^= zobrist_keys[771];
			}
		} else {
			board->king_square[WHITE] = compact_move.end_square;
			if (board->can_kingside_castle_w) {
				board->can_kingside_castle_w = 0;
				board->hash_code ^= zobrist_keys[768];
			}
			if (board->can_queenside_castle_w) {
				board->can_queenside_castle_w = 0;
				board->hash_code ^= zobrist_keys[769];
			}
		}
		board_move_piece(board, compact_move.start_square, compact_move.end_square);

		//Castling
		if (compact_move.start_square - compact_move.end_square == 2)
			board_move_piece_quietly(board, compact_move.start_square - 4, compact_move.end_square + 1);
		else if (compact_move.start_square - compact_move.end_square == -2)
			board_move_piece_quietly(board, compact_move.start_square + 3, compact_move.end_square - 1);
		break;
	default:
		DEBUG_ERROR("Attempted to move invalid piece type.");
		return -1;
	}

	if (ISROOK(captured_piece)) {
		if (compact_move.end_square == 7 && board->can_kingside_castle_w) {
			board->can_kingside_castle_w = 0;
			board->hash_code ^= zobrist_keys[768];
		} else if (compact_move.end_square == 0 && board->can_queenside_castle_w) {
			board->can_queenside_castle_w = 0;
			board->hash_code ^= zobrist_keys[769];
		} else if (compact_move.end_square == 0 && board->can_queenside_castle_w) {
			board->can_queenside_castle_w = 0;
			board->hash_code ^= zobrist_keys[769];
		} else if (compact_move.end_square == 63 && board->can_kingside_castle_b) {
			board->can_kingside_castle_b = 0;
			board->hash_code ^= zobrist_keys[770];
		} else if (compact_move.end_square == 63 && board->can_kingside_castle_b) {
			board->can_kingside_castle_b = 0;
			board->hash_code ^= zobrist_keys[770];
		} else if (compact_move.end_square == 56 && board->can_queenside_castle_b) {
			board->can_queenside_castle_b = 0;
			board->hash_code ^= zobrist_keys[771];
		}
	}

	if (!(ISPAWN(piece) && abs(compact_move.start_square - compact_move.end_square) == 16) && board->ep_square != NO_EP_SQUARE)
		board->ep_square = NO_EP_SQUARE;

	//50 move rule counting
	if (ISPAWN(piece) || captured_piece != EMPTY_PIECE)
		board->halfmove_clock = 0;
	else
		board->halfmove_clock++;

	board->half_move_count++;

	board->turn ^= 1;
	board->hash_code ^= zobrist_keys[780];
	board->in_check = is_threatened(board, board->king_square[board->turn], board->turn) ? 1 : 0;
	board->plies_from_null++;

	//3-fold
	if (repetitions) {
		if (!repetitions->hash_codes) {
			repetitions->capacity = 16;
			while (repetitions->capacity <= board->half_move_count)
				repetitions->capacity *= 2;
			repetitions->hash_codes = calloc(repetitions->capacity, sizeof(uint64_t));
		}

		//This supports going up/down the array using half_move_count as a guide.
		//fen may create an offset that wastes the first few indices.
		while (repetitions->capacity <= board->half_move_count) {
			repetitions->capacity *= 2;
			repetitions->hash_codes = realloc(repetitions->hash_codes, repetitions->capacity * sizeof(uint64_t));
		}
		repetitions->hash_codes[board->half_move_count] = board->hash_code;
	}

	return 0;
}

void apply_null_move(bitboard *board, bitboard *new_board, repetition_vector *repetitions)
{
	assert(board);
	assert(new_board);
	memcpy(new_board, board, sizeof(bitboard));

	board->plies_from_null = 0;

	new_board->turn = board->turn ^ 1;
	new_board->hash_code = board->hash_code ^ zobrist_keys[780];
	new_board->half_move_count++;

	new_board->hash_code ^= get_ep_hash(board);
	new_board->ep_square = NO_EP_SQUARE;

	while (repetitions->capacity <= board->half_move_count) {
		repetitions->capacity *= 2;
		repetitions->hash_codes = realloc(repetitions->hash_codes, repetitions->capacity * sizeof(uint64_t));
	}
	repetitions->hash_codes[board->half_move_count] = board->hash_code;


}

move get_struct_from_string(bitboard *board, char *str)
{
	//Special case - null move
	if (str[0] == '0')
		return (move){0};

	//String format: [2 char: startsquare][2 char: endsquare][1 char: promotion (q, n, r, b)]
	char start[3] = {'\0'};

	strncpy(start, str, 2);
	char end[3] = {'\0'};

	strncpy(end, str + 2, 2);
	char promotion = str[4];

	int start_square = get_square_number(start);
	int end_square = get_square_number(end);
	int piece = (board->piece_arr[start_square]);

	if (piece == EMPTY_PIECE) {
		DEBUG_ERROR("Could not find piece on start square.");
		return (move){0};
	}

	int promote_to = 0;

	switch (promotion) {
	case 'q':
		promote_to = QUEEN;
		break;
	case 'r':
		promote_to = ROOK;
		break;
	case 'n':
		promote_to = KNIGHT;
		break;
	case 'b':
		promote_to = BISHOP;
		break;
	default:
		break;
	}

	move m = {0};

	create_compact_move(&m, start_square, end_square, promote_to);

	move move_list[MAX_MOVES];
	int count = generate_move_list(move_list, board, GET_ALL_MOVES);
	int is_potential_move = 0;

	for (int index = 0; index < count; index++) {
		if (m.start_square == move_list[index].start_square && m.end_square == move_list[index].end_square) {
			is_potential_move = 1;
			break;
		}
	}

	if (!is_potential_move) {
		printf("Piece move is not legal.\n");
		return (move){0};
	}

	return m;
}

int move_from_struct(bitboard *board, bitboard *new_board, move m, repetition_vector *repetitions)
{
	assert(board);
	assert(new_board);

	if (board != new_board)
		memcpy(new_board, board, sizeof(bitboard));

	if (!m.raw || move_piece(new_board, m, repetitions) != 0) {
		DEBUG_ERROR("Failed to move piece from struct.");
		return -1;
	}

	//Psuedo-legal move generator created an illegal move. Fail silently.
	if (is_threatened(new_board, new_board->king_square[FLIP_COLOR(new_board->turn)], FLIP_COLOR(new_board->turn)))
		return -1;

	#ifdef VERIFY
		bitboard temp;

		memcpy(&temp, new_board, sizeof(bitboard));
		generate_hash_code(&temp);
		if (new_board->hash_code != temp.hash_code) {
			uint64_t xor = temp.hash_code ^ new_board->hash_code;

			printf("Error! board code 0x%016" PRIx64 " != expected 0x%016" PRIx64 " (XOR = 0x%016" PRIx64 ")\n", board->hash_code, temp.hash_code, xor);
			for (int i = 780; i >= 0; i--) {
				if (zobrist_keys[i] == xor) {
					printf("\tXOR = zobrist_keys[%d]\n", i);
					break;
				}
			}
			assert(0);
		} else if (new_board->pawn_hash != temp.pawn_hash) {
			uint64_t xor = temp.pawn_hash ^ new_board->pawn_hash;

			printf("Error! board code 0x%016" PRIx64 " != expected 0x%016" PRIx64 " (XOR = 0x%016" PRIx64 ")\n", board->pawn_hash, temp.pawn_hash, xor);
			for (int i = 780; i >= 0; i--) {
				if (zobrist_keys[i] == xor) {
					printf("\tXOR = zobrist_keys[%d]\n", i);
					break;
				}
			}
			assert(0);
		} else if (new_board->nonpawn_hash[WHITE] != temp.nonpawn_hash[WHITE]) {
			uint64_t xor = temp.nonpawn_hash[WHITE] ^ new_board->nonpawn_hash[WHITE];

			printf("Error! board code 0x%016" PRIx64 " != expected 0x%016" PRIx64 " (XOR = 0x%016" PRIx64 ")\n", board->nonpawn_hash[WHITE], temp.nonpawn_hash[WHITE], xor);
			for (int i = 780; i >= 0; i--) {
				if (zobrist_keys[i] == xor) {
					printf("\tXOR = zobrist_keys[%d]\n", i);
					break;
				}
			}
			assert(0);
		} else if (new_board->nonpawn_hash[BLACK] != temp.nonpawn_hash[BLACK]) {
			uint64_t xor = temp.nonpawn_hash[BLACK] ^ new_board->nonpawn_hash[BLACK];

			printf("Error! board code 0x%016" PRIx64 " != expected 0x%016" PRIx64 " (XOR = 0x%016" PRIx64 ")\n", board->nonpawn_hash[BLACK], temp.nonpawn_hash[BLACK], xor);
			for (int i = 780; i >= 0; i--) {
				if (zobrist_keys[i] == xor) {
					printf("\tXOR = zobrist_keys[%d]\n", i);
					break;
				}
			}
			assert(0);
		}
	#endif

	return 0;
}
