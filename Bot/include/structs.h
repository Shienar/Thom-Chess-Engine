#ifndef ENGINESTRUCTS
#define ENGINESTRUCTS

#include <stdint.h>
#include <windows.h>

typedef struct table_entry {
    char* key;
    double count;
} table_entry;

typedef struct hashtable {
    table_entry* array;
    size_t capacity;
    size_t size;
} hashtable;

typedef struct move {
    int startSquare;
    int endSquare;
    int piece;
    int promoteTo;
    int capturedPiece;
    int capturedPieceSquare; //Not always the same as endsquare because of en passant
    
    //Flags from the previous board state.
    int flags; 
    struct move* nextMove;
} move;

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

    //History
    move* moveStackTop;
    struct hashtable* ht;
} bitboard;


typedef struct {
    double pawnPieceWeights[64];
    double knightPieceWeights[64];
    double bishopPieceWeights[64];
    double rookPieceWeights[64];
    double queenPieceWeights[64];
    double kingPieceWeights[64];

    double pawnWeight;
    double knightWeight;
    double bishopWeight;
    double rookWeight;
    double queenWeight;
    double kingWeight;

    hashtable* transpositionTable;
    move* pvTable;
} engine;

// Windows multithreading. TODO: Revisit this
typedef struct {
    HANDLE parentWaitSemaphore;
    HANDLE threadCountSemaphore;
    bitboard* board;
    engine* engine;
    int depth;
    double* returnValue;
} threadParam;

#endif