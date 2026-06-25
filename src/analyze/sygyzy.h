#ifndef SYGYZY
#define SYGYZY

#include "types.h"
#include "board/bitboard.h"
#include "pyrrhic/tbprobe.h"

int getSygyzyResult(bitboard* board);
void filterSygyzyMoves(bitboard* board, move* requiredMoves);

#endif
