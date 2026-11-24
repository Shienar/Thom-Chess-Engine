#include "analyze/history.h"
#include "board/bitboard.h"
#include "analyze/search.h"
#include <stdlib.h>

int history_bonus_scale = 266;
int history_bonus_offset = 181;
int history_penalty_scale = 431;
int history_penalty_offset = 194;

int capture_history_bonus_scale = 346;
int capture_history_bonus_offset = 121;
int capture_history_penalty_scale = 456;
int capture_history_penalty_offset = 169;

void update_killer_moves(thread_context *context, move current_move, int ply)
{
	if (current_move.raw != context->killer_moves[ply][0].raw) {
		context->killer_moves[ply][1] = context->killer_moves[ply][0];
		context->killer_moves[ply][0] = current_move;
	}
}

int get_history_value(thread_context *context, int turn, int piece, int to)
{
	return context->history_table[turn][piece / 2][to];
}

void update_history_values(int16_t *history_table, int boosted_index, int searched_quiet_indices[MAX_MOVES], int searched_quiet_count, int depth)
{
	int bonus = _min(MAX_HISTORY_SCORE, _max(-MAX_HISTORY_SCORE, history_bonus_scale * depth + history_bonus_offset));
	int penalty = -_min(MAX_HISTORY_SCORE, _max(-MAX_HISTORY_SCORE, history_penalty_scale * depth + history_penalty_offset));

	history_table[boosted_index] += bonus - history_table[boosted_index] * abs(bonus) / MAX_HISTORY_SCORE;
	for (int i = 0; i < searched_quiet_count; i++)
		history_table[searched_quiet_indices[i]] += penalty - history_table[searched_quiet_indices[i]] * abs(penalty) / MAX_HISTORY_SCORE;
}

void update_continuation_history(thread_context *context, move current_move, int ply)
{
	//Countermove heuristic
	if (ply >= 1) {
		int side = context->board_stack[ply - 1].turn;
		int from = context->move_stack[ply - 1].start_square;
		int piece = PIECE((context->board_stack[ply - 1].piece_arr[from])) / 2;
		int to = context->move_stack[ply - 1].end_square;

		context->counter_move[side][piece][to] = current_move;

		//Follow-up Move heuristic
		if (ply >= 2) {
			int side = context->board_stack[ply - 2].turn;
			int from = context->move_stack[ply - 2].start_square;
			int piece = PIECE((context->board_stack[ply - 2].piece_arr[from])) / 2;
			int to = context->move_stack[ply - 2].end_square;

			context->followup_move[side][piece][to] = current_move;
		}
	}
}

move get_counter_move(thread_context *context, int ply)
{
	if (ply < 1 || !context->move_stack[ply - 1].raw)
		return (move){0};

	int side = context->board_stack[ply - 1].turn;
	int from = context->move_stack[ply - 1].start_square;
	int piece = PIECE(context->board_stack[ply - 1].piece_arr[from]) / 2;
	int to = context->move_stack[ply - 1].end_square;

	return context->counter_move[side][piece][to];
}

move get_followup_move(thread_context *context, int ply)
{
	if (ply < 2 || !context->move_stack[ply - 2].raw)
		return (move){0};

	int side = context->board_stack[ply - 2].turn;
	int from = context->move_stack[ply - 2].start_square;
	int piece = PIECE(context->board_stack[ply - 2].piece_arr[from]) / 2;
	int to = context->move_stack[ply - 2].end_square;

	return context->followup_move[side][piece][to];
}

int get_capture_history_offset(thread_context *context, int piece, int to, int captured_piece)
{
	return context->capture_history_table[piece / 2][to][captured_piece / 2] / CAPTURE_HISTORY_SCALE;
}

void apply_capture_history_bonus(int16_t *dest, int depth)
{
	*dest = _min(*dest + (capture_history_bonus_scale * depth + capture_history_bonus_offset), MAX_HISTORY_SCORE);
}

void apply_capture_history_penalty(int16_t *dest, int depth)
{
	*dest = _max(*dest - (capture_history_penalty_scale * depth + capture_history_penalty_offset), -MAX_HISTORY_SCORE);
}

void update_correction_history(int16_t *old_hist, int depth, int search_score, int static_score)
{
	*old_hist = _min(MAX_CORRHIST_VAL, _max(-MAX_CORRHIST_VAL, *old_hist + depth * depth * (search_score - static_score)));
}

int get_correction_history_offset(thread_context *context, bitboard *board)
{
	int correction = 0;

	correction += context->pawn_corrhist[board->turn][board->pawn_hash & (CORRHIST_SIZE - 1)];
	correction += context->nonpawn_corrhist[WHITE][board->turn][board->nonpawn_hash[WHITE] & (CORRHIST_SIZE - 1)];
	correction += context->nonpawn_corrhist[BLACK][board->turn][board->nonpawn_hash[BLACK] & (CORRHIST_SIZE - 1)];

	return correction / CORRHIST_SCALE;
}
