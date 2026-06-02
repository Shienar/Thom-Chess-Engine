#ifndef MOVES
#define MOVES

#include "../types.h"
#include "magic.h"

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

#define WHITE_PAWN_PUSH_MASK(board) ((board->pieces[WHITE_PAWN] << 8)&(~board->pieces_all))
#define WHITE_PAWN_DOUBLEPUSH_MASK(board) (((WHITE_PAWN_PUSH_MASK(board) & RANK_3) << 8)&(~board->pieces_all))
#define WHITE_PAWN_LEFTATTACKS(board)  (((board->pieces[WHITE_PAWN] & NOT_A_FILE) << 7) & board->pieces_side[BLACK])
#define WHITE_PAWN_RIGHTATTACKS(board)  (((board->pieces[WHITE_PAWN] & NOT_H_FILE) << 9) & board->pieces_side[BLACK])

#define BLACK_PAWN_PUSH_MASK(board) ((board->pieces[BLACK_PAWN] >> 8)&(~board->pieces_all))
#define BLACK_PAWN_DOUBLEPUSH_MASK(board) (((BLACK_PAWN_PUSH_MASK(board) & RANK_6) >> 8)&(~board->pieces_all))
#define BLACK_PAWN_LEFTATTACKS(board)  (((board->pieces[BLACK_PAWN] & NOT_H_FILE) >> 7) & board->pieces_side[WHITE])
#define BLACK_PAWN_RIGHTATTACKS(board)  (((board->pieces[BLACK_PAWN] & NOT_A_FILE) >> 9) & board->pieces_side[WHITE])

#define EN_PASSANT_ATTACKERS_WHITE(epMask, board) ((((epMask >> 7) & NOT_A_FILE) |  ((epMask >> 9) & NOT_H_FILE)) & board->pieces[WHITE_PAWN])
#define EN_PASSANT_ATTACKERS_BLACK(epMask, board) ((((epMask << 7) & NOT_H_FILE) |  ((epMask << 9) & NOT_A_FILE)) & board->pieces[BLACK_PAWN])

//A zero'd out move will have all values set to zero.
#define IS_VALID_MOVE(m) (m.startSquare != m.endSquare)

int generateMoveList(move* movesList, bitboard* board, int capturesOnly);

int isThreatened(bitboard* board, int square, int squareColor);

//Includes illegal moves (in regards to check)
uint64_t pyrrhicPawnAttacks(int square, int color);
uint64_t bishopMoves(uint64_t allyPieces, uint64_t enemyPieces, int square);
uint64_t knightMoves(uint64_t allyPieces, int square);
uint64_t rookMoves(uint64_t allyPieces, uint64_t enemyPieces, int square);
uint64_t queenMoves(uint64_t allyPieces, uint64_t enemyPieces, int square);
uint64_t pyrrhicKingAttacks(int square);
uint64_t kingMoves(bitboard* board, int square, int color);

int movePiece(bitboard *board, move* m);
int moveFromStruct(bitboard* board, move m);
move getStructFromString(bitboard* board, char* str);

move unmove(bitboard *board);

#endif