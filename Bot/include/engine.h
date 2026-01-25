#ifndef ENGINE
#define ENGINE

#include "structs.h"
#include "transpositiontable.h"
#include <time.h>

//The evaluation score given to draws.
#define CONTEMPT_FACTOR -0.25

/**
 * PeSTO Evaluation
 * Source: https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function#References
 * NOTE: This is a placeholder and will be replaced later.
 */

#define FLIP(square) ((square)^56)
#define OTHER(color) ((color)^(WHITE|BLACK))

void init_tables();

/**
 * Evaluates a position based on piece/square weights.
 * Returns white's score - black's score.
 */
double evaluate(bitboard* board);

/* End of PeSTO */

/**
 * Quiescence search function.
 * At the leaf nodes of alpha/beta, continue searching until a "quiet" 
 * position is reached. (Do extra searching for subsequent capture moves).
 */
double quiesce(bitboard* board, hashtable_tt* tt, double alpha, double beta, int depth);

/**
 * pv = move array of length depth - 1. Saved from previous iteration, different from pv table stored in engine.
 * 
 * Depth = "Depth remaining" = Maxdepth - ply (distance from root)
 * Call with depth == maxdepth;
 */
double principalVariationSearch(bitboard* board, hashtable_tt* tt, double alpha, double beta, int maxDepth, int depth, move* pv, int pvIndex, clock_t timeLimit);

/**
 * Iterative deepening function.
 */
move* calculateBestMove(bitboard* board, int maxDepth, int maxTimeSeconds);

#endif