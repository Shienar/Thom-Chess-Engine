#ifndef ENGINESTRUCTS
#define ENGINESTRUCTS

#include "compatibility.h"
#include <stdint.h>
#include <limits.h>
#include <time.h>

#define clamp(originalValue, minClamp, maxClamp) _min(maxClamp, _max(minClamp, originalValue));

#define singleBitMask(x) (1ull << (x))
#define MAX_MOVES 218

typedef union move {
    uint16_t raw;
    struct {
        uint16_t startSquare : 6;
        uint16_t endSquare   : 6;
        uint16_t promoteTo   : 4;
    };
} move;

typedef struct moveIterator {
    move* moveList;
    int16_t* moveScores;
    uint8_t count;
    uint8_t visitedCount;
} moveIterator;

typedef struct tt_entry {
    uint64_t hashCode;
    union {
        uint64_t data;
        struct {
            uint16_t age;
            uint16_t bestMove;
            int16_t evaluation;
            uint8_t depth;
            uint8_t nodeType;
        };
    };
} tt_entry;

typedef struct hashtable_tt {
    tt_entry* array;
    size_t capacity;
    size_t usedSlots;
} hashtable_tt;

typedef struct {
    uint64_t* hashCodes;
    int capacity;
} repetitionVector;

//a1 = 0, h1 = 7
//...
//a8 = 56, h8 = 63
#define PIECE_COUNT 12
#define PIECE_TYPE_COUNT 6
#define MAX_PLY 40
typedef struct bitboard {
    uint64_t hashCode;
    uint64_t pawnHash;
    uint64_t pieces[PIECE_COUNT];
    uint64_t pieces_side[2];
    uint64_t pieces_all;

    uint16_t halfMoveCount;
    uint16_t lastChangeIndex;

    uint8_t pieceArr[64];
    uint8_t kingSquare[2];
    uint8_t halfmoveClock;
    int8_t enPassantSquare;

    uint8_t canKingsideCastle_w  : 1;
    uint8_t canQueensideCastle_w : 1;
    uint8_t canKingsideCastle_b  : 1;
    uint8_t canQueensideCastle_b : 1;
    uint8_t in_check             : 1;
    uint8_t in_book              : 1;
    uint8_t turn                 : 1;
} bitboard;

typedef struct magic {
    uint64_t mask;
    uint64_t magic;
    uint64_t* attacks;
    int8_t shiftOffset;
} magic;

typedef struct PVar {
    int length;
    move line[MAX_PLY];
    uint64_t hashCodes[MAX_PLY];
} PVar;

#define QA 255
#define QB 64
#define QA_RSHIFT 8
#define QB_RSHIFT 6

#define INPUT_BITS (2 * BITS_PER_KING_BUCKET * KING_BUCKETS)
#define HALF_INPUT_BITS (INPUT_BITS / 2)
#define ACCUMULATOR_NODES 512
#define ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES / 2)

#define KING_BUCKETS 10
#define BITS_PER_KING_BUCKET 768
#define BITBOARDS_PER_INPUT_SIDE (PIECE_COUNT * KING_BUCKETS)

#define OUTPUT_BUCKETS 8

typedef struct __attribute__((aligned(64))) nnue_weights {
    int16_t weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    int16_t weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    int16_t weights2[OUTPUT_BUCKETS][ACCUMULATOR_NODES];
    int16_t weights2_bias[OUTPUT_BUCKETS];
} nnue_weights;

typedef struct accumulator {
    int16_t rawValues[2][ACCUMULATOR_NODES_PER_SIDE];
} accumulator;

/**
 * First half of indices are on king buckets on files a-d.
 * Second half of indices are on king buckets on files e-h.
 * 
 * Full refreshes won't be necessary when moving within a 
 * king bucket on the same half of board.
 */
typedef struct accumulatorRefreshTable {
    bitboard boards[2][2 * KING_BUCKETS];
    accumulator accumulators[2][2 * KING_BUCKETS];
    uint8_t initialized[2][2 * KING_BUCKETS];
} accumulatorRefreshTable;

#define MAX_REQUIRED_MOVES 32
#define CORRHIST_SIZE 16384
#define MAX_CORRHIST_VAL 16384
typedef struct searchThreadContext {
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
    accumulatorRefreshTable* refreshTable;
    
    //Improving heuristic
    int evalHistory[MAX_PLY];
    int8_t improving[MAX_PLY];

    //Killer heuristic
    move killerMoves[MAX_PLY][2];

    //History heuristic
    int16_t historyTable[2][6][64];
    int16_t captureHistoryTable[6][64][6];

    //Continuation heuristics
    move countermove[2][6][64];
    move followUpMove[2][6][64];

    //Correction History
    int16_t pawnCorrHist[2][CORRHIST_SIZE];
} searchThreadContext;

#endif