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

extern const int min_aspiration_depth;
extern const int reverse_futility_pruning_depth;
extern const int futility_pruning_depth;
extern const int nullmove_pruning_depth;
extern const int probcut_depth;
extern const int probcut_depth_reduction;
extern const int lm_depth;

extern int initial_aspiration_margin;
extern int maximum_aspiration_margin;
extern float aspiration_margin_mult_factor;

extern int reverse_futility_margin;
extern int reverse_futility_margin_improving;

extern int futility_margin;

extern int probcut_offset;
extern int probcut_offset_improving;

extern float lm_base;
extern float lm_scale;

extern int delta_pruning_offset;

void initSearchTables();
int perft(bitboard* board, int depth, int verbose);
int quiescentSearch(searchThreadContext* context, int alpha, int beta, int ply);
int principalVariationSearch(searchThreadContext* context, int alpha, int beta, int depth, int ply, PVar* myPV);
THREAD_RETURN calculateBestMove(THREAD_PARAM param);

#endif