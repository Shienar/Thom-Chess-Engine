#include "transposition/hash.h"
#include "board/bitboard.h"
#include <stdint.h>

/**
 * - Indexes 0-767 are for piece-square values.
 * - Index = 64 * piece type + square
 * - Index 768 = white can kingside castle
 * - Index 769 = white can queenside castle
 * - Index 770 = black can kingside castle
 * - Index 771 = black can queenside castle
 * - Index 772-779 = En passant file.
 *  - 772 + file in range 0-7
 * - Index 780 = is white's turn
 */
uint64_t zobrist_keys[781] = { 0 };

//https://en.wikipedia.org/wiki/Xorshift#xorshift*
uint64_t rng_xorshift64(uint64_t *seed)
{
	uint64_t x = *seed;

	x ^= x << 12;
	x ^= x >> 25;
	x ^= x << 27;
	*seed = x;
	return x * 0x2545F4914F6CDD1DULL;
}

uint64_t zobrist_piece_keys[PIECE_COUNT][64];
void init_zobrist_piece_keys(void)
{
	uint64_t zobrist_seed = 0x314023;

	for (int i = 0; i < 781; i++)
		zobrist_keys[i] = rng_xorshift64(&zobrist_seed);

	for (int piece = 0; piece < PIECE_COUNT; piece++)
		for (int square = 0; square < 64; square++)
			zobrist_piece_keys[piece][square] = zobrist_keys[64 * piece + square];
}

uint64_t get_ep_hash(bitboard *board)
{
	if (board->ep_square == NO_EP_SQUARE)
		return 0;

	int ep_square = board->ep_square;
	int ep_row = get_row(ep_square);
	uint64_t bordering_pawn_mask = 0;

	int captured_pawn_square = (ep_row == 5) ? ep_square - 8 : ep_square + 8;

	if (get_column(captured_pawn_square) > 0)
		bordering_pawn_mask |= (1ull << (captured_pawn_square - 1));
	if (get_column(captured_pawn_square) < 7)
		bordering_pawn_mask |= (1ull << (captured_pawn_square + 1));

	if (ep_row == 5) {
		if (bordering_pawn_mask & board->pieces[WHITE_PAWN])
			return zobrist_keys[772 + get_column(ep_square)];
	} else {
		if (bordering_pawn_mask & board->pieces[BLACK_PAWN])
			return zobrist_keys[772 + get_column(ep_square)];
	}

	return 0;
}

void generate_hash_code(bitboard *board)
{
	if (!board)
		return;

	board->hash_code = 0;
	board->pawn_hash = 0;
	board->nonpawn_hash[WHITE] = 0;
	board->nonpawn_hash[BLACK] = 0;

	int piece;
	uint64_t mask = board->pieces_all;

	while (mask) {
		int square = __builtin_ctzll(mask);

		piece = (board->piece_arr[square]);

		if (piece != EMPTY_PIECE) {
			board->hash_code ^= zobrist_piece_keys[piece][square];

			if (ISPAWN(piece))
				board->pawn_hash ^= zobrist_piece_keys[piece][square];
			else
				board->nonpawn_hash[COLOR(piece)] ^= zobrist_piece_keys[piece][square];
		}

		mask &= (mask - 1);
	}

	if (board->can_kingside_castle_w)
		board->hash_code ^= zobrist_keys[768];
	if (board->can_queenside_castle_w)
		board->hash_code ^= zobrist_keys[769];
	if (board->can_kingside_castle_b)
		board->hash_code ^= zobrist_keys[770];
	if (board->can_queenside_castle_b)
		board->hash_code ^= zobrist_keys[771];


	board->hash_code ^= get_ep_hash(board);

	if (ISWHITE(board->turn))
		board->hash_code ^= zobrist_keys[780];
}
