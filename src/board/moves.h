#pragma once

#include "board/board.h"

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

#define WHITE_PAWN_PUSH_MASK(board) ((board->pieces[WHITE_PAWN] << 8)&(~board->pieces_all))
#define WHITE_PAWN_DOUBLEPUSH_MASK(board) (((WHITE_PAWN_PUSH_MASK(board) & board_rank[2]) << 8)&(~board->pieces_all))
#define WHITE_PAWN_LEFTATTACKS(board)  (((board->pieces[WHITE_PAWN] & (~board_file[0])) << 7) & board->pieces_side[BLACK])
#define WHITE_PAWN_RIGHTATTACKS(board)  (((board->pieces[WHITE_PAWN] & (~board_file[7])) << 9) & board->pieces_side[BLACK])

#define BLACK_PAWN_PUSH_MASK(board) ((board->pieces[BLACK_PAWN] >> 8)&(~board->pieces_all))
#define BLACK_PAWN_DOUBLEPUSH_MASK(board) (((BLACK_PAWN_PUSH_MASK(board) & board_rank[5]) >> 8)&(~board->pieces_all))
#define BLACK_PAWN_LEFTATTACKS(board)  (((board->pieces[BLACK_PAWN] & (~board_file[7])) >> 7) & board->pieces_side[WHITE])
#define BLACK_PAWN_RIGHTATTACKS(board)  (((board->pieces[BLACK_PAWN] & (~board_file[0])) >> 9) & board->pieces_side[WHITE])

#define EN_PASSANT_ATTACKERS_WHITE(ep_mask, board) ((((ep_mask >> 7) & (~board_file[0])) | ((ep_mask >> 9) & (~board_file[7]))) & board->pieces[WHITE_PAWN])
#define EN_PASSANT_ATTACKERS_BLACK(ep_mask, board) ((((ep_mask << 7) & (~board_file[7])) | ((ep_mask << 9) & (~board_file[0]))) & board->pieces[BLACK_PAWN])

int generate_move_list(move * moves_list, bitboard * board, int captures_only);

extern const int piece_values_see[15];
extern char *promote_symbols[16];

#define COUNTERMOVE_BONUS 3000
#define FOLLOWUPMOVE_BONUS 2000
move_iterator *create_move_iterator(struct thread_context *context, int captures_only, int ply, move *tt_move);
move *iterate_next_move(move_iterator *iter);

int is_threatened(bitboard *board, int square, int defending_color);

//Includes illegal moves (in regards to check)
uint64_t pyrrhic_pawn_attacks(int square, int color);
uint64_t bishop_moves(uint64_t ally_pieces, uint64_t enemy_pieces, int square);
uint64_t knight_moves(uint64_t ally_pieces, int square);
uint64_t rook_moves(uint64_t ally_pieces, uint64_t enemy_pieces, int square);
uint64_t queen_moves(uint64_t ally_pieces, uint64_t enemy_pieces, int square);
uint64_t pyrrhic_king_attacks(int square);
uint64_t king_moves(bitboard *board, int square, int color);

int move_piece(bitboard *board, move compact_move, repetition_vector *repetitions);

int move_from_struct(bitboard *board, bitboard *new_board, move m, repetition_vector *repetitions);
move get_struct_from_string(bitboard *board, char *str);

void apply_null_move(bitboard *board, bitboard *new_board, repetition_vector *repetitions);
