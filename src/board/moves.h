#ifndef MOVES
#define MOVES

#include "types.h"
#include "board/magic.h"

#define GET_ALL_MOVES 0
#define GET_CAPTURES_AND_CHECKS 1 //Direct checks only. Discovered checks would be too expensive to compute.
#define GET_CAPTURES 2
#define GET_WINNING_CAPTURES 3

#define THREAT_TYPE_NONE 0
#define THREAT_TYPE_PAWN 1
#define THREAT_TYPE_KNIGHT 2
#define THREAT_TYPE_BISHOPQUEEN 3
#define THREAT_TYPE_ROOKQUEEN 4
#define THREAT_TYPE_KING 5

#define WHITE_PAWN_PUSH_MASK(board) ((board->pieces[WHITE_PAWN] << 8)&(~board->pieces_all))
#define WHITE_PAWN_DOUBLEPUSH_MASK(board) (((WHITE_PAWN_PUSH_MASK(board) & RANK_3) << 8)&(~board->pieces_all))
#define WHITE_PAWN_LEFTATTACKS(board)  (((board->pieces[WHITE_PAWN] & (~FILE_A)) << 7) & board->pieces_side[BLACK])
#define WHITE_PAWN_RIGHTATTACKS(board)  (((board->pieces[WHITE_PAWN] & (~FILE_H)) << 9) & board->pieces_side[BLACK])

#define BLACK_PAWN_PUSH_MASK(board) ((board->pieces[BLACK_PAWN] >> 8)&(~board->pieces_all))
#define BLACK_PAWN_DOUBLEPUSH_MASK(board) (((BLACK_PAWN_PUSH_MASK(board) & RANK_6) >> 8)&(~board->pieces_all))
#define BLACK_PAWN_LEFTATTACKS(board)  (((board->pieces[BLACK_PAWN] & (~FILE_H)) >> 7) & board->pieces_side[WHITE])
#define BLACK_PAWN_RIGHTATTACKS(board)  (((board->pieces[BLACK_PAWN] & (~FILE_A)) >> 9) & board->pieces_side[WHITE])

#define EN_PASSANT_ATTACKERS_WHITE(epMask, board) ((((epMask >> 7) & (~FILE_A)) | ((epMask >> 9) & (~FILE_H))) & board->pieces[WHITE_PAWN])
#define EN_PASSANT_ATTACKERS_BLACK(epMask, board) ((((epMask << 7) & (~FILE_H)) | ((epMask << 9) & (~FILE_A))) & board->pieces[BLACK_PAWN])

//A zero'd out move will have all values set to zero.
#define IS_VALID_MOVE(m) (m.startSquare != m.endSquare)

int generateMoveList(move_c* movesList, bitboard* board, int capturesOnly);

#define PV_MOVE_SCORE 30000
#define TT_MOVE_SCORE 20000
#define KILLER_1_SCORE 10000
#define KILLER_2_SCORE 9500
#define MAX_HISTORY_SCORE 5000 //+- max bound
#define CAPTURE_SCORE 5000 //+- min bound, used as an exact score for quiet promotions.
moveIterator* create_move_iterator(bitboard* board, int capturesOnly, move_c* pvMove, move_c* ttMove, move_c* requiredMoves, move_c* killerMoves, int16_t history[2][6][64]);
move_c* iterate_next_move(moveIterator* iter);
void destroy_move_iterator(moveIterator* iter);


int isThreatened(bitboard* board, int square, int defendingColor);

//Includes illegal moves (in regards to check)
uint64_t pyrrhicPawnAttacks(int square, int color);
uint64_t bishopMoves(uint64_t allyPieces, uint64_t enemyPieces, int square);
uint64_t knightMoves(uint64_t allyPieces, int square);
uint64_t rookMoves(uint64_t allyPieces, uint64_t enemyPieces, int square);
uint64_t queenMoves(uint64_t allyPieces, uint64_t enemyPieces, int square);
uint64_t pyrrhicKingAttacks(int square);
uint64_t kingMoves(bitboard* board, int square, int color);

int movePiece(bitboard *board, move_c compactMove);

//Assigns captured piece.
int moveFromStruct(bitboard* board, move_c m);
move_c getStructFromString(bitboard* board, char* str);

//2 Calls result in an unchanged board state.
void applyNullMove(bitboard* board);

move_d unmove(bitboard *board);

#endif