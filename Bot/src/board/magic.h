#ifndef MAGICBOARDS
#define MAGICBOARDS

#include "bitboard.h"

extern magic bishopMagics[64];
extern magic rookMagics[64];
extern uint64_t rookTable[102400];
extern uint64_t bishopTable[20480];

void initMagics();

//Not magic but similar enough to belong in this file.
extern uint64_t pawnAttacks[2][64]; //isThreatened Only
extern uint64_t knightAttacks[64];
extern uint64_t kingAttacks[64];
void initPawnAttacks();
void initKnightMoveTable();
void initKingMoveTable();

#endif