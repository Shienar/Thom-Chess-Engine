#ifndef MOVES
#define MOVES

#include "structs.h"
#include "hashtable.h"

//Questioned piece is this direction from selected king.
//Assume a1 is bottomleft
#define PIN_TYPE_NONE 0
#define PIN_TYPE_LEFT 1
#define PIN_TYPE_RIGHT 2
#define PIN_TYPE_BELOW 3
#define PIN_TYPE_ABOVE 4
#define PIN_TYPE_UPLEFT 5
#define PIN_TYPE_UPRIGHT 6
#define PIN_TYPE_DOWNLEFT 7
#define PIN_TYPE_DOWNRIGHT 8

#define THREAT_TYPE_NONE 0
#define THREAT_TYPE_PAWN 1
#define THREAT_TYPE_KNIGHT 2
#define THREAT_TYPE_BISHOPQUEEN 3
#define THREAT_TYPE_ROOKQUEEN 4
#define THREAT_TYPE_KING 5

//Legal moves.
//Lists are null-terminated.
move** generateMoveList(bitboard* board, int capturesOnly);
move** generatePieceMoves(bitboard* board, int piece, int square, int color, int capturesOnly);
void freeMoveList(move** moveList);

int isPinned(bitboard* board, int questionedSquare, int kingSquare, int kingColor);
int isThreatened(bitboard* board, int square, int squareColor);

//Includes illegal moves (in regards to check)
uint64_t pawnMoves(bitboard* board, int square, int color);
uint64_t bishopMoves(bitboard* board, int square, int color);
uint64_t knightMoves(bitboard* board, int square, int color);
uint64_t rookMoves(bitboard* board, int square, int color);
uint64_t queenMoves(bitboard* board, int square, int color);
uint64_t kingMoves(bitboard* board, int square, int color, int ignoreThreats);

//Returns 0 on success, -1 on fail.
int movePawn(bitboard *board, int startSquare, int endSquare, int color, int promoteTo);
int moveKnight(bitboard *board, int startSquare, int endSquare, int color);
int moveBishop(bitboard *board, int startSquare, int endSquare, int color);
int moveRook(bitboard *board, int startSquare, int endSquare, int color);
int moveQueen(bitboard *board, int startSquare, int endSquare, int color);
int moveKing(bitboard *board, int startSquare, int endSquare, int color);

int movePiece(bitboard *board, int startSquare, int endSquare, int piece, int promoteTo);
int moveFromStruct(bitboard* board, move* m);
int moveFromString(bitboard* board, char* str);

move* unmove(bitboard *board);

#endif