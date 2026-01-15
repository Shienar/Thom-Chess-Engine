#ifndef ENGINE
#define ENGINE

#include "structs.h"
#include <time.h>

/**
 * Initializes piece weights.
 * Set useExisting to 1 to read weights from file (TODO).
 */
void initEnginePieceWeights(engine* w, int useExisting);

/**
 * Evaluates a position based on piece/square weights.
 * Returns white's score - black's score.
 */
double evaluate(bitboard* board, engine* w);

/**
 * Quiescence search function.
 * At the leaf nodes of alpha/beta, continue searching until a "quiet" 
 * position is reached. (Do extra searching for subsequent capture moves).
 */
double quiesce(bitboard* board, engine* engine, double alpha, double beta, int depth);

/**
 * pv = move array of length depth - 1. Saved from previous iteration, different from pv table stored in engine.
 * 
 * Depth = "Depth remaining" = Maxdepth - ply (distance from root)
 * Call with depth == maxdepth;
 */
double principalVariationSearch(bitboard* board, engine* engine, double alpha, double beta, int maxDepth, int depth, move* pv, int pvIndex, clock_t timeLimit);

/**
 * Iterative deepening function.
 */
move* calculateBestMove(bitboard* board, engine* engine, int maxDepth, int maxTimeSeconds);

#endif