#include "binpack/generate.h"
#include "binpack/viri_binpack.h"
#include "analyze/search.h"
#include "board/moves.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>

binpack_details details;
int exit_while;

void generate(const char *path)
{
	exit_while = 0;
	int concurrency = thread_count;

	thread_count = 1;

	int was_debug_enabled = print_debug_messages;

	if (was_debug_enabled)
		disable_debug_messages();

	suppress_uci_messages = 1;
	thread_context *context_list = calloc(concurrency, sizeof(thread_context));

	THREADTYPE *thread_list = calloc(concurrency, sizeof(THREADTYPE));

	binpack_open(&details, path, 0);
	details.start_time = clock();

	for (int i = 0; i < concurrency; i++) {
		context_list[i].start_time = 0,
		context_list[i].hard_end_time = LONG_MAX,
		context_list[i].soft_end_time = LONG_MAX,
		context_list[i].max_depth = 9;
		context_list[i].hard_max_nodes = 5000;
		context_list[i].soft_max_nodes = 5000;
		context_list[i].abort_flag = calloc(1, sizeof(uint8_t));
		context_list[i].deepening_skip = (rand() << 16) | rand(); //Used as rng seed & reset to zero by worker thread.

		if (use_nnue) {
			context_list[i].accumulator_stack = calloc(MAX_PLY + 1, sizeof(accumulator));
			context_list[i].refresh_table = calloc(1, sizeof(accumulator_refresh_table));
		}

		context_list[i].tt = create_tt_table();

		THREAD_START(thread_list[i], generate_worker_thread, &context_list[i]);
	}

	printf("Started generating data. Type 'stop' to stop.\n");
	char buffer[128];
	char *strtok_ptr;
	const char *delim = " \t\r\n";

	while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
		char *token = _strtok(buffer, delim, &strtok_ptr);

		while (token != NULL) {
			if (strcmp(token, "stop") == 0)
				goto cleanup;

			token = _strtok(NULL, delim, &strtok_ptr);
		}
	}

cleanup:
	exit_while = 1;

	printf("Stopping all threads...\n");
	for (int i = 0; i < concurrency; i++) {
		*context_list[i].abort_flag = 1;
		THREAD_WAIT(thread_list[i]);
		destroy_tt_table(context_list[i].tt);
		free(context_list[i].abort_flag);

		if (use_nnue) {
			free(context_list[i].accumulator_stack);
			free(context_list[i].refresh_table);
		}
	}

	thread_count = concurrency;
	suppress_uci_messages = 0;

	binpack_close(&details);
	if (was_debug_enabled)
		enable_debug_messages();
	printf("Stopped generating data.\n");

	binpack_print_info(path);
}

uint32_t rng_xorshift32(uint32_t *seed)
{
	uint32_t x = *seed;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*seed = x;
	return x;
}

/**
 * Make eight random moves on the board, creating some random opening position.
 * Do a search on this position, returning me a score and a move.
 * The score gets applied to the current position and the move creates the next position.
 * The final move of the game doesn't get included in the binpack.
 *
 * If the initial position is noisy (abs(cp) > 300), skip that game and try another opening.
 *
 * If one side is dominant (> 2000 cp) for 5 moves, adjudicate an early win.
 * If both sides are tied for 5 moves after move 40 (cp in [-10, +10]), adjudicate an early draw
 *
 * If the score is a mate score, we can terminate early & don't need to play out the full continuation.
 */
THREAD_RETURN generate_worker_thread(THREAD_PARAM param)
{
	thread_context *context = (thread_context *) param;
	bitboard *board = &context->board_stack[0];
	bitboard temp_board;

	int is_new_game = 1;

	generated_game_buffer *write_buffer = calloc(1, sizeof(generated_game_buffer));

	for (int i = 0; i < VIRI_WRITEBUFFER_SIZE; i++)
		write_buffer->packed_boards[i].white_score = INT16_MAX;

	int cur_game_index = 0;
	int moves_this_game = 0;

	uint32_t seed = context->deepening_skip;

	context->deepening_skip = 0;

	//Adjudication
	int consecutive_high_scores = 0;
	int consecutive_low_scores = 0;
	int consecutive_draw_scores = 0;

	while (!exit_while) {
		viri_packed_board *packed_board = &write_buffer->packed_boards[cur_game_index];
		viri_move_score_pair *pair_list = write_buffer->pair_lists[cur_game_index];

		if (is_new_game) {
newgame:
			load_fen_string_to_board(board, STARTPOS_FEN, &context->repetitions);

			//Play some random moves
			for (int i = 0; i < 8; i++) {
				move move_list[MAX_MOVES] = {0};
				int count = generate_move_list(move_list, board, GET_ALL_MOVES);

				if (!count)
					goto newgame;
				int index = rng_xorshift32(&seed) % count;

				if (move_from_struct(board, &temp_board, move_list[index], &context->repetitions)) {
					int found_legal = 0;

					for (int j = (index + 1) % count; j != index; j = (j + 1) % count) {
						if (!move_from_struct(board, &temp_board, move_list[j], &context->repetitions)) {
							found_legal = 1;
							break;
						}
					}
					if (!found_legal)
						goto newgame;
				}
				memcpy(board, &temp_board, sizeof(bitboard));
			}

			is_new_game = 0;
			moves_this_game = 0;
			consecutive_high_scores = 0;
			consecutive_low_scores = 0;
			consecutive_draw_scores = 0;

			board_to_packed_board(board, packed_board);
			packed_board->result = UINT8_MAX;
			clear_tt(context->tt);
			continue;
		}

		assert(board->pieces[WHITE_KING] && board->pieces[BLACK_KING]);

		calculate_best_move(param);

		move best_move = context->pv.line[0];

		int white_eval = (ISWHITE(board->turn)) ? context->score : -context->score;

		if (moves_this_game == 0 && abs(white_eval) > 300) {
			//Reject noisy opening.
			is_new_game = 1;
			continue;
		}

		if (white_eval >= MIN_MATE_SCORE)
			packed_board->result = VIRI_WHITE_WIN;
		else if (white_eval <= -MIN_MATE_SCORE)
			packed_board->result = VIRI_BLACK_WIN;

		consecutive_high_scores = (white_eval > 2000) ? consecutive_high_scores + 1 : 0;
		consecutive_low_scores = (white_eval < -2000) ? consecutive_low_scores + 1 : 0;
		consecutive_draw_scores = (moves_this_game > 32 && white_eval > -10 && white_eval < 10) ? consecutive_draw_scores + 1 : 0;

		if (consecutive_high_scores > 5)
			packed_board->result = VIRI_WHITE_WIN;
		else if (consecutive_low_scores > 5)
			packed_board->result = VIRI_BLACK_WIN;
		else if (consecutive_draw_scores > 5)
			packed_board->result = VIRI_DRAW;

		if (packed_board->result < UINT8_MAX) {
adjudication:

			write_buffer->moves_this_game[cur_game_index] = moves_this_game;
			cur_game_index++;
			if (cur_game_index >= VIRI_WRITEBUFFER_SIZE) {
				binpack_write(&details, write_buffer);
				cur_game_index = 0;
			}
			is_new_game = 1;
			continue;
		}

		if (best_move.raw == 0)
			goto gameover;

		int piece = (board->piece_arr[best_move.start_square]);
		int difference = best_move.end_square - best_move.start_square;

		pair_list[moves_this_game].move.end_square = best_move.end_square;
		pair_list[moves_this_game].move.start_square = best_move.start_square;
		pair_list[moves_this_game].move.promote_piece = promote_mappings_to_viri[best_move.promote_to];
		if (best_move.promote_to)
			pair_list[moves_this_game].move.move_type = VIRI_MOVE_TYPE_PROMOTIONS;
		else if (ISKING(piece) && abs(difference) == 2) {
			pair_list[moves_this_game].move.move_type = VIRI_MOVE_TYPE_CASTLING;

			//Reformat move as king-takes rook (Assumes standard variant)
			int row_offset = get_row(best_move.start_square) * 8;

			if (best_move.end_square > best_move.start_square)
				pair_list[moves_this_game].move.end_square = row_offset + 7;
			else
				pair_list[moves_this_game].move.end_square = row_offset;
		} else if (ISPAWN(piece) && best_move.end_square == board->ep_square)
			pair_list[moves_this_game].move.move_type = VIRI_MOVE_TYPE_ENPASSANT;
		else
			pair_list[moves_this_game].move.move_type = VIRI_MOVE_TYPE_NONE;

		pair_list[moves_this_game].white_score = white_eval;

		moves_this_game++;
		if (moves_this_game >= MAX_POSITIONS_PER_GAME) {
			//Determine game result without saving extra moves.
			do {
				calculate_best_move(param);
				int white_eval = (ISWHITE(board->turn)) ? context->score : -context->score;

				consecutive_high_scores = (white_eval > 2000) ? consecutive_high_scores + 1 : 0;
				consecutive_low_scores = (white_eval < -2000) ? consecutive_low_scores + 1 : 0;
				consecutive_draw_scores = (white_eval > -10 && white_eval < 10) ? consecutive_draw_scores + 1 : 0;

				if (consecutive_high_scores > 5 || white_eval >= MIN_MATE_SCORE)
					packed_board->result = VIRI_WHITE_WIN;
				else if (consecutive_low_scores > 5 || white_eval <= -MIN_MATE_SCORE)
					packed_board->result = VIRI_BLACK_WIN;
				else if (consecutive_draw_scores > 5)
					packed_board->result = VIRI_DRAW;

				if (packed_board->result < UINT8_MAX)
					goto adjudication;


			} while (!move_from_struct(board, board, context->pv.line[0], &context->repetitions));
			goto gameover;
		}

		//End of game
		if (move_from_struct(board, board, best_move, &context->repetitions)) {
gameover:

			if (is_draw(board, &context->repetitions))
				packed_board->result = VIRI_DRAW;
			else {
				//Should get detected early, this is a fallback.
				packed_board->result = get_mate_result(board);
				switch (packed_board->result) {
				case VICTOR_BLACK:
					packed_board->result = VIRI_BLACK_WIN;
					break;
				case VICTOR_WHITE:
					packed_board->result = VIRI_WHITE_WIN;
					break;
				default:
					packed_board->result = VIRI_DRAW;
					break;
				}
			}

			write_buffer->moves_this_game[cur_game_index] = moves_this_game;
			cur_game_index++;
			if (cur_game_index >= VIRI_WRITEBUFFER_SIZE) {
				binpack_write(&details, write_buffer);
				cur_game_index = 0;
			}
			is_new_game = 1;
			continue;
		}
	}

	free(write_buffer);
	return 0;
}
