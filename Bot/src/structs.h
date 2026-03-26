#ifndef ENGINESTRUCTS
#define ENGINESTRUCTS

#include <stdint.h>
#include <time.h>

typedef struct move {
    int startSquare;
    int endSquare;
    int piece;
    int promoteTo;
    int capturedPiece;
    int capturedPieceSquare; //Not always the same as endsquare because of en passant
    
    int prevEnPassantSquare;
    int previousMovesSinceLastChange;
    
    //Flags from the previous board state.
    int flags; 
    struct move* nextMove;
} move;

typedef struct table_entry_pos {
    uint64_t hashCode;
    int count;
} table_entry_pos;

typedef struct hashtable_pos {
    table_entry_pos* array;
    size_t capacity;
    size_t size;
} hashtable_pos;

typedef struct table_entry_tt {
    uint64_t hashCode;
    clock_t age;
    double evaluation;
    int evaluationDepth;
    int nodeType; //PV-node = score is exact; All-node = score is upper bound; Cut-node = score is lower bound.
    move bestMove;
    uint8_t checkSum;
} table_entry_tt;

typedef struct hashtable_tt {
    table_entry_tt* array;
    size_t capacity;
    size_t size;
} hashtable_tt;

//a1 = 0, h1 = 7
//a2 = 8, h2 = 15
//a3 = 16, h3 = 23
//a4 = 24, h4 = 31
//a5 = 32, h5 = 39
//a6 = 40, h6 = 47
//a7 = 48, h7 = 55
//a8 = 56, h8 = 63
typedef struct bitboard {
    uint64_t pawn_w;
    uint64_t knight_w;
    uint64_t bishop_w;
    uint64_t rook_w;
    uint64_t queen_w;
    
    uint64_t pawn_b;
    uint64_t knight_b;
    uint64_t bishop_b;
    uint64_t rook_b;
    uint64_t queen_b;

    uint64_t king_w;
    uint64_t king_b;

    uint64_t pieces_w;
    uint64_t pieces_b;
    uint64_t pieces_all;

    //Constantly referenced for check checking. 0-63
    int kingSquare_b;
    int kingSquare_w;

    /**
     * flags&1 == canKingsideCastle_w
     * flags&2 == canQueensideCastle_w 
     * flags&4 == canKingsideCastle_b
     * flags&8 == canQueensideCastle_b
     * flags&16 == in_check_w
     * flags&32 == in_check_b
     */ 
    int flags;

    //Turn
    int turn;

    //Checkmate
    int victor;

    //50 move rule
    int movesSinceLastChange;

    //History
    move* moveStackTop;
    hashtable_pos* ht;

    //A pawn can capture to this square.
    //[0,63] OR -1 if empty.
    int enPassantSquare;

    //Other
    int halfMoveCount;
} bitboard;

typedef struct polyglot_book_entry {
    uint64_t hashKey;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
} polyglot_book_entry;


#define INPUT_BITS 81920
#define HALF_INPUT_BITS 40960
#define ACCUMULATOR_NODES_PER_SIDE 256
#define SECOND_HIDDEN_LAYER_NODES 32
#define THIRD_HIDDEN_LAYER_NODES 32
#define OUTPUT_LAYER_NODES 1

/**
 * Weights
 * 
 * Each index in the inputNodes array is a bitboard.
 * To find the bitboard of PIECE while COLOR's
 * king is on SQUARE, use the following formula.
 * 
 * i = (640 * ISBLACK(COLOR)) + (10 * SQUARE) + PIECE
 *  - PIECE
 *      - Ally Pawn = 0
 *      - Ally Knight = 1
 *      - Ally Bishop = 2
 *      - Ally Rook = 3
 *      - Ally Queen = 4
 *      - Enemy Pawn = 5
 *      - Enemy Knight = 6
 *      - Enemy Bishop = 7
 *      - Enemy Rook = 8
 *      - Enemy Queen = 9
 */
typedef struct network_weights_training {
    float weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    float weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    float weights2[2 * ACCUMULATOR_NODES_PER_SIDE][SECOND_HIDDEN_LAYER_NODES];
    float weights2_bias[SECOND_HIDDEN_LAYER_NODES];
    float weights3[SECOND_HIDDEN_LAYER_NODES][THIRD_HIDDEN_LAYER_NODES];
    float weights3_bias[THIRD_HIDDEN_LAYER_NODES];
    float weights4[THIRD_HIDDEN_LAYER_NODES];
    float weights4_bias;

    //Saved for dequantization.
    float scalingFactor;
} network_weights_training;
typedef struct network_weights_playing {
    int8_t weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    int8_t weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    int8_t weights2[2 * ACCUMULATOR_NODES_PER_SIDE][SECOND_HIDDEN_LAYER_NODES];
    int8_t weights2_bias[SECOND_HIDDEN_LAYER_NODES];
    int8_t weights3[SECOND_HIDDEN_LAYER_NODES][THIRD_HIDDEN_LAYER_NODES];
    int8_t weights3_bias[THIRD_HIDDEN_LAYER_NODES];
    int8_t weights4[THIRD_HIDDEN_LAYER_NODES];
    int8_t weights4_bias;
} network_weights_playing;

typedef struct accumulator_training {
    //Incrementally updates.
    uint64_t inputNodes[1280];
    float accumulator[2][ACCUMULATOR_NODES_PER_SIDE]; //[0][i] = white; [1][i] = black;
    
    //Saved for use in backpropagation.
    float h2[SECOND_HIDDEN_LAYER_NODES];
    float h3[THIRD_HIDDEN_LAYER_NODES];
    float outputNode;
} accumulator_training;

typedef struct accumulator_playing {
    uint64_t inputNodes[1280];
    int8_t accumulator[2][ACCUMULATOR_NODES_PER_SIDE]; //[0][i] = white; [1][i] = black;
} accumulator_playing;

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
typedef struct accumulator_playing_refreshTable {
    bitboard* boards[2][64];
    accumulator_playing accumulators[64];
} accumulator_playing_refreshTable;
typedef struct accumulator_training_refreshTable {
    bitboard* boards[2][64];
    accumulator_training accumulators[64];
} accumulator_training_refreshTable;

typedef struct network_training_data {
    bitboard board;
    float evaluation;
} network_training_data;

#endif