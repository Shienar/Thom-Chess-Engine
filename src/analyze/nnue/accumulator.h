#ifndef ACCUMULATOR
#define ACCUMULATOR

#include "types.h"
#include "analyze/nnue/neuralnet.h"

/**
 * Reinitializes input nodes and accumulator based off of a bitboard.
 */
void loadInputAccumulator(bitboard* board, accumulator* acc, int color);

void updateMoveAccumulator(bitboard* board, move_d lastMove, int shouldUndoMove, accumulator* acc);
#endif