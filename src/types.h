#ifndef ENGINESTRUCTS
#define ENGINESTRUCTS

#include "compatibility.h"
#include <stdint.h>
#include <limits.h>
#include <time.h>

#define clamp(originalValue, minClamp, maxClamp) _min(maxClamp, _max(minClamp, originalValue));

// Will get assigned by makefile in non-release builds.
#ifndef PROJECT_CWD
#define PROJECT_CWD ""
#endif

#define singleBitMask(x) (1ull << (x))
#define MAX_MOVES 218

//Compact move contains the bare minimum move information.
typedef union move_c {
    uint16_t raw;
    struct {
        uint16_t startSquare : 6;
        uint16_t endSquare : 6;
        uint16_t promoteTo: 4;
    };
} move_c;

//Detailed move contains information necessary for unmoving. 
typedef union move_d {
    uint64_t raw;
    uint16_t arr[4]; // Compare arr[0] to see if two moves' start/end square & piece are equal.
    struct {
        uint64_t startSquare                    :  6;
        uint64_t endSquare                      :  6;
        uint64_t piece                          :  4;
        uint64_t promoteTo                      :  4;
        uint64_t capturedPiece                  :  4;
        uint64_t prevEnPassantSquare            :  7;
        uint64_t previousMovesSinceLastChange   :  7;
        uint64_t prevFlags                      :  6;
        uint64_t lastChangeIndex                : 16;
    };
} move_d;

typedef struct moveIterator {
    move_c* moveList;
    int16_t* moveScores;
    uint8_t count;
    uint8_t visitedCount;
} moveIterator;

typedef struct table_entry_tt {
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
} table_entry_tt;

typedef struct hashtable_tt {
    table_entry_tt* array;
    size_t capacity;
} hashtable_tt;

//a1 = 0, h1 = 7
//...
//a8 = 56, h8 = 63
#define PIECE_COUNT 12
#define PIECE_TYPE_COUNT 6
#define MAX_PLY 40
typedef struct bitboard {
    uint64_t pieces[PIECE_COUNT];

    uint64_t pieces_side[2];
    uint64_t pieces_all;

    uint8_t pieceArr[64];
    uint8_t kingSquare[2];

    /**
     * flags&1 == canKingsideCastle_w
     * flags&2 == canQueensideCastle_w 
     * flags&4 == canKingsideCastle_b
     * flags&8 == canQueensideCastle_b
     * flags&16 == in_check_w
     * flags&32 == in_check_b
     * flags&64 == in book.
     */ 
    uint8_t flags;

    uint8_t turn;

    uint8_t movesSinceLastChange;

    move_d history[MAX_PLY];
    uint8_t historyIndex;
    uint64_t repetitionHashCodes[4096];
    uint16_t repetitionIndex;
    uint16_t lastChangeIndex;

    //A pawn can capture to this square.
    int8_t enPassantSquare;

    uint16_t halfMoveCount;

    uint64_t hashCode;
} bitboard;

typedef struct magic {
    uint64_t mask;
    uint64_t magic;
    uint64_t* attacks;
    int8_t shiftOffset;
} magic;

#ifdef NNUE
#define QA 255
#define QB 64
#define QA_RSHIFT 8
#define QB_RSHIFT 6
#define OUTPUT_SCALE_RSHIFT 4

#define KING_BUCKETS 10
#define BITS_PER_KING_BUCKET 768
#define OUTPUT_BUCKETS 8
#define BITBOARDS_PER_INPUT_SIDE (PIECE_COUNT * KING_BUCKETS)

#define INPUT_BITS (2 * BITS_PER_KING_BUCKET * KING_BUCKETS)
#define HALF_INPUT_BITS (INPUT_BITS / 2)
#define ACCUMULATOR_NODES 1024
#define ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES / 2)

typedef struct quantized_weights {
    int16_t weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    int16_t weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    int8_t weights2[OUTPUT_BUCKETS][ACCUMULATOR_NODES];
    int32_t weights2_bias[OUTPUT_BUCKETS];
} quantized_weights;
typedef struct training_weights {
    float weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    float weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    float weights2[OUTPUT_BUCKETS][ACCUMULATOR_NODES];
    float weights2_bias[OUTPUT_BUCKETS];
} training_weights;

typedef struct accumulator {
    uint64_t inputNodes[2 * BITBOARDS_PER_INPUT_SIDE];
    uint8_t accumulator[2][ACCUMULATOR_NODES_PER_SIDE]; 
    int16_t rawAccumulator[2][ACCUMULATOR_NODES_PER_SIDE]; //Unactivated values. Efficiently updateable.
} accumulator;

typedef struct accumulatorRefreshTable {
    bitboard* boards[2][64];
    accumulator accumulators[2][64];
    uint8_t initialized[2][64];
} accumulatorRefreshTable;
#endif

typedef struct PVar {
    int length;
    move_c line[MAX_PLY];
} PVar;

#define MAX_REQUIRED_MOVES 32
typedef struct searchThreadContext {
    //UCI Thread settings or info
    int isPonder;
    int maxDepth, seldepth, completedDepth, deepeningSkip;
    int maxNodes, countedNodes;
    clock_t startTime;
    volatile clock_t* endTime;
    move_c searchedMoves[MAX_REQUIRED_MOVES];
    move_c excludedMove;

    //Necessary search information
    int16_t score;
    bitboard* board;
    #ifdef NNUE
    accumulator* accumulator;
    accumulatorRefreshTable* refreshTable;
    #endif
    hashtable_tt* tt;
    PVar pv;
    
    //Improving heuristic
    int evalHistory[MAX_PLY];
    int8_t improving[MAX_PLY];

    //Killer heuristic
    move_c killerMoves[MAX_PLY][2];

    //History heuristic
    int16_t historyTable[2][6][64];
} searchThreadContext;

#endif