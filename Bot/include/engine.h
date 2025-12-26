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
double quiesce(bitboard* board, engine* engine, double alpha, double beta);

/**
 * Depth = "Depth remaining" = Maxdepth - ply (distance from root)
 */
double principalVariationSearch(bitboard* board, engine* engine, double alpha, double beta, int depth, int pvIndex);

/**
 * Iterative deepening function.
 */
move* calculateBestMove(bitboard* board, engine* engine, int maxDepth, int maxTimeSeconds);

#endif