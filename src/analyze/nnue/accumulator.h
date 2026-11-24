#pragma once

#include "board/bitboard.h"
#include "analyze/nnue/neuralnet.h"

void load_input_accumulator(bitboard * board, accumulator * acc);
void update_move_accumulator(bitboard *board, move last_move, int captured_piece, int is_en_passant, accumulator *input_acc, accumulator *output_acc, accumulator_refresh_table *refresh_table);
void update_accumulator_from_table(bitboard *board, accumulator *acc,  accumulator_refresh_table *refresh_table);
