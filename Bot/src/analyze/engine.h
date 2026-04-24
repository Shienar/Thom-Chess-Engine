#ifndef ENGINE
#define ENGINE

#include "../structs.h"
#include "../hashtables/transpositiontable.h"
#include "neuralnet.h"
#include "accumulator.h"
#include <time.h>

//The evaluation score given to draws.
#define CONTEMPT_FACTOR_SCALE_EARLYGAME 2.5
#define CONTEMPT_FACTOR_SCALE_MIDDLEGAME 1.5
#define CONTEMPT_FACTOR_SCALE_ENDGAME 0.75

#define MIDDLEGAME_START_HALFMOVES 20 
#define MIDDLEGAME_END_HALFMOVES 60 

#define CONTEMPT_FACTOR_STALEMATE -0.25
#define CONTEMPT_FACTOR_THREEFOLD -1
#define CONTEMPT_FACTOR_FIFTYMOVERULE -0.25
#define CONTEMPT_FACTOR_INSUFFICIENT_MATERIAL -0.25

extern int useHelperThreads;
#ifndef HELPER_THREAD_COUNT
#define HELPER_THREAD_COUNT 8
#endif

//Score are in the range of [-128, 127]
#define INITIAL_ASPIRATION_MARGIN 10.0
#define MAXIMUM_ASPIRATION_MARGIN 40.0
#define ASPIRATION_MARGIN_MULT_FACTOR 2.0

#define MAX_DEPTH 20

int perft(bitboard* board, int depth, int maxDepth, int verbose);

/**
 * Evaluates a position based on piece/square weights.
 * Returns white's score - black's score.
 * 
 * One of the accumulators will be used for forward propagation;
 * the other should be NULL.
 */
double evaluate(bitboard* board, void* accumulator, int accumulatorType);

/**
 * Quiescence search function.
 * At the leaf nodes of alpha/beta, continue searching until a "quiet" 
 * position is reached. (Do extra searching for subsequent capture moves).
 */
double quiesce(bitboard* board, double alpha, double beta, int depth, void* accumulator, void* accumulatorTable, int accumulatorType);

/**
 * pv = move array of length depth - 1. Saved from previous iteration, different from pv table stored in engine.
 * 
 * Depth = "Depth remaining" = Maxdepth - ply (distance from root)
 * Call with depth == maxdepth;
 * 
 * timeLimit is passed as a pointer. You can modify its value from another thread to end the search early.
 */
double principalVariationSearch(bitboard* board, double alpha, double beta, int maxDepth, int depth, move* pv, int pvIndex, clock_t* timeLimit, void* accumulator, void* accumulatorTable, int accumulatorType);

/**
 * Iterative deepening function.
 */
move calculateBestMove(bitboard* board, int maxDepth, int maxTimeSeconds, int networkType);

#endif