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
    uint64_t king_w;
    
    uint64_t pawn_b;
    uint64_t knight_b;
    uint64_t bishop_b;
    uint64_t rook_b;
    uint64_t queen_b;
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

#endif