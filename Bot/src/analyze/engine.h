#ifndef ENGINE
#define ENGINE

#include "../types.h"
#include "../hashtables/transpositiontable.h"
#include "neuralnet.h"
#include "accumulator.h"
#include <time.h>

//The evaluation score given to draws.
#define CONTEMPT_FACTOR_SCALE_EARLYGAME 15.0f
#define CONTEMPT_FACTOR_SCALE_MIDDLEGAME 8.0f
#define CONTEMPT_FACTOR_SCALE_ENDGAME 2.0f

#define MIDDLEGAME_START_HALFMOVES 20.0f 
#define MIDDLEGAME_END_HALFMOVES 60.0f

#define CONTEMPT_FACTOR_STALEMATE -15.0f
#define CONTEMPT_FACTOR_THREEFOLD -75.0f
#define CONTEMPT_FACTOR_FIFTYMOVERULE -40.0f
#define CONTEMPT_FACTOR_INSUFFICIENT_MATERIAL -100.0f

#define SCORE_WIN 1e12f

extern int threadCount;
extern int enablePonder;
extern int isCalculating;
#define MIN_THREADS 1
#define MAX_THREADS 64

#define MIN_ASPIRATION_DEPTH 5
#define INITIAL_ASPIRATION_MARGIN 32.0f
#define MAXIMUM_ASPIRATION_MARGIN 512.0f
#define ASPIRATION_MARGIN_MULT_FACTOR 2.0f

int perft(bitboard* board, int depth, int maxDepth, int verbose);

/**
 * Quiescence search function.
 * At the leaf nodes of alpha/beta, continue searching until a "quiet" 
 * position is reached. (Do extra searching for subsequent capture moves).
 */
float quiesce(THREAD_PARAM param, float alpha, float beta, int ply);

/**
 * pv = move array of length depth - 1. Saved from previous iteration, different from pv table stored in engine.
 * 
 * Depth = "Depth remaining" = Maxdepth - ply (distance from root)
 * Call with depth == maxdepth;
 * 
 * timeLimit is passed as a pointer. You can modify its value from another thread to end the search early.
 */
float principalVariationSearch(THREAD_PARAM param, float alpha, float beta, int maxDepth, int depth, int pvIndex);

/**
 * Iterative deepening function.
 */
THREAD_RETURN calculateBestMove(THREAD_PARAM param);

#endif