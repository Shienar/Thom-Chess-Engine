#ifndef HASHTABLE
#define HASHTABLE

#include "structs.h"
#include "engine.h"
#include <stdint.h>

#define STARTING_CAPACITY 1024

/**
 * 0-5 = white
 * 6-11 = black
 * Index = define piece value - 1 
 */
extern uint64_t zobrist_pieceSquareValues[64][12];

extern uint64_t zobrist_blackToMove;
extern uint64_t zobrist_whiteToMove;
extern uint64_t zobrist_castle_wk;
extern uint64_t zobrist_castle_wq;
extern uint64_t zobrist_castle_bk;
extern uint64_t zobrist_castle_bq;
extern uint64_t zobrist_enPassantFile[8];

void generateZobristRandoms();
uint64_t getHashCode(bitboard* board);

#endif