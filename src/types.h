#ifndef ENGINESTRUCTS
#define ENGINESTRUCTS

#include "compatibility.h"
#include <stdint.h>
#include <limits.h>
#include <time.h>

#define clamp(originalValue, minClamp, maxClamp) _min(maxClamp, _max(minClamp, originalValue));

#define singleBitMask(x) (1ull << (x))
#define MAX_MOVES 218

//Compact move contains the bare minimum move information.
typedef union move_c {
    uint16_t raw;
    struct {
        uint16_t startSquare : 6;
        uint16_t endSquare   : 6;
        uint16_t promoteTo   : 4;
    };
} move_c;

//Detailed move contains information necessary for unmoving. 
typedef union move_d {
    uint64_t raw;
    struct {
        uint64_t compactMove                    : 16;
        uint64_t piece                          :  4;
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

typedef struct PVar {
    int length;
    move_c line[MAX_PLY];
} PVar;

#define QA 255
#define QB 64
#define QA_RSHIFT 8
#define QB_RSHIFT 6

#define INPUT_BITS (2 * 768)
#define HALF_INPUT_BITS (INPUT_BITS / 2)
#define ACCUMULATOR_NODES 512
#define ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES / 2)
#define OUTPUT_BUCKETS 8

typedef struct __attribute__((aligned(64))) nnue_weights {
    int16_t weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    int16_t weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    int16_t weights2[OUTPUT_BUCKETS][ACCUMULATOR_NODES];
    int16_t weights2_bias[OUTPUT_BUCKETS];
} nnue_weights;

typedef struct accumulator {
    uint64_t inputNodes[2 * PIECE_COUNT];
    int16_t rawAccumulator[2][ACCUMULATOR_NODES_PER_SIDE]; //Unactivated values. Efficiently updateable.
} accumulator;

#define MAX_REQUIRED_MOVES 32
typedef struct searchThreadContext {
    //UCI Thread settings or info
    int maxDepth, seldepth, completedDepth, deepeningSkip;
    int hardMaxNodes, softMaxNodes, countedNodes;
    clock_t startTime, softEndTime, hardEndTime;
    move_c searchedMoves[MAX_REQUIRED_MOVES];
    uint8_t* abortFlag;

    //Necessary search information
    int16_t score;
    bitboard* board;
    hashtable_tt* tt;
    PVar pv;
    move_c excludedMove;
    
    accumulator* accumulator;
    
    //Improving heuristic
    int evalHistory[MAX_PLY];
    int8_t improving[MAX_PLY];

    //Killer heuristic
    move_c killerMoves[MAX_PLY][2];

    //History heuristic
    int16_t historyTable[2][6][64];

    //Continuation heuristics
    move_c countermove[64][64];
    move_c followUpMove[64][64];
} searchThreadContext;

#endif