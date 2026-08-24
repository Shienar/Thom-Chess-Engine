#ifndef SYZYGY
#define SYZYGY

#include "types.h"
#include "board/bitboard.h"
#include "pyrrhic/tbprobe.h"

int getSyzygyResult(bitboard* board);
void filterSyzygyMoves(bitboard* board, move* requiredMoves);

#endif
