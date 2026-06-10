#ifndef SYGYZY
#define SYGYZY

#include "types.h"
#include "board/bitboard.h"
#include "pyrrhic/tbprobe.h"

#define SCORE_WIN 1e6f

float getSygyzyResult(bitboard* board);
void filterSygyzyMoves(bitboard* board, move* requiredMoves);

#endif
