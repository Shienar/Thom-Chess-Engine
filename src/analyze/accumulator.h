#ifndef ACCUMULATOR
#define ACCUMULATOR

#include "types.h"
#include "analyze/neuralnet.h"

#define MIRROR_SQUARE(x) x^=7

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

//Main thread accumulators
extern accumulator* playerAccumulator;
extern accumulatorRefreshTable* playingRefreshTable;

/**
 * Reinitializes input nodes and accumulator based off of a bitboard.
 */
void loadInputAccumulator(bitboard* board, accumulator* acc, int color);


void updateMoveAccumulator(bitboard* board, move lastMove, int shouldUndoMove, accumulator* acc,  accumulatorRefreshTable* refreshTable);

// Updates an accumulator & board until it reaches the same state as the currentboard.
// Both boards must have the same kingsquares.
void updateBoardAccumulator(bitboard* currentBoard, bitboard* accumulatorBoard, accumulator* acc, int color);

void updateAccumulatorFromTable(bitboard* currentBoard, accumulator* acc,  accumulatorRefreshTable* refreshTable);

accumulatorRefreshTable* createRefreshTable();

void destroyRefreshTable(accumulatorRefreshTable* refreshTable);

#endif