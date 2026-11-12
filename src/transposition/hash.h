#pragma once

#include <stdint.h>
#include "board/board.h"

extern uint64_t zobrist_keys[781];
extern uint64_t zobrist_piece_keys[PIECE_COUNT][64];

void initZobristPieceKeys();

uint64_t getEnPassantHash(bitboard* board);
void generateHashCode(bitboard* board);