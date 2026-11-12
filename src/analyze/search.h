#pragma once

#include <time.h>
#include "transposition/transpositiontable.h"
#include "analyze/nnue/accumulator.h"
#include "analyze/history.h"
#include "board/board.h"
#include "compatibility.h"

extern uint8_t abortFlag;
extern volatile uint8_t isPonder;

#ifdef SEARCHINFO
#define RECORD_SEARCH(x) do { x } while(0)
#else
#define RECORD_SEARCH(x)
#endif

extern int threadCount;
extern int enablePonder;
extern int isSearching;
extern int suppressUCIMessages;

#define MIN_THREADS 1
#define MAX_THREADS 64
#define MAX_PLY 60

#define SCORE_WIN 32000
#define MIN_MATE_SCORE (SCORE_WIN - MAX_PLY)

typedef struct PVar {
    int length;
    move line[MAX_PLY];
    uint64_t hashCodes[MAX_PLY];
} PVar;

#define MAX_REQUIRED_MOVES 32
typedef struct threadContext {
    //UCI Thread settings or info
    int maxDepth, seldepth, completedDepth, deepeningSkip;
    int hardMaxNodes, softMaxNodes, countedNodes;
    clock_t startTime, softEndTime, hardEndTime;
    move searchedMoves[MAX_REQUIRED_MOVES];
    uint8_t* abortFlag;

    //Necessary search information
    int16_t score;
    bitboard boardStack[MAX_PLY];
    move moveStack[MAX_PLY];
    repetitionVector repetitions;
    hashtable_tt* tt;
    PVar pv;
    move excludedMove[MAX_PLY];
    
    accumulator* accumulatorStack;
    uint8_t isAccClean[MAX_PLY];
    uint8_t lastCleanPly;
    accumulatorRefreshTable* refreshTable;
    
    //Improving heuristic
    int evalHistory[MAX_PLY];
    int8_t improving[MAX_PLY]; // 0 or 1
    int8_t worsening[MAX_PLY]; // >= 0

    //Killer heuristic
    move killerMoves[MAX_PLY][2];

    //History heuristic
    int16_t historyTable[2][6][64];
    int16_t captureHistoryTable[6][64][6];

    //Continuation heuristics
    move countermove[2][6][64];
    move followUpMove[2][6][64];

    //Correction History
    int16_t pawnCorrHist[2][CORRHIST_SIZE]; //[turn]
    int16_t nonPawnCorrHist[2][2][CORRHIST_SIZE]; //[color][turn]

    #ifdef SEARCHINFO
    uint64_t pvs_nodes;
    uint64_t qs_nodes;

    uint64_t tt_hits;
    uint64_t tt_cutoffs;
    uint64_t tt_misses;

    uint64_t quiescentSearchedMoves;
    uint64_t quiescentSearchedPositions;

    uint64_t pvsSearchedMoves;
    uint64_t pvsSearchedPositions;

    uint64_t singularSearches;
    uint64_t tripleExtensions;
    uint64_t doubleExtensions;
    uint64_t singleExtensions;
    uint64_t multicutPrunes;
    uint64_t negativeExtensions;
    uint64_t failedExtensions;

    uint64_t evaluations;

    uint64_t stable_reductions;
    uint64_t worsening_reductions;
    uint64_t tt_reductions;
    uint64_t rfp_prunes;
    uint64_t razoring_prunes;
    uint64_t nmp_prunes;
    #endif
} threadContext;

extern int initial_aspiration_margin;
extern float aspiration_margin_mult_factor;

extern int razoring_a;
extern int razoring_b;

extern int futility_margin;
extern int futility_depth_margin;

extern int reverse_futility_margin;
extern int reverse_futility_margin_improving;

extern float lmr_a;
extern float lmr_b;

extern float lmp_a;
extern float lmp_b;
extern float lmp_improving_a;
extern float lmp_improving_b;

extern int stable_eval_margin;

void initSearchTables();
uint64_t perft(bitboard* board, int depth, int verbose);
uint64_t iterator_perft(threadContext* context, int depth, int ply, int verbose);
int quiescentSearch(threadContext* context, int alpha, int beta, int ply, int pvNode);
int principalVariationSearch(threadContext* context, int alpha, int beta, int depth, int ply, PVar* myPV, int pvNode, int cutNode);
THREAD_RETURN calculateBestMove(THREAD_PARAM param);