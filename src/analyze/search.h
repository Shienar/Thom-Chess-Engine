#ifndef ENGINE
#define ENGINE

#include "types.h"
#include "hashtables/transpositiontable.h"
#include "analyze/syzygy.h"
#include <time.h>

#include "analyze/hce/hce.h"

extern uint8_t abortFlag;
extern uint8_t isPonder;

extern int threadCount;
extern int enablePonder;
extern int isCalculating;
extern int suppressUCIMessages;
#define MIN_THREADS 1
#define MAX_THREADS 64

extern int initial_aspiration_margin;
extern int maximum_aspiration_margin;
extern float aspiration_margin_mult_factor;

extern int delta_pruning_offset;

extern int futility_margin;
extern int futility_margin_improving;

extern int reverse_futility_margin;
extern int reverse_futility_margin_improving;

extern int probcut_offset;
extern int probcut_offset_improving;

extern int historyBonusScale;
extern int historyBonusOffset;
extern int historyPenaltyScale;
extern int historyPenaltyOffset;

extern int lowHistoryVal;

extern float lmr_a;
extern float lmr_b;

extern float lmp_a;
extern float lmp_b;
extern float lmp_improving_a;
extern float lmp_improving_b;


void initSearchTables();
int perft(bitboard* board, int depth, int verbose);
int quiescentSearch(searchThreadContext* context, int alpha, int beta, int ply);
int principalVariationSearch(searchThreadContext* context, int alpha, int beta, int depth, int ply, PVar* myPV, int isCutNode);
THREAD_RETURN calculateBestMove(THREAD_PARAM param);

#endif