#ifndef ENGINESTRUCTS
#define ENGINESTRUCTS

#include "compatibility.h"
#include <stdint.h>
#include <limits.h>
#include <time.h>

#define clamp(originalValue, minClamp, maxClamp) _min(maxClamp, _max(minClamp, originalValue));

// Should always get assigned with makefile.
#ifndef PROJECT_CWD
#define PROJECT_CWD ""
#endif

#define singleBitMask(x) (1ull << (x))
#define MAX_MOVES 218

typedef struct move {
    int8_t startSquare;
    int8_t endSquare;
    int8_t piece;
    int8_t promoteTo;
    int8_t capturedPiece;
    int8_t capturedPieceSquare; //Not always the same as endsquare because of en passant
    
    int8_t prevEnPassantSquare;
    uint8_t previousMovesSinceLastChange;
    uint8_t prevFlags;
    uint16_t lastChangeIndex;
} move;

typedef struct moveIterator {
    move* moveList;
    int16_t* moveScores;
    uint8_t count;
    uint8_t visitedCount;
} moveIterator;

typedef struct table_entry_tt {
    uint64_t hashCode;
    float evaluation;
    uint8_t depth;
    int8_t nodeType; //PV-node = score is exact; All-node = score is upper bound; Cut-node = score is lower bound.
    move bestMove;
    uint8_t checkSum;
} table_entry_tt;

typedef struct hashtable_tt {
    table_entry_tt* array;
    size_t capacity;
} hashtable_tt;

//a1 = 0, h1 = 7
//...
//a8 = 56, h8 = 63
#define PIECE_COUNT 12
#define MAX_PLY 40
typedef struct bitboard {
    uint64_t pieces[PIECE_COUNT];

    uint64_t pieces_side[2];
    uint64_t pieces_all;

    uint8_t pieceArr[64];
    uint8_t kingSquare_b;
    uint8_t kingSquare_w;


    /**
     * flags&1 == canKingsideCastle_w
     * flags&2 == canQueensideCastle_w 
     * flags&4 == canKingsideCastle_b
     * flags&8 == canQueensideCastle_b
     * flags&16 == in_check_w
     * flags&32 == in_check_b
     */ 
    uint8_t flags;

    uint8_t turn;

    uint8_t victor;

    uint8_t movesSinceLastChange;

    move history[MAX_PLY];
    uint8_t historyIndex;
    uint64_t repetitionHashCodes[4096];
    uint16_t repetitionIndex;
    uint16_t lastChangeIndex;

    //A pawn can capture to this square.
    //[0,63] OR -1 if empty.
    int8_t enPassantSquare;

    //Other
    uint16_t halfMoveCount;

    uint64_t hashCode;
} bitboard;

typedef struct magic {
    uint64_t mask;
    uint64_t magic;
    uint64_t* attacks;
    int8_t shiftOffset;
} magic;

#define QA 255
#define QB 64

#define INPUT_BITS 1536
#define HALF_INPUT_BITS (INPUT_BITS / 2)
#define ACCUMULATOR_NODES 512
#define ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES / 2)

// 2x(768 -> 256) -> 1
typedef struct quantized_weights {
    int16_t weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    int16_t weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    int8_t weights2[ACCUMULATOR_NODES];
    int32_t weights2_bias;
} quantized_weights;
typedef struct training_weights {
    float weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    float weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    float weights2[ACCUMULATOR_NODES];
    float weights2_bias;
} training_weights;

typedef struct accumulator {
    uint64_t inputNodes[24];
    uint8_t accumulator[2][ACCUMULATOR_NODES_PER_SIDE]; 
    int16_t rawAccumulator[2][ACCUMULATOR_NODES_PER_SIDE]; //Unactivated values. Efficiently updateable.
} accumulator;

typedef struct PVar {
    int length;
    move line[MAX_PLY];
} PVar;

#define MAX_REQUIRED_MOVES 32
typedef struct searchThreadContext {
    int isPonder;
    int maxDepth, seldepth, completedDepth, deepeningSkip;
    int maxNodes, countedNodes;
    int score;
    clock_t startTime;
    bitboard* board;
    volatile clock_t* endTime;
    accumulator* accumulator;
    move searchedMoves[MAX_REQUIRED_MOVES]; //search only these at depth 1
    move killerMoves[MAX_PLY][2]; //Killer heuristic
    int16_t historyTable[2][6][64]; //History heuristic
    PVar pv;
} searchThreadContext;

#endif