#ifndef ENGINE
#define ENGINE

#include "types.h"
#include "hashtables/transpositiontable.h"
#include "analyze/neuralnet.h"
#include "analyze/accumulator.h"
#include "analyze/sygyzy.h"
#include <time.h>

extern int threadCount;
extern int enablePonder;
extern int isCalculating;
#define MIN_THREADS 1
#define MAX_THREADS 64

#define MIN_ASPIRATION_DEPTH 5
#define INITIAL_ASPIRATION_MARGIN 20
#define MAXIMUM_ASPIRATION_MARGIN 160
#define ASPIRATION_MARGIN_MULT_FACTOR 2

#define REVERSE_FUTILITY_PRUNING_DEPTH 4
#define REVERSE_FUTILITY_MARGIN 200
#define REVERSE_FUTILITY_MARGIN_IMPROVING 125

#define FUTILITY_PRUNING_DEPTH 2
#define FUTILITY_MARGIN 350

#define NULLMOVE_PRUNING_DEPTH 5

#define PROBCUT_DEPTH 8
#define PROBCUT_DEPTH_REDUCTION 4
#define PROBCUT_OFFSET 400
#define PROBCUT_OFFSET_IMPROVING 300

#define LM_DEPTH 4
#define LM_BASE 2.0f
#define LM_SCALE 0.5f

#define LARGE_DELTA 500

void initSearchTables();
int perft(bitboard* board, int depth, int verbose);
int quiescentSearch(searchThreadContext* context, int alpha, int beta, int ply);
int principalVariationSearch(searchThreadContext* context, int alpha, int beta, int depth, int ply, PVar* myPV);
THREAD_RETURN calculateBestMove(THREAD_PARAM param);

#endif