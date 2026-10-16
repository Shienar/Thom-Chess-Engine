//Based on this paper by Marcel van Kervinck
//http://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf
#pragma once

#include "types.h"

#define CUCKOO_SIZE 8192
#define CUCKOO_BITS 8191

extern uint64_t betweenSquares[64][64];

extern uint64_t cuckooHashes[CUCKOO_SIZE];
extern move     cuckooMoves[CUCKOO_SIZE];

#define CUCKOO_HASH1(hash64) (hash64 & CUCKOO_BITS)
#define CUCKOO_HASH2(hash64) ((hash64 >> 16) & CUCKOO_BITS)

void initCuckooMoveTable();
int isUpcomingRepetition(bitboard* board, repetitionVector repetitions, int ply);