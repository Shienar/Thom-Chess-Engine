#pragma once

#include <stdint.h>

typedef struct magic {
	uint64_t mask;
	uint64_t magic;
	uint64_t *attacks;
	int8_t shift_offset;
} magic;

//Fancy magic bitboards
//Hashed Index = relevantOccupancy * squareMagic >> squareShiftOffset
//squareShiftOffset = 64 - popcount(relevantOccupancy)
extern magic bishop_magics[64];
extern magic rook_magics[64];
extern uint64_t rook_table[102400];
extern uint64_t bishop_table[20480];

void initMagics(void);

//Not magic but similar enough to belong in this file.
extern uint64_t pawn_attacks[2][64]; //is_threatened only
extern uint64_t knight_attacks[64];
extern uint64_t king_attacks[64];
void init_pawn_attacks(void);
void init_knight_move_table(void);
void init_king_move_table(void);
