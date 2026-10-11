#ifndef MOVES
#define MOVES

#include "types.h"
#include "board/magic.h"

#define GET_ALL_MOVES 0
#define GET_NOISY_MOVES 1 //Captures + Direct Checks + Promotions
#define GET_QUIET_MOVES 2

#define ITER_STAGE_TT 0
#define ITER_STAGE_GET_NOISY 1
#define ITER_STAGE_GOOD_NOISY 2
#define ITER_STAGE_KILLER_1 3
#define ITER_STAGE_KILLER_2 4
#define ITER_STAGE_GET_QUIETS 5
#define ITER_STAGE_QUIETS 6
#define ITER_STAGE_BAD_NOISY 7
#define ITER_STAGE_DONE 8
#define ITER_STAGE_REQUIRED 9

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

int generateMoveList(move* movesList, bitboard* board, int capturesOnly);

extern int pieceValuesSEE[15];

#define TT_MOVE_SCORE 32000
#define CAPTURE_SCORE 17000 //+- min bound(-ish). receives see + +-256 from history. Promotions are treated as captures
#define MAX_HISTORY_SCORE 16384 //+- max bound
#define COUNTERMOVE_BONUS 3000 //added to history score
#define FOLLOWUPMOVE_BONUS 2000 //added to history score
#define EXCLUDED_SCORE INT16_MIN
moveIterator* create_move_iterator(threadContext* context, int capturesOnly, int ply, move* ttMove);
move* iterate_next_move(moveIterator* iter);

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