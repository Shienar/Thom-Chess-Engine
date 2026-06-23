#ifndef ENGINE
#define ENGINE

#include "types.h"
#include "hashtables/transpositiontable.h"
#include "analyze/neuralnet.h"
#include "analyze/accumulator.h"
#include "analyze/sygyzy.h"
#include <time.h>

#define MIN_MATE_SCORE (SCORE_WIN - MAX_PLY)

extern int threadCount;
extern int enablePonder;
extern int isCalculating;
#define MIN_THREADS 1
#define MAX_THREADS 64

#define MIN_ASPIRATION_DEPTH 5
#define INITIAL_ASPIRATION_MARGIN 16
#define MAXIMUM_ASPIRATION_MARGIN 128
#define ASPIRATION_MARGIN_MULT_FACTOR 2

#define REVERSE_FUTILITY_PRUNING_DEPTH 7
#define FUTILITY_PRUNING_DEPTH 4
#define NULLMOVE_PRUNING_DEPTH 5

#define REVERSE_FUTILITY_MARGIN 150
#define FUTILITY_MARGIN 2500

#define LM_DEPTH 7
#define LM_BASE 2.0f
#define LM_SCALE 0.5f

#define LARGE_DELTA 900

void initLMTable();

int perft(bitboard* board, int depth, int maxDepth, int verbose);

/**
 * Quiescence search function.
 * At the leaf nodes of alpha/beta, continue searching until a "quiet" 
 * position is reached. (Do extra searching for subsequent capture moves).
 */
int quiesce(searchThreadContext* context, float alpha, float beta, int ply);

/**
 * pv = move array of length depth - 1. Saved from previous iteration, different from pv table stored in engine.
 * 
 * Depth = "Depth remaining"  - normally described by Maxdepth - ply, but extensions/reductions exist.
 * Call with depth == maxdepth;
 * 
 * timeLimit is passed as a pointer. You can modify its value from another thread to end the search early.
 */
int principalVariationSearch(searchThreadContext* context, float alpha, float beta, int maxDepth, int depth, int ply, PVar* myPV);

/**
 * Iterative deepening function.
 */
THREAD_RETURN calculateBestMove(THREAD_PARAM param);

#endif