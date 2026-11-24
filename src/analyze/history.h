#pragma once

#include "board/board.h"

#define CORRHIST_SIZE 16384
#define MAX_HISTORY_SCORE 16384
#define CAPTURE_HISTORY_SCALE 4 //This is mainly use to avoid int16 limits at this point. It used to significantly reduce the impact of history values vs SEE ordering.
#define MAX_CORRHIST_VAL 256
#define CORRHIST_SCALE 32

extern int history_bonus_scale;
extern int history_bonus_offset;
extern int history_penalty_scale;
extern int history_penalty_offset;

extern int capture_history_bonus_scale;
extern int capture_history_bonus_offset;
extern int capture_history_penalty_scale;
extern int capture_history_penalty_offset;

void update_killer_moves(struct thread_context *context, move current_move, int ply);

int  get_history_value(struct thread_context *context, int turn, int piece, int to);
void update_history_values(int16_t *history_table, int boosted_index, int searched_quiet_indices[MAX_MOVES], int searched_quiet_count, int depth);

void update_continuation_history(struct thread_context *context, move current_move, int ply);
move get_counter_move(struct thread_context *context, int ply);
move get_followup_move(struct thread_context *context, int ply);

int  get_capture_history_offset(struct thread_context *context, int piece, int to, int captured_piece);
void apply_capture_history_bonus(int16_t *dest, int depth);
void apply_capture_history_penalty(int16_t *dest, int depth);

void update_correction_history(int16_t *old_hist, int depth, int search_score, int static_score);
int  get_correction_history_offset(struct thread_context *context, bitboard *board);
