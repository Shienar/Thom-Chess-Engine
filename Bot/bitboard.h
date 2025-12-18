#ifndef BITBOARD_H

#define BITBOARD_H

#include <stdint.h>

#define WHITE 0x10
#define BLACK 0x20

#define ISWHITE(piece) ((piece&WHITE) == WHITE)
#define ISBLACK(piece) ((piece&BLACK) == BLACK)

#define PAWN 0x01
#define KNIGHT 0x02
#define BISHOP 0x03
#define ROOK 0x04
#define QUEEN 0x05
#define KING 0x06

#define ISPAWN(piece) ((piece&0xF) == PAWN)
#define ISKNIGHT(piece) ((piece&0xF) == KNIGHT)
#define ISBISHOP(piece) ((piece&0xF) == BISHOP)
#define ISROOK(piece) ((piece&0xF) == ROOK)
#define ISQUEEN(piece) ((piece&0xF) == QUEEN)
#define ISKING(piece) ((piece&0xF) == KING)

#define REMOVE = 0x100
#define SHOULDREMOVE(piece) (piece&REMOVE == REMOVE)

typedef struct move {
    int startSquare;
    int endSquare;
    int piece;
    int promoteTo;
    int capturedPiece;
    int capturedPieceSquare; //Not always the same as endsquare because of en passant
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

    int canQueensideCastle_b;
    int canKingsideCastle_b;
    int canQueensideCastle_w;
    int canKingsideCastle_w;

    //Check
    int in_check_w;
    int in_check_b;

    //Turn
    int turn;

    //Checkmate
    int victor;

    //History
    move* moveStackTop;
} bitboard;

#define columnNames "abcdefgh"

int getColumn(int square);
int getRow(int square);
char getColumnChar(int x, int isSquare);
void getSquareName(int square, char* target);
int getSquareNumber(char* squareName);
int findPieceOnSquare(bitboard* board, int square);

void board_reset(bitboard* board);
void board_clear(bitboard* board);

void board_clear_square(bitboard* board, int square, int pieceType);
void board_set(bitboard* board, int piece, int square);

void piece_print(char boardArray[8][9], uint64_t piece, char printChar);
void board_print(bitboard* board, int printValues);
void values_print(bitboard* board);
void bitmask_print(uint64_t mask, char fill);

int moves_push(bitboard* board, move* m);
move* moves_pop(bitboard* board);
move* createMove(int startSquare, int endSquare, int promoteTo, int piece, int capturedPiece, int capturedPieceSquare);

#endif