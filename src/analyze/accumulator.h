#ifndef ACCUMULATOR
#define ACCUMULATOR

#include "types.h"
#include "analyze/neuralnet.h"

#define MIRROR_SQUARE(x) x^=7

//Main thread accumulators
extern accumulator* playerAccumulator;

/**
 * Reinitializes input nodes and accumulator based off of a bitboard.
 */
void loadInputAccumulator(bitboard* board, accumulator* acc, int color);

void updateMoveAccumulator(bitboard* board, move_d lastMove, int shouldUndoMove, accumulator* acc);


#endif