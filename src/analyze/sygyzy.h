#ifndef SYGYZY
#define SYGYZY

#include "types.h"
#include "board/bitboard.h"
#include "pyrrhic/tbprobe.h"

#define SCORE_WIN 1e8

int getSygyzyResult(bitboard* board);
void filterSygyzyMoves(bitboard* board, move* requiredMoves);

#endif
