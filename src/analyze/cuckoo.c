#include "compatibility.h"
#include "analyze/cuckoo.h"
#include "board/moves.h"
#include "board/bitboard.h"
#include "board/magic.h"
#include "transposition/hash.h"
#include <string.h>

uint64_t between_squares[64][64];
uint64_t cuckoo_hashes[CUCKOO_SIZE] = {0};
move	 cuckoo_moves[CUCKOO_SIZE]  = {0};

//Also initializes between_squares since only cuckootables use them
void init_cuckoo_move_table(void)
{
	memset(cuckoo_hashes, 0, sizeof(cuckoo_hashes));
	memset(cuckoo_moves, 0, sizeof(cuckoo_moves));

	uint64_t stm_hash = zobrist_keys[780];

	for (int pc = WHITE_KNIGHT; pc <= BLACK_KING; pc++) {
		for (int from = 0; from < 64; from++) {
			uint64_t attacks;

			switch (PIECE(pc)) {
			case KNIGHT:
				attacks = knight_moves(0, from);
				break;
			case BISHOP:
				attacks = bishop_moves(0, 0, from);
				break;
			case ROOK:
				attacks = rook_moves(0, 0, from);
				break;
			case QUEEN:
				attacks = queen_moves(0, 0, from);
				break;
			case KING:
				//Castling moves aren't reversible or symmetrical.
				attacks = king_attacks[from];
				break;
			default:
				attacks = 0;
				break;
			}

			//Non-pawn moves are symmetrical in from/to.
			for (int to = from + 1; to < 64; to++) {
				if (!(attacks & (1ull << (to))))
					continue;

				move m = {
					.start_square = from,
					.end_square = to
				};
				uint64_t move_hash = zobrist_piece_keys[pc][from] ^ zobrist_piece_keys[pc][to] ^ stm_hash;

				int index = CUCKOO_HASH1(move_hash);

				while (1) {
					uint64_t temp_hash = cuckoo_hashes[index];

					cuckoo_hashes[index] = move_hash;
					move_hash = temp_hash;

					move temp_move = cuckoo_moves[index];

					cuckoo_moves[index] = m;
					m = temp_move;

					if (!m.raw)
						break;

					index = (index == CUCKOO_HASH1(move_hash)) ? CUCKOO_HASH2(move_hash) : CUCKOO_HASH1(move_hash);
				}
			}
		}
	}

	for (int a = 0; a < 64; a++) {
		for (int b = 0; b < 64; b++) {
			if (get_row(a) == get_row(b) || get_column(a) == get_column(b))
				between_squares[a][b] = rook_moves((1ull << (b)), 0, a) & rook_moves((1ull << (a)), 0, b);
			else if (bishop_moves(0, 0, a) & (1ull << (b)))
				between_squares[a][b] = bishop_moves((1ull << (b)), 0, a) & bishop_moves((1ull << (a)), 0, b);
		}
	}

}

int is_upcoming_repetition(bitboard *board, repetition_vector repetitions, int ply)
{
	int halfmoves = _min(board->halfmove_clock, board->plies_from_null);

	if (halfmoves < 3)
		return 0;

	uint64_t turn_hash = zobrist_keys[780];

	uint64_t original_hash = repetitions.hash_codes[board->half_move_count];
	uint64_t prev_hash	 = repetitions.hash_codes[board->half_move_count - 1];

	uint64_t other = original_hash ^ prev_hash ^ turn_hash;

	for (int i = 3; i <= halfmoves; i += 2) {
		uint64_t current_hash = repetitions.hash_codes[board->half_move_count - i];

		other ^= current_hash ^ repetitions.hash_codes[board->half_move_count - (i - 1)] ^ turn_hash;

		if (!other)
			continue;

		uint64_t diff = original_hash ^ current_hash;
		int cuckoo_index = CUCKOO_HASH1(diff);

		if (diff != cuckoo_hashes[cuckoo_index])
			cuckoo_index = CUCKOO_HASH2(diff);
		if (diff != cuckoo_hashes[cuckoo_index])
			continue;

		move m = cuckoo_moves[cuckoo_index];

		if (!(between_squares[m.start_square][m.end_square] & board->pieces_all)) {
			if (ply > i)
				return 1;

			int pc = (board->piece_arr[m.start_square]);

			if (pc == EMPTY_PIECE)
				pc = (board->piece_arr[m.end_square]);
			return COLOR(pc) == board->turn;
		}
	}
	return 0;
}
