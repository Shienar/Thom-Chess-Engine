#include "analyze/hce/hce.h"
#include "board/magic.h"
#include "board/moves.h"
#include "analyze/search.h"

/**
 * For simplicity, this represents white's view of the board when
 * viewed in a text editor.
 *
 * White Piece square tables receive a rank-wise FLIP_SQUARE. (xor 56)
 * Black piece square tables receive a column-wise MIRROR_SQUARE (xor 7)
 * Black column-based parameters receive the column-wise MIRROR_SQUARE.
 * Black row-based parameters also received MIRROR_SQUARE, since the math works out there as well given row=[0, 7].
 *
 * The tuner is intended to bootstrap an eval from the following start:
 * K = 3.612
 * eval_parameters hce_params = {
 *	.generic_piece_values = {
 *		P(   100,  100), P(  300,  300), P(  300,  300), P(  500,  500), P(  900, 900), P(	0,	0)
 *	},
 *	.tempo = P(20, 20)
 * };
 *
 */
eval_parameters hce_params = {
	.generic_piece_values = {
		P(   55,  147), P(  269,  346), P(  307,  372), P(  376,  643), P(  810, 1200), P(	0,	0)
	},
	.raw_piece_tables = {
		[PAWN / 2] = {
			P(	0,	0), P(	0,	0), P(	0,	0), P(	0,	0), P(	0,	0), P(	0,	0), P(	0,	0), P(	0,	0),
			P(   12,   67), P(   53,   80), P(   63,   64), P(   85,   24), P(   70,   30), P(   44,   59), P(	6,   93), P(  -17,   85),
			P(   12,	8), P(   17,   39), P(   42,   -8), P(   29,  -38), P(   38,  -39), P(   49,  -23), P(   11,   33), P(	9,   10),
			P(   -6,  -15), P(   -2,	5), P(	4,  -29), P(	1,  -40), P(	8,  -43), P(   -4,  -29), P(   -8,	0), P(  -10,  -17),
			P(  -21,  -27), P(  -17,	4), P(  -15,  -26), P(   -7,  -31), P(   -9,  -30), P(  -15,  -24), P(  -19,   -1), P(  -22,  -26),
			P(  -25,  -31), P(  -27,	2), P(  -24,  -20), P(  -23,  -16), P(  -24,  -16), P(  -28,  -15), P(  -35,	3), P(  -31,  -26),
			P(  -24,  -25), P(   -9,   12), P(  -20,   -6), P(  -25,   -2), P(  -27,	3), P(  -18,   -5), P(  -14,	9), P(  -27,  -22),
			P(	0,	0), P(	0,	0), P(	0,	0), P(	0,	0), P(	0,	0), P(	0,	0), P(	0,	0), P(	0,	0)
		},
		[KNIGHT / 2] = {
			P( -122,  -66), P( -120,   -5), P(  -54,	2), P(  -21,	0), P(   13,   -2), P(  -33,   -6), P(  -65,  -15), P( -109,  -84),
			P(  -32,  -25), P(  -20,   -9), P(   25,   -9), P(	5,   -5), P(	4,  -11), P(   35,  -20), P(  -23,   -1), P(  -21,  -32),
			P(   -5,  -14), P(   22,   -6), P(   39,   13), P(   46,   10), P(   55,	9), P(   46,	5), P(   26,   -4), P(   -6,  -20),
			P(	5,   -3), P(   19,   13), P(   45,   22), P(   39,   27), P(   37,   27), P(   44,   20), P(   19,   13), P(	7,   -9),
			P(	5,	7), P(   19,   18), P(   30,   35), P(   31,   38), P(   35,   39), P(   27,   32), P(   29,   14), P(	1,	4),
			P(  -11,   -5), P(   13,	7), P(   17,   14), P(   26,   32), P(   30,   25), P(   15,   15), P(	9,	5), P(  -13,   -6),
			P(   -7,   -9), P(  -10,   -5), P(	3,	6), P(   10,	8), P(   10,	5), P(   -1,	8), P(  -14,   -2), P(   -7,  -10),
			P(  -45,  -16), P(  -19,  -17), P(  -10,  -10), P(   -4,	0), P(   -5,	0), P(   -3,  -10), P(  -19,  -21), P(  -44,  -18)
		},
		[BISHOP / 2] = {
			P(  -40,	5), P(  -44,	5), P(  -54,	0), P(  -76,   14), P(  -75,	7), P(  -51,	5), P(  -33,	8), P(  -34,   -3),
			P(  -11,   -5), P(	7,	2), P(   11,   -1), P(   10,   -7), P(	1,   -2), P(	3,   -4), P(   10,   -2), P(   -7,   -9),
			P(   19,	4), P(   24,	5), P(   30,	6), P(   27,	1), P(   30,   -7), P(   36,	3), P(   35,   -3), P(   21,	0),
			P(   -1,	3), P(	0,	9), P(   21,	7), P(   29,   10), P(   29,   16), P(   18,	7), P(	7,	3), P(  -11,	3),
			P(   -2,   -9), P(   -1,	8), P(   -4,   12), P(   24,   11), P(   17,   10), P(	3,	8), P(   -8,	5), P(	2,   -9),
			P(	6,   -8), P(	7,	0), P(	9,	6), P(   -3,   11), P(	5,	8), P(	2,	6), P(   10,	2), P(   -6,   -8),
			P(   -3,  -16), P(   16,  -13), P(   10,  -18), P(	3,   -3), P(   -7,   -1), P(	7,   -8), P(	8,  -10), P(	1,   -7),
			P(	8,  -20), P(   12,  -28), P(   -7,   -4), P(   -8,   -4), P(   -6,   -2), P(  -16,	0), P(	1,   18), P(  -12,  -16)
		},
		[ROOK / 2] = {
			P(   24,	9), P(   10,	8), P(	7,   19), P(   12,	9), P(	9,   13), P(   21,   13), P(	6,   19), P(   15,   11),
			P(   12,	9), P(   13,   16), P(   29,   17), P(   26,   12), P(   23,   13), P(   25,   13), P(	3,   17), P(   13,   10),
			P(   -6,	9), P(   19,	6), P(   14,	7), P(	6,	4), P(   13,	4), P(	6,	5), P(   28,	5), P(	0,	7),
			P(  -10,	8), P(	1,	3), P(	0,	2), P(   -2,	2), P(   -7,	2), P(   -5,	8), P(   -1,	4), P(   -7,	6),
			P(  -15,	0), P(  -12,	1), P(  -17,	2), P(   -8,   -2), P(  -14,	0), P(  -22,	1), P(  -17,   -1), P(  -22,	2),
			P(  -16,   -9), P(   -4,  -13), P(  -11,  -11), P(   -9,  -10), P(  -14,   -7), P(  -11,  -10), P(	1,  -21), P(  -12,   -9),
			P(  -20,   -7), P(  -10,  -15), P(   -6,  -11), P(   -8,  -10), P(   -9,  -10), P(   -9,  -12), P(   -4,  -16), P(  -20,  -12),
			P(   -6,  -12), P(	0,  -17), P(   -2,  -11), P(	4,  -17), P(	4,  -18), P(	0,  -15), P(   -2,  -14), P(   -3,  -11)
		},
		[QUEEN / 2] = {
			P(  -14,   -9), P(   10,  -11), P(   -9,   25), P(	4,   12), P(	4,   11), P(  -11,   25), P(   -5,   -9), P(  -20,	7),
			P(   22,   -7), P(   -5,   17), P(	2,   36), P(  -35,   71), P(  -29,   63), P(  -13,   41), P(  -12,   19), P(   12,	4),
			P(	5,	0), P(	3,   17), P(   -3,   32), P(   -7,   33), P(  -11,   48), P(   -2,   34), P(   21,  -14), P(   10,   -1),
			P(  -12,   11), P(  -11,   14), P(   -8,   19), P(   -8,   34), P(   -9,   35), P(  -10,   25), P(   -8,   22), P(   -8,   12),
			P(	1,   -6), P(	6,	6), P(   -6,   18), P(   -5,   27), P(	1,   18), P(   -5,	4), P(	3,	4), P(   -1,	1),
			P(	3,  -17), P(	8,   -8), P(	3,	3), P(   -1,	1), P(	2,	1), P(	3,	1), P(	7,   -5), P(	5,  -16),
			P(	8,  -44), P(   12,  -44), P(   10,  -33), P(   12,  -16), P(   10,  -15), P(   14,  -34), P(   14,  -42), P(	8,  -32),
			P(	3,  -44), P(	6,  -61), P(	7,  -59), P(   14,  -54), P(   11,  -44), P(	1,  -44), P(	7,  -50), P(   -6,  -31)
		},
		[KING / 2] = {
			P(  118, -115), P(  123,  -48), P(   45,  -11), P(  -22,	1), P(  -24,	5), P(   39,  -27), P(  -19,  -21), P(   49,  -97),
			P(	6,   -8), P(   27,   37), P(	4,   51), P(   57,   35), P(   35,   36), P(  -20,   44), P(  -11,   35), P( -100,	5),
			P(  -34,	9), P(   29,   45), P(   62,   58), P(   12,   67), P(  -12,   72), P(	5,   60), P(   26,   41), P(  -82,   13),
			P(  -88,	9), P(  -45,   42), P(  -40,   65), P(  -52,   77), P(  -74,   77), P(  -47,   61), P(  -51,   42), P( -110,   12),
			P( -103,	2), P(  -45,   22), P(  -51,   47), P(  -63,   63), P(  -76,   68), P(  -49,   45), P(  -69,   29), P( -113,	3),
			P(  -30,  -16), P(	7,	1), P(  -23,   27), P(  -32,   39), P(  -24,   37), P(  -24,   24), P(	0,	0), P(  -32,  -19),
			P(   74,  -77), P(   70,  -54), P(   54,  -41), P(   47,  -29), P(   46,  -30), P(   47,  -38), P(   60,  -46), P(   64,  -72),
			P(   62, -111), P(   65,  -81), P(   46,  -68), P(   67,  -76), P(   57,  -71), P(   45,  -65), P(   62,  -76), P(   59, -109)
		}
	},
	.knight_mobility_bonus = {
		P(  -23,  -20), P(   -9,   -7), P(   -2,	0), P(	2,	0), P(	7,	1), P(	7,	7), P(	7,	7), P(	7,	7),
		P(	7,	7)
	},
	.bishop_mobility_bonus = {
		P(  -26,  -54), P(  -18,  -33), P(  -12,  -25), P(  -10,  -16), P(   -5,   -5), P(	0,	7), P(	5,	8), P(	7,   13),
		P(	8,   18), P(	9,   18), P(	9,   18), P(   11,   18), P(   11,   18), P(   11,   18)
	},
	.rook_mobility_bonus = {
		P(  -17,  -39), P(   -8,  -23), P(   -4,  -20), P(   -2,  -15), P(   -2,  -13), P(   -1,   -4), P(	1,   -2), P(	3,	1),
		P(	3,	9), P(	3,   13), P(	4,   14), P(	6,   16), P(	6,   20), P(	6,   22), P(	6,   22)
	},
	.queen_mobility_bonus = {
		P(  -27, -114), P(   -5, -114), P(   -5, -114), P(   -5,  -82), P(   -4,  -62), P(   -2,  -53), P(	2,  -44), P(	2,  -32),
		P(	2,  -18), P(	2,  -12), P(	2,   -2), P(	2,	2), P(	2,	8), P(	2,   14), P(	2,   20), P(	2,   24),
		P(	2,   30), P(	2,   36), P(	2,   43), P(	2,   45), P(	3,   49), P(	3,   50), P(	3,   50), P(	3,   51),
		P(	3,   51), P(	3,   58), P(	3,   58), P(	3,   59)
	},
	.virtual_mobility_bonus = {
		P(   60,   51), P(   60,   23), P(   60,   18), P(   60,	6), P(   58,	6), P(   57,	6), P(   52,	5), P(   48,	2),
		P(   45,	2), P(   38,	2), P(   36,	2), P(   32,	2), P(   27,	2), P(   19,	2), P(   10,	2), P(   -2,	2),
		P(  -19,	2), P(  -32,	2), P(  -43,	2), P(  -48,   -2), P(  -53,   -6), P(  -55,   -9), P(  -66,  -12), P(  -66,  -17),
		P(  -66,  -19), P(  -72,  -25), P(  -72,  -29), P(  -72,  -29)
	},
	.minor_pawn_cover = P(	8,	3),
	.passed_pawn_bonus = {
		P(	0,	0), P(   -1,  -16), P(	1,  -13), P(	6,	2), P(   18,   31), P(   22,   89), P(   22,   89), P(	0,	0)
	},
	.connected_pawn_bonus = {
		P(	0,	0), P(   11,	9), P(	7,	3), P(	9,	5), P(	6,   11), P(   20,   20), P(  120,	5), P(	0,	0)
	},
	.neighbor_pawn_bonus = {
		P(	0,	0), P(	0,	0), P(	8,	4), P(   12,	9), P(   22,   25), P(   53,   94), P(  -45,  212), P(	0,	0)
	},
	.doubled_pawn_bonus = {
		P(	1,  -30), P(   12,  -30), P(   10,  -22), P(   16,  -16), P(   11,   -9), P(   11,  -23), P(   14,  -30), P(	4,  -32)
	},
	.isolated_pawn_bonus = {
		P(   -3,   18), P(   -7,   -9), P(   -9,   -1), P(  -13,   -8), P(  -13,   -8), P(  -12,   -1), P(   -6,   -7), P(   -2,   15)
	},
	.knight_outpost_bonus = P(   33,   12),
	.bishop_pair_bonus = P(   19,   62),
	.bad_bishop_bonus = P(   -2,   -5),
	.open_rook_file_bonus = { P(	8,   10), P(   30,	5) },
	.connected_rook_bonus = { P(   -4,	8), P(   11,	1) },
	.connected_queen_bonus = { P(   -7,   41), P(	4,   33), P(	7,	7) },
	.king_shield_bonus = {
		P(   31,  -26), P(   25,  -20), P(   29,  -16), P(   -2,   -9), P(   12,  -13), P(   10,   -9), P(   22,  -17), P(   32,  -26)
	},
	.king_storm_bonus = {
		P(	0,	0), P(   16,	1), P(	7,	9), P(   13,   -1), P(	7,	2), P(   15,   -4), P(   12,   -2), P(	0,	0)
	},
	.open_king_file = { P(   -9,	6), P(   -3,	3) },
	.king_safety = {
		P(	0,	1), P(	0,	1), P(	5,	1), P(	5,	1), P(	5,	1), P(	5,	1), P(	5,	1), P(	9,	1), P(   10,	1), P(   13,	1),
		P(   13,	1), P(   22,	1), P(   22,	1), P(   30,	1), P(   30,	1), P(   30,	1), P(   35,	1), P(   39,	1), P(   48,	1), P(   55,	1),
		P(   55,	1), P(   65,	1), P(   65,	1), P(   74,	1), P(   75,	1), P(   75,	1), P(   77,	1), P(   90,	1), P(   98,	1), P(  115,	1),
		P(  115,	1), P(  115,	1), P(  126,	1), P(  138,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1),
		P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1),
		P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1),
		P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1),
		P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1),
		P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1),
		P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1), P(  173,	1)
	},
	.tempo = P(   19,   22),
};

phased_eval piece_bonus_table[PIECE_COUNT][64] = {0};
uint64_t king_shield_mask[2][COLUMN_COUNT];
uint64_t king_storm_mask[COLUMN_COUNT];
uint64_t king_zone[2][64];
int32_t gamephase_piece_values[PIECE_COUNT] = {0, 0, 1, 1, 1, 1, 2, 2, 4, 4, 0, 0};
int attack_weight[8] = {0, 0, 50, 75, 88, 94, 97, 99};

int init_hce = 0;
void init_hce_tables(void)
{
	if (init_hce)
		return;
	init_hce = 1;

	//PSQT
	for (int piece = 0; piece < PIECE_COUNT; piece += 2) {
		for (int square = 0; square < 64; square++) {
			eval_add(&piece_bonus_table[piece][square], hce_params.raw_piece_tables[piece / 2][FLIP_SQUARE(square)]);
			eval_add(&piece_bonus_table[piece][square], hce_params.generic_piece_values[piece / 2]);

			eval_add(&piece_bonus_table[piece + 1][square], hce_params.raw_piece_tables[piece / 2][MIRROR_SQUARE(square)]);
			eval_add(&piece_bonus_table[piece + 1][square], hce_params.generic_piece_values[piece / 2]);
		}
	}

	for (int column = 0; column < 8; column++) {
		for (int column_offset = -1; column_offset <= 1; column_offset++) {
			int target_column = column + column_offset;

			if (target_column >= 0 && target_column <= 7) {
				// Ranks 2 & 3
				king_shield_mask[WHITE][column] |= (1ull << (1 * 8 + target_column));
				king_shield_mask[WHITE][column] |= (1ull << (2 * 8 + target_column));

				//Ranks 7 & 6
				king_shield_mask[BLACK][column] |= (1ull << (6 * 8 + target_column));
				king_shield_mask[BLACK][column] |= (1ull << (5 * 8 + target_column));
			}
		}

		king_storm_mask[column] = bordering_files[column] | board_file[column];
	}

	for (int sq = 0; sq < 64; sq++) {
		king_zone[WHITE][sq] = (1ull << (sq)) | king_attacks[sq];
		king_zone[BLACK][sq] = (1ull << (sq)) | king_attacks[sq];

		if (sq < 16)
			king_zone[WHITE][sq] |= king_attacks[sq + 8];

		if (sq > 47)
			king_zone[BLACK][sq] |= king_attacks[sq - 8];
	}
}

void evaluate_pawns(bitboard *board, phased_eval *score)
{
	phased_eval eval = P(0, 0);

	uint64_t mask = board->pieces[WHITE_PAWN];
	int protected_count = __builtin_popcountll((mask >> 8) & (board->pieces[WHITE_BISHOP] | board->pieces[WHITE_KNIGHT]));

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/square value
		eval_add(&eval, piece_bonus_table[WHITE_PAWN][sq]);

		//Passed Pawns
		if ((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
			eval_add(&eval, hce_params.passed_pawn_bonus[row]);

		//Doubled pawns
		if (__builtin_popcountll(board->pieces[WHITE_PAWN] & board_file[column]) > 1)
			eval_add(&eval, hce_params.doubled_pawn_bonus[row]);

		uint64_t bordering_mask = board->pieces[WHITE_PAWN] & bordering_files[column];
		//Isolated Pawns
		if (!bordering_mask)
			eval_add(&eval, hce_params.isolated_pawn_bonus[row]);
		//connected pawns
		else if (bordering_mask & (board_rank[row - 1] | board_rank[row + 1]))
			eval_add(&eval, hce_params.connected_pawn_bonus[row]);

		//Neighboring pawns
		int neighbors = __builtin_popcountll(bordering_mask & board_rank[row]);

		if (neighbors && row > 1)
			eval_madd(&eval, hce_params.neighbor_pawn_bonus[row], neighbors);

		mask &= mask - 1;
	}

	mask = board->pieces[BLACK_PAWN];

	protected_count -= __builtin_popcountll((mask << 8) & (board->pieces[BLACK_BISHOP] | board->pieces[BLACK_KNIGHT]));

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		int mirrored_column = MIRROR_SQUARE(column);
		int mirrored_row = MIRROR_SQUARE(row);

		//piece/square value
		eval_sub(&eval, piece_bonus_table[BLACK_PAWN][sq]);

		//Passed Pawns
		if ((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
			eval_sub(&eval, hce_params.passed_pawn_bonus[mirrored_row]);

		//Doubled pawns
		if (__builtin_popcountll(board->pieces[BLACK_PAWN] & board_file[column]) > 1)
			eval_sub(&eval, hce_params.doubled_pawn_bonus[mirrored_column]);

		uint64_t bordering_mask = board->pieces[BLACK_PAWN] & bordering_files[column];
		//Isolated Pawns
		if (!bordering_mask)
			eval_sub(&eval, hce_params.isolated_pawn_bonus[mirrored_column]);
		//connected pawns
		else if (bordering_mask & (board_rank[row - 1] | board_rank[row + 1]))
			eval_sub(&eval, hce_params.connected_pawn_bonus[mirrored_row]);

		//Neighboring pawns
		int neighbors = -1 * __builtin_popcountll(bordering_mask & board_rank[row]);

		if (neighbors && mirrored_row > 1)
			eval_madd(&eval, hce_params.neighbor_pawn_bonus[mirrored_row], neighbors);

		mask &= mask - 1;
	}

	eval_madd(&eval, hce_params.minor_pawn_cover, protected_count);

	eval_add(score, eval);
}

void evaluate_knights(bitboard *board, phased_eval *score, eval_context *context)
{
	phased_eval eval = P(0, 0);

	uint64_t mask = board->pieces[WHITE_KNIGHT];

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/square value
		eval_add(&eval, piece_bonus_table[WHITE_KNIGHT][sq]);

		//mobility
		uint64_t moves = knight_moves(board->pieces_side[WHITE], sq);
		int move_count = __builtin_popcountll(moves);

		eval_add(&eval, hce_params.knight_mobility_bonus[move_count]);

		//king safety
		context->attack_weight[WHITE] += KNIGHT_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[BLACK]);

		//outpost
		if (row >= 4 && row <= 6 &&
			board->pieces[WHITE_PAWN] && board_rank[row - 1] &&
			(bordering_files[column] & board->pieces[BLACK_PAWN] & (~((1ull << (sq + 7)) - 1))) == 0)
				eval_add(&eval, hce_params.knight_outpost_bonus);

		mask &= mask - 1;
	}

	mask = board->pieces[BLACK_KNIGHT];
	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/square value
		eval_sub(&eval, piece_bonus_table[BLACK_KNIGHT][sq]);

		//mobility
		uint64_t moves = knight_moves(board->pieces_side[BLACK], sq);
		int move_count = __builtin_popcountll(moves);

		eval_sub(&eval, hce_params.knight_mobility_bonus[move_count]);

		//king safety
		context->attack_weight[BLACK] += KNIGHT_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[WHITE]);

		//outpost
		if (row >= 1 && row <= 3 &&
		   board->pieces[BLACK_PAWN] && board_rank[row + 1] &&
		   (bordering_files[column] & board->pieces[WHITE_PAWN] & (((1ull << (sq - 6)) - 1))) == 0)
				eval_sub(&eval, hce_params.knight_outpost_bonus);

		mask &= mask - 1;
	}

	eval_add(score, eval);
}

void evaluate_bishops(bitboard *board, phased_eval *score, eval_context *context)
{
	phased_eval eval = P(0, 0);

	uint64_t mask = board->pieces[WHITE_BISHOP];

	//bad bishop-pawns
	int bad_pawns = 0;

	if (mask & LIGHT_SQUARES)
		bad_pawns += __builtin_popcountll(board->pieces[WHITE_PAWN] & LIGHT_SQUARES);
	if (mask & DARK_SQUARES)
		bad_pawns += __builtin_popcountll(board->pieces[WHITE_PAWN] & DARK_SQUARES);

	//bishop pair
	if (__builtin_popcountll(mask) >= 2)
		eval_add(&eval, hce_params.bishop_pair_bonus);

	while (mask) {
		int sq = __builtin_ctzll(mask);

		//piece/square value
		eval_add(&eval, piece_bonus_table[WHITE_BISHOP][sq]);

		//mobility
		uint64_t moves = bishop_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int move_count = __builtin_popcountll(moves);

		eval_add(&eval, hce_params.bishop_mobility_bonus[move_count]);

		//king safety
		context->attack_weight[WHITE] += BISHOP_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[BLACK]);

		mask &= mask - 1;
	}

	mask = board->pieces[BLACK_BISHOP];

	//bad bishop-pawns
	if (mask & LIGHT_SQUARES)
		bad_pawns -= __builtin_popcountll(board->pieces[BLACK_PAWN] & LIGHT_SQUARES);
	if (mask & DARK_SQUARES)
		bad_pawns -= __builtin_popcountll(board->pieces[BLACK_PAWN] & DARK_SQUARES);

	//bishop pair
	if (__builtin_popcountll(mask) >= 2)
		eval_sub(&eval, hce_params.bishop_pair_bonus);

	while (mask) {
		int sq = __builtin_ctzll(mask);

		//piece/sq value
		eval_sub(&eval, piece_bonus_table[BLACK_BISHOP][sq]);

		//mobility
		uint64_t moves = bishop_moves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int move_count = __builtin_popcountll(moves);

		eval_sub(&eval, hce_params.bishop_mobility_bonus[move_count]);

		//king safety
		context->attack_weight[BLACK] += BISHOP_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[WHITE]);

		mask &= mask - 1;
	}

	eval_madd(&eval, hce_params.bad_bishop_bonus, bad_pawns);

	eval_add(score, eval);
}

void evaluate_rooks(bitboard *board, phased_eval *score, eval_context *context)
{
	phased_eval eval = P(0, 0);
	int connected_rooks_by_row = 0;
	int connected_rooks_by_column = 0;

	uint64_t mask = board->pieces[WHITE_ROOK];

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/sq value
		eval_add(&eval, piece_bonus_table[WHITE_ROOK][sq]);

		//open rook file
		if ((board->pieces[WHITE_PAWN] & board_file[column]) == 0) {
			if ((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
				eval_add(&eval, hce_params.open_rook_file_bonus[OPEN_FILE]);
			else
				eval_add(&eval, hce_params.open_rook_file_bonus[SEMI_OPEN_FILE]);
		}

		//mobility
		uint64_t moves = rook_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int move_count = __builtin_popcountll(moves);

		eval_add(&eval, hce_params.rook_mobility_bonus[move_count]);

		//king safety
		context->attack_weight[WHITE] += ROOK_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[BLACK]);

		//rook rams
		uint64_t connections = rook_moves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		uint64_t connected_rooks = connections & board->pieces[WHITE_ROOK];

		connected_rooks_by_row += __builtin_popcountll(connected_rooks & board_rank[row]);
		connected_rooks_by_column += __builtin_popcountll(connected_rooks & board_file[column]);

		mask &= mask - 1;
	}

	mask = board->pieces[BLACK_ROOK];
	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/sq value
		eval_sub(&eval, piece_bonus_table[BLACK_ROOK][sq]);

		//open rook file
		if ((board->pieces[BLACK_PAWN] & board_file[column]) == 0) {
			if ((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
				eval_sub(&eval, hce_params.open_rook_file_bonus[OPEN_FILE]);
			else
				eval_sub(&eval, hce_params.open_rook_file_bonus[SEMI_OPEN_FILE]);
		}

		//mobility
		uint64_t moves = rook_moves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int move_count = __builtin_popcountll(moves);

		eval_sub(&eval, hce_params.rook_mobility_bonus[move_count]);

		//king safety
		context->attack_weight[BLACK] += ROOK_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[WHITE]);

		//rook rams
		uint64_t connections = rook_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		uint64_t connected_rooks = connections & board->pieces[BLACK_ROOK];

		connected_rooks_by_row -= __builtin_popcountll(connected_rooks & board_rank[row]);
		connected_rooks_by_column -= __builtin_popcountll(connected_rooks & board_file[column]);

		mask &= mask - 1;
	}

	eval_madd(&eval, hce_params.connected_rook_bonus[CONNECTED_ROW], connected_rooks_by_row);
	eval_madd(&eval, hce_params.connected_rook_bonus[CONNECTED_COLUMN], connected_rooks_by_column);

	eval_add(score, eval);
}

void evaluate_queens(bitboard *board, phased_eval *score, eval_context *context)
{
	phased_eval eval = P(0, 0);
	int connected_sliders_by_row = 0;
	int connected_sliders_by_column = 0;
	int connected_sliders_by_diagonal = 0;

	uint64_t mask = board->pieces[WHITE_QUEEN];

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/square value
		eval_add(&eval, piece_bonus_table[WHITE_QUEEN][sq]);

		//mobility
		uint64_t moves = queen_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int move_count = __builtin_popcountll(moves);

		eval_add(&eval, hce_params.queen_mobility_bonus[move_count]);

		//king safety
		context->attack_weight[WHITE] += QUEEN_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[BLACK]);

		//queen & slider rams
		uint64_t rook_connections = rook_moves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);

		rook_connections &= (board->pieces[WHITE_QUEEN] | board->pieces[WHITE_ROOK]);
		connected_sliders_by_row += __builtin_popcountll(rook_connections & board_rank[row]);
		connected_sliders_by_column += __builtin_popcountll(rook_connections & board_file[column]);

		uint64_t bishop_connections = bishop_moves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);

		bishop_connections &= (board->pieces[WHITE_QUEEN] | board->pieces[WHITE_BISHOP]);
		connected_sliders_by_diagonal += __builtin_popcountll(bishop_connections);

		mask &= mask - 1;
	}

	mask = board->pieces[BLACK_QUEEN];
	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/square value
		eval_sub(&eval, piece_bonus_table[BLACK_QUEEN][sq]);

		//mobility
		uint64_t moves = queen_moves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int move_count = __builtin_popcountll(moves);

		eval_sub(&eval, hce_params.queen_mobility_bonus[move_count]);

		//king safety
		context->attack_weight[BLACK] += QUEEN_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[WHITE]);

		//queen & slider rams
		uint64_t rook_connections = rook_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);

		rook_connections &= (board->pieces[BLACK_QUEEN] | board->pieces[BLACK_ROOK]);
		connected_sliders_by_row -= __builtin_popcountll(rook_connections & board_rank[row]);
		connected_sliders_by_column -= __builtin_popcountll(rook_connections & board_file[column]);

		uint64_t bishop_connections = bishop_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);

		bishop_connections &= (board->pieces[BLACK_QUEEN] | board->pieces[BLACK_BISHOP]);
		connected_sliders_by_diagonal -= __builtin_popcountll(bishop_connections);

		mask &= mask - 1;
	}

	eval_madd(&eval, hce_params.connected_queen_bonus[CONNECTED_ROW], connected_sliders_by_row);
	eval_madd(&eval, hce_params.connected_queen_bonus[CONNECTED_COLUMN], connected_sliders_by_column);
	eval_madd(&eval, hce_params.connected_queen_bonus[CONNECTED_DIAGONAL], connected_sliders_by_diagonal);

	eval_add(score, eval);
}

void evaluate_kings(bitboard *board, phased_eval *score, eval_context *context)
{
	phased_eval eval = P(0, 0);

	int semi_open_file_count = 0;
	int open_file_count = 0;

	uint64_t mask = board->pieces[WHITE_KING];

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int row = get_row(sq);
		int column = get_column(sq);

		//piece/square value
		eval_add(&eval, piece_bonus_table[WHITE_KING][sq]);

		//virtual mobility
		uint64_t virtual_moves = queen_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtual_move_count = __builtin_popcountll(virtual_moves);

		eval_add(&eval, hce_params.virtual_mobility_bonus[virtual_move_count]);

		if (row <= 1) {
			//pawn shield
			int pawn_shield_count = __builtin_popcountll(king_shield_mask[WHITE][column] & board->pieces[WHITE_PAWN]);

			eval_madd(&eval, hce_params.king_shield_bonus[column], pawn_shield_count);

			//pawn storm
			for (uint64_t storm_mask = king_storm_mask[column] & board->pieces[BLACK_PAWN]; storm_mask > 0; storm_mask &= storm_mask - 1)
				eval_add(&eval, hce_params.king_storm_bonus[get_row(__builtin_ctzll(storm_mask))]);

			//(semi) open file
			for (int column = 0; column < 8; column++) {
				for (int column_offset = -1; column_offset <= 1; column_offset++) {
					int target_column = column + column_offset;

					if (target_column >= 0 && target_column <= 7) {
						if ((board->pieces[WHITE_PAWN] & board_file[target_column]) == 0) {
							if ((board->pieces[BLACK_PAWN] & board_file[target_column]) == 0)
								open_file_count++;
							else
								semi_open_file_count++;
						}
					}
				}
			}
		}


		mask &= mask - 1;
	}

	//Kingside/Queenside castles are symmetrical so don't mirror column.
	mask = board->pieces[BLACK_KING];
	while (mask) {
		int sq = __builtin_ctzll(mask);
		int row = get_row(sq);
		int column = get_column(sq);

		int mirrored_row = MIRROR_SQUARE(row);

		//piece/square value
		eval_sub(&eval, piece_bonus_table[BLACK_KING][sq]);

		//virtual mobility
		uint64_t virtual_moves = queen_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtual_move_count = __builtin_popcountll(virtual_moves);

		eval_sub(&eval, hce_params.virtual_mobility_bonus[virtual_move_count]);

		if (mirrored_row <= 1) {
			//pawn shield
			int pawn_shield_count = -1 * __builtin_popcountll(king_shield_mask[BLACK][column] & board->pieces[BLACK_PAWN]);

			eval_madd(&eval, hce_params.king_shield_bonus[column], pawn_shield_count);

			//pawn storm
			for (uint64_t storm_mask = king_storm_mask[column] & board->pieces[WHITE_PAWN]; storm_mask > 0; storm_mask &= storm_mask - 1)
				eval_sub(&eval, hce_params.king_storm_bonus[MIRROR_SQUARE(get_row(	__builtin_ctzll(storm_mask))	)]);

			//(semi) open file
			for (int column = 0; column < 8; column++) {
				for (int column_offset = -1; column_offset <= 1; column_offset++) {
					int target_column = column + column_offset;

					if (target_column >= 0 && target_column <= 7) {
						if ((board->pieces[BLACK_PAWN] & board_file[target_column]) == 0) {
							if ((board->pieces[WHITE_PAWN] & board_file[target_column]) == 0)
								open_file_count--;
							else
								semi_open_file_count--;
						}
					}
				}
			}
		}

		mask &= mask - 1;
	}

	eval_madd(&eval, hce_params.open_king_file[SEMI_OPEN_FILE], semi_open_file_count);
	eval_madd(&eval, hce_params.open_king_file[OPEN_FILE], open_file_count);

	//king safety
	context->attack_weight[WHITE] = _min(context->attack_weight[WHITE], 99);
	context->attack_weight[BLACK] = _min(context->attack_weight[BLACK], 99);
	phased_eval king_safety = hce_params.king_safety[context->attack_weight[WHITE]];

	eval_sub(&king_safety, hce_params.king_safety[context->attack_weight[BLACK]]);
	eval_add(&eval, king_safety);

	eval_add(score, eval);
}

int evaluate_phased_score(bitboard *board, phased_eval eval)
{
	int phase = 24;

	for (int pc = 2; pc < PIECE_COUNT - 2; pc++)
		phase -= gamephase_piece_values[pc] * __builtin_popcountll(board->pieces[pc]);

	phase = _min(24, _max(0, phase));

	int eg_phase = (phase * 256 + 12) / 24;
	int mg_phase = 256 - eg_phase;

	return (mg_phase * eval.mg + eg_phase * eval.eg) / 256;
}

int hce_eval(bitboard *board)
{
	phased_eval eval = hce_params.tempo;
	eval_context context = {
		.king_zone = {
			[WHITE] = king_zone[WHITE][board->king_square[WHITE]],
			[BLACK] = king_zone[BLACK][board->king_square[BLACK]]
		}
	};

	evaluate_pawns(board, &eval);
	evaluate_knights(board, &eval, &context);
	evaluate_bishops(board, &eval, &context);
	evaluate_rooks(board, &eval, &context);
	evaluate_queens(board, &eval, &context);
	evaluate_kings(board, &eval, &context);

	int phased_eval = evaluate_phased_score(board, eval);

	phased_eval = _min(MIN_MATE_SCORE - 1, _max(-(MIN_MATE_SCORE - 1), phased_eval));

	return (ISWHITE(board->turn)) ? phased_eval : -phased_eval;
}
