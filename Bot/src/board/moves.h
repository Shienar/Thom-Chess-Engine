#ifndef MOVES
#define MOVES

#include "../structs.h"
#include "magic.h"

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

#define NOT_A_FILE 0xfefefefefefefefe
#define NOT_H_FILE 0x7f7f7f7f7f7f7f7f
#define RANK_3 0x0000000000FF0000
#define RANK_6 0x0000FF0000000000

#define WHITE_PAWN_PUSH_MASK(board) ((board->pawn_w << 8)&(~board->pieces_all))
#define WHITE_PAWN_DOUBLEPUSH_MASK(board) (((WHITE_PAWN_PUSH_MASK(board) & RANK_3) << 8)&(~board->pieces_all))
#define WHITE_PAWN_LEFTATTACKS(board)  (((board->pawn_w & NOT_A_FILE) << 7) & board->pieces_b)
#define WHITE_PAWN_RIGHTATTACKS(board)  (((board->pawn_w & NOT_H_FILE) << 9) & board->pieces_b)

#define BLACK_PAWN_PUSH_MASK(board) ((board->pawn_b >> 8)&(~board->pieces_all))
#define BLACK_PAWN_DOUBLEPUSH_MASK(board) (((BLACK_PAWN_PUSH_MASK(board) & RANK_6) >> 8)&(~board->pieces_all))
#define BLACK_PAWN_LEFTATTACKS(board)  (((board->pawn_b & NOT_H_FILE) >> 7) & board->pieces_w)
#define BLACK_PAWN_RIGHTATTACKS(board)  (((board->pawn_b & NOT_A_FILE) >> 9) & board->pieces_w)

#define EN_PASSANT_ATTACKERS_WHITE(epMask, board) ((((epMask >> 7) & NOT_A_FILE) |  ((epMask >> 9) & NOT_H_FILE)) & board->pawn_w)
#define EN_PASSANT_ATTACKERS_BLACK(epMask, board) ((((epMask << 7) & NOT_H_FILE) |  ((epMask << 9) & NOT_A_FILE)) & board->pawn_b)

int generateMoveList(move* movesList, bitboard* board, int capturesOnly);

int isThreatened(bitboard* board, int square, int squareColor);

//Includes illegal moves (in regards to check)
uint64_t pyrrhicPawnAttacks(int square, int color);
void generatePawnMoves(move* moveList, int* size, bitboard* board, int capturesOnly);
uint64_t bishopMoves(uint64_t allyPieces, uint64_t enemyPieces, int square);
uint64_t knightMoves(uint64_t allyPieces, int square);
uint64_t rookMoves(uint64_t allyPieces, uint64_t enemyPieces, int square);
uint64_t queenMoves(uint64_t allyPieces, uint64_t enemyPieces, int square);
uint64_t pyrrhicKingAttacks(int square);
uint64_t kingMoves(bitboard* board, int square, int color);

//Returns 0 on success, -1 on fail.
int movePawn(bitboard *board, int startSquare, int endSquare, int color, int promoteTo);
int moveKnight(bitboard *board, int startSquare, int endSquare, int color);
int moveBishop(bitboard *board, int startSquare, int endSquare, int color);
int moveRook(bitboard *board, int startSquare, int endSquare, int color);
int moveQueen(bitboard *board, int startSquare, int endSquare, int color);
int moveKing(bitboard *board, int startSquare, int endSquare, int color);

int movePiece(bitboard *board, int startSquare, int endSquare, int piece, int promoteTo);
int moveFromStruct(bitboard* board, move m);
int moveFromString(bitboard* board, char* str);

moveEntry* unmove(bitboard *board);

#endif