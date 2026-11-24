#pragma once

#include "board/board.h"

int get_syzygy_result(bitboard *board);
void filter_syzygy_moves(bitboard *board, move *required_moves);
