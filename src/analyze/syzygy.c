#include "debug.h"
#include "analyze/syzygy.h"
#include "board/bitboard.h"
#include "pyrrhic/tbconfig.h"
#include "pyrrhic/tbprobe.h"
#include "analyze/search.h"

void get_from_pyrrhic(bitboard *board, move *dest, unsigned int result)
{
	dest->end_square = TB_RESULT_TO(result);
	dest->start_square = TB_RESULT_FROM(result);
	int piece = (board->piece_arr[dest->start_square]);

	if (!ISPAWN(piece))
		dest->promote_to = 0;
	else if (TB_RESULT_IS_QPROMO(result))
		dest->promote_to = QUEEN;
	else if (TB_RESULT_IS_RPROMO(result))
		dest->promote_to = ROOK;
	else if (TB_RESULT_IS_BPROMO(result))
		dest->promote_to = BISHOP;
	else if (TB_RESULT_IS_NPROMO(result))
		dest->promote_to = KNIGHT;
	else
		dest->promote_to = 0;
}

void filter_syzygy_moves(bitboard *board, move *required_moves)
{
	//3-n man syzygy endgame with no castling rights.
	if (__builtin_popcountll(board->pieces_all) > syzygyProbeLimit ||
		board->in_check ||
		board->halfmove_clock ||
		board->can_kingside_castle_b ||
		board->can_kingside_castle_w ||
		board->can_queenside_castle_b ||
		board->can_queenside_castle_w)
			return;

	//Don't probe syzygy if the GUI assigned required search moves.
	if (required_moves[0].raw)
		return;

	unsigned int move_results[TB_MAX_MOVES];
	unsigned int result = tb_probe_root(board->pieces_side[WHITE],								  board->pieces_side[BLACK],
									board->pieces[BLACK_KING]   | board->pieces[WHITE_KING],	board->pieces[BLACK_QUEEN]  | board->pieces[WHITE_QUEEN],
									board->pieces[BLACK_ROOK]   | board->pieces[WHITE_ROOK],	board->pieces[BLACK_BISHOP] | board->pieces[WHITE_BISHOP],
									board->pieces[BLACK_KNIGHT] | board->pieces[WHITE_KNIGHT],  board->pieces[BLACK_PAWN]   | board->pieces[WHITE_PAWN],
									(unsigned int) board->halfmove_clock,
									(board->ep_square == NO_EP_SQUARE) ? 0 : board->ep_square,
									!board->turn,
									move_results);
	if (result == TB_RESULT_FAILED) {
		DEBUG_ERROR("Failed to probe syzygy move.");
		return;
	} else if (result == TB_RESULT_CHECKMATE || result == TB_RESULT_STALEMATE) {
		return;
	}

	int insert_index = 0;

	get_from_pyrrhic(board, &required_moves[insert_index], result);
	if (required_moves[insert_index].raw)
		insert_index++;

	for (int i = 0; i < TB_MAX_MOVES && insert_index < MAX_REQUIRED_MOVES; i++) {
		if (move_results[i] == TB_RESULT_FAILED)
			break;

		if (TB_GET_WDL(move_results[i]) == TB_GET_WDL(result) && TB_GET_DTZ(move_results[i]) <= TB_GET_DTZ(result)) {
			get_from_pyrrhic(board, &required_moves[insert_index], move_results[i]);

			if (required_moves[insert_index].raw != required_moves[0].raw && required_moves[insert_index].raw)
				insert_index++;
			else
				required_moves[insert_index].raw = 0;
		}
	}
}

//Return -1 on error. Valid results are SCORE_WIN, -SCORE_WIN, 0
int get_syzygy_result(bitboard *board)
{
	//3-n man syzygy endgame with no castling rights.
	if (__builtin_popcountll(board->pieces_all) > syzygyProbeLimit ||
		board->in_check ||
		board->halfmove_clock ||
		board->can_kingside_castle_b ||
		board->can_kingside_castle_w ||
		board->can_queenside_castle_b ||
		board->can_queenside_castle_w)
			return -1;

	uint32_t ep = board->ep_square;

	if (ep == -1)
		ep = 0;

	bool turn = PYRRHIC_WHITE;

	if (ISBLACK(board->turn))
		turn = PYRRHIC_BLACK;

	int result = tb_probe_wdl(board->pieces_side[WHITE], board->pieces_side[BLACK],
									board->pieces[BLACK_KING]|board->pieces[WHITE_KING], board->pieces[BLACK_QUEEN]|board->pieces[WHITE_QUEEN],
									board->pieces[BLACK_ROOK]|board->pieces[WHITE_ROOK], board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP],
									board->pieces[BLACK_KNIGHT]|board->pieces[WHITE_KNIGHT], board->pieces[BLACK_PAWN]|board->pieces[WHITE_PAWN],
									ep, turn);
	switch (result) {
	case TB_LOSS:
		return -SCORE_WIN;
	case TB_WIN:
		return SCORE_WIN;
	case TB_BLESSED_LOSS:
	case TB_CURSED_WIN:
	case TB_DRAW:
		return 0;
	case TB_RESULT_FAILED:
		DEBUG_ERROR("Failed to probe syzygy result.");
	default:
		return -1;
	}
}
