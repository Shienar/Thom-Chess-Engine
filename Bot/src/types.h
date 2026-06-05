#ifndef ENGINESTRUCTS
#define ENGINESTRUCTS

#include <stdint.h>
#include <limits.h>
#include <time.h>

//Avoid the 1ull << x calls.
#define singleBitMask(x) (1ull << (x))

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
    int8_t repetitionIndex;
} move;

typedef struct table_entry_tt {
    uint64_t hashCode;
    float evaluation;
    uint8_t depth;
    int8_t nodeType; //PV-node = score is exact; All-node = score is upper bound; Cut-node = score is lower bound.
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
#define MAX_PLY 20
#define MAX_PV_SIZE ((MAX_PLY * (MAX_PLY + 1)) / 2)
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
    uint64_t repetitionHashCodes[128];
    uint8_t repetitionIndex;

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

#define INPUT_BITS 24576
#define HALF_INPUT_BITS 12288
#define ACCUMULATOR_NODES 512
#define ACCUMULATOR_NODES_PER_SIDE 256
#define SECOND_HIDDEN_LAYER_NODES 32
#define THIRD_HIDDEN_LAYER_NODES 32
#define OUTPUT_BUCKETS 8

#define KING_BUCKETS 16
#define BITBOARDS_PER_INPUT_SIDE 192 //1 bitboard for each of the twelve pieces for each king bucket.

extern int kingBuckets[64];
extern int kingBucketMap[KING_BUCKETS];

/**
 * Weights are stored as w[output][input] to make SIMD possible for the hidden layers.
 * 
 * The sparse input layer is stored as w[input][output] instead since we are jumping
 * to the few active inputs. Output layer weights are w[bucket][output], not w[input][output].
 * 
 * Output bucket: 0-7, calculated with ((piece count - 1) / 4)
 */
typedef struct network_weights {
    float weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    float weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    float weights2[SECOND_HIDDEN_LAYER_NODES][ACCUMULATOR_NODES];
    float weights2_bias[SECOND_HIDDEN_LAYER_NODES];
    float weights3[THIRD_HIDDEN_LAYER_NODES][SECOND_HIDDEN_LAYER_NODES];
    float weights3_bias[THIRD_HIDDEN_LAYER_NODES];
    float weights4[OUTPUT_BUCKETS][THIRD_HIDDEN_LAYER_NODES];
    float weights4_bias[OUTPUT_BUCKETS];
} network_weights;

typedef struct accumulator {
    uint64_t inputNodes[384];
    float accumulator[2][ACCUMULATOR_NODES_PER_SIDE]; 
    float rawAccumulator[2][ACCUMULATOR_NODES_PER_SIDE]; //Unactivated values. Efficiently updateable.
} accumulator;

/**
 * Each accumulator is truly two entries, one for each half
 * of the accumulator.
 * 
 * boards[0] = white
 * boards[1] = black
 * 
 * White accumulators:
 *  - accumulators[i].accumulator[0]
 * Black accumulators:
 *  - accumulators[i].accumulator[1]
 */
typedef struct accumulatorRefreshTable {
    bitboard* boards[2][KING_BUCKETS];
    accumulator accumulators[KING_BUCKETS];
} accumulatorRefreshTable;

#define MAX_REQUIRED_MOVES 32
typedef struct searchThreadContext {
    int isPonder;
    int maxDepth;
    int reachedDepth;
    int maxNodes;
    int countedNodes;
    float score;
    int deepeningSkip;
    clock_t startTime;
    bitboard* board;
    volatile clock_t* endTime;
    accumulator* accumulator;
    accumulatorRefreshTable* accumulatorTable;
    move searchedMoves[MAX_REQUIRED_MOVES]; //search only these at depth 1
    move pvTable[MAX_PV_SIZE];
} searchThreadContext;

#endif