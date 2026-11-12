#pragma once

#include "board/board.h"

int getSyzygyResult(bitboard* board);
void filterSyzygyMoves(bitboard* board, move* requiredMoves);
