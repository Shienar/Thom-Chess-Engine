#ifndef ENGINE
#define ENGINE

#include "../types.h"
#include "../hashtables/transpositiontable.h"
#include "neuralnet.h"
#include "accumulator.h"
#include <time.h>

//The evaluation score given to draws.
#define CONTEMPT_FACTOR_SCALE_EARLYGAME 15.0
#define CONTEMPT_FACTOR_SCALE_MIDDLEGAME 8.0
#define CONTEMPT_FACTOR_SCALE_ENDGAME 2.0

#define MIDDLEGAME_START_HALFMOVES 20 
#define MIDDLEGAME_END_HALFMOVES 60 

#define CONTEMPT_FACTOR_STALEMATE -15
#define CONTEMPT_FACTOR_THREEFOLD -75
#define CONTEMPT_FACTOR_FIFTYMOVERULE -40
#define CONTEMPT_FACTOR_INSUFFICIENT_MATERIAL -100

#define SCORE_WIN 1e12f

extern int useHelperThreads;
#ifndef HELPER_THREAD_COUNT
#define HELPER_THREAD_COUNT 8
#endif

#define INITIAL_ASPIRATION_MARGIN 32.0
#define MAXIMUM_ASPIRATION_MARGIN 512.0
#define ASPIRATION_MARGIN_MULT_FACTOR 2.0

int perft(bitboard* board, int depth, int maxDepth, int verbose);

/**
 * Evaluates a position based on piece/square weights.
 * Returns white's score - black's score.
 * 
 * One of the accumulators will be used for forward propagation;
 * the other should be NULL.
 */
float evaluate(bitboard* board, accumulator* acc);

/**
 * Quiescence search function.
 * At the leaf nodes of alpha/beta, continue searching until a "quiet" 
 * position is reached. (Do extra searching for subsequent capture moves).
 */
float quiesce(bitboard* board, float alpha, float beta, int depth, accumulator* acc, accumulatorRefreshTable* refreshTable);

/**
 * pv = move array of length depth - 1. Saved from previous iteration, different from pv table stored in engine.
 * 
 * Depth = "Depth remaining" = Maxdepth - ply (distance from root)
 * Call with depth == maxdepth;
 * 
 * timeLimit is passed as a pointer. You can modify its value from another thread to end the search early.
 */
float principalVariationSearch(bitboard* board, float alpha, float beta, int maxDepth, int depth, move* pv, int pvIndex, clock_t* timeLimit, accumulator* acc, accumulatorRefreshTable* refreshTable);

/**
 * Iterative deepening function.
 */
move calculateBestMove(bitboard* board, int maxDepth, int maxTimeSeconds);

#endif