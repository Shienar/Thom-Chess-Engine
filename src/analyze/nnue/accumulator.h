#ifndef ACCUMULATOR
#define ACCUMULATOR

#include "types.h"

//https://www.chessprogramming.org/Flipping_Mirroring_and_Rotating#Vertical
#define k1 0x5555555555555555
#define k2 0x3333333333333333
#define k4 0x0f0f0f0f0f0f0f0f
static inline uint64_t mirrorBoard(uint64_t x)
{
   x = ((x >> 1) & k1) | ((x & k1) << 1);
   x = ((x >> 2) & k2) | ((x & k2) << 2);
   x = ((x >> 4) & k4) | ((x & k4) << 4);
   return x;
}

/**
 * Reinitializes input nodes and accumulator based off of a bitboard.
 */
void loadInputAccumulator(bitboard* board, accumulator* acc);

void updateMoveAccumulator(bitboard* board, move_d lastMove, accumulator* inputAcc, accumulator* outputAcc, accumulatorRefreshTable* refreshTable);

void updateAccumulatorFromTable(bitboard* board, accumulator* acc,  accumulatorRefreshTable* refreshTable);

accumulatorRefreshTable* createRefreshTable();
void destroyRefreshTable(accumulatorRefreshTable* refreshTable);
#endif