#pragma once

#include <stdint.h>
#include "board/board.h"

extern uint64_t zobrist_keys[781];
extern uint64_t zobrist_piece_keys[PIECE_COUNT][64];

void init_zobrist_piece_keys(void);

uint64_t get_ep_hash(bitboard *board);
void generate_hash_code(bitboard *board);
