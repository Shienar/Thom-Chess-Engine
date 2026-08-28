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
#define WHITE_PAWN_DOUBLEPUSH_MASK(board) (((WHITE_PAWN_PUSH_MASK(board) & board_rank[2]) << 8)&(~board->pieces_all))
#define WHITE_PAWN_LEFTATTACKS(board)  (((board->pieces[WHITE_PAWN] & (~board_file[0])) << 7) & board->pieces_side[BLACK])
#define WHITE_PAWN_RIGHTATTACKS(board)  (((board->pieces[WHITE_PAWN] & (~board_file[7])) << 9) & board->pieces_side[BLACK])

#define BLACK_PAWN_PUSH_MASK(board) ((board->pieces[BLACK_PAWN] >> 8)&(~board->pieces_all))
#define BLACK_PAWN_DOUBLEPUSH_MASK(board) (((BLACK_PAWN_PUSH_MASK(board) & board_rank[5]) >> 8)&(~board->pieces_all))
#define BLACK_PAWN_LEFTATTACKS(board)  (((board->pieces[BLACK_PAWN] & (~board_file[7])) >> 7) & board->pieces_side[WHITE])
#define BLACK_PAWN_RIGHTATTACKS(board)  (((board->pieces[BLACK_PAWN] & (~board_file[0])) >> 9) & board->pieces_side[WHITE])

#define EN_PASSANT_ATTACKERS_WHITE(epMask, board) ((((epMask >> 7) & (~board_file[0])) | ((epMask >> 9) & (~board_file[7]))) & board->pieces[WHITE_PAWN])
#define EN_PASSANT_ATTACKERS_BLACK(epMask, board) ((((epMask << 7) & (~board_file[7])) | ((epMask << 9) & (~board_file[0]))) & board->pieces[BLACK_PAWN])

//A zero'd out move will have all values set to zero.
#define IS_VALID_MOVE(m) (m.startSquare != m.endSquare)
extern const int pieceValuesSEE[15];

int generateMoveList(move* movesList, bitboard* board, int capturesOnly);

#define TT_MOVE_SCORE 32000
#define PV_MOVE_SCORE 31999
#define CAPTURE_SCORE 17000 //+- min bound(-ish). Max SEE bonus is 1700 for capture-queen promotion. +-256 history bonus added
#define KILLER_1_SCORE 16399
#define KILLER_2_SCORE 16398
#define PROMOTION_SCORE 16385
#define MAX_HISTORY_SCORE 16384 //+- max bound
#define COUNTERMOVE_BONUS 6000 //added to history score
#define FOLLOWUPMOVE_BONUS 5000 //added to history score
moveIterator* create_move_iterator(searchThreadContext* context, int capturesOnly, int ply, move* pvMove, move* ttMove);
move* iterate_next_move(moveIterator* iter);
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

int movePiece(bitboard* board, move compactMove, repetitionVector* repetitions);

int moveFromStruct(bitboard* board, bitboard* newBoard, move m, repetitionVector* repetitions);
move getStructFromString(bitboard* board, char* str);

void applyNullMove(bitboard* board, bitboard* newBoard, repetitionVector* repetitions);

#endif