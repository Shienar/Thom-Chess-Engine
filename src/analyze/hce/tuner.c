#include "compatibility.h"
#include "board/moves.h"
#include "analyze/hce/tuner.h"
#include "train/train.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <float.h>
#include <math.h>
#include <string.h>

eval_parameters_fp current_parameters;
tuning_entry *tuner_entries;
int tuner_entry_count;

//Print out all of the parameters in C array format.
//Just look at the declaration at the top of hce.c if this doesn't make sense.
void print_parameters(FILE *output, eval_parameters_fp *current_parameters)
{
	eval_parameters params = {0};

	for (int i = 0; i < PARAMETER_COUNT; i++)
		params.parameters[i] = (phased_eval)P(round(current_parameters->parameters[i].mg), round(current_parameters->parameters[i].eg));

	const char *piece_names[6] = {"PAWN", "KNIGHT", "BISHOP", "ROOK", "QUEEN", "KING"};

	rewind(output);
	fprintf(output, "eval_parameters hce_params = {\n");

	fprintf(output, "\t.generic_piece_values = {\n\t\t");
	for (int piece = 0; piece < PIECE_TYPE_COUNT; piece++)
		fprintf(output, "P(%5d,%5d)%s ", params.generic_piece_values[piece].mg, params.generic_piece_values[piece].eg, (piece == (PIECE_TYPE_COUNT) - 1) ? "" : ",");
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.raw_piece_tables = {\n");
	for (int pc = 0; pc < 6; pc++) {
		fprintf(output, "\t\t[%s / 2] = {\n", piece_names[pc]);
		for (int row = 0; row < ROW_COUNT; row++) {
			fprintf(output, "\t\t\t");
			for (int col = 0; col < COLUMN_COUNT; col++) {
				int idx = row * COLUMN_COUNT + col;

				fprintf(output, "P(%5d,%5d)%s ", params.raw_piece_tables[pc][idx].mg, params.raw_piece_tables[pc][idx].eg, (idx == 63) ? "" : ",");
			}
			fprintf(output, "\n");
		}
		fprintf(output, "\t\t}%s\n", (pc == 5) ? "" : ",");
		}
	fprintf(output, "\t},\n");

	fprintf(output, "\t.knight_mobility_bonus = {");
	for (int i = 0; i < 9; i++) {
		if (!(i % 8))
			fprintf(output, "\n\t\t");
		fprintf(output, "P(%5d,%5d)%s ", params.knight_mobility_bonus[i].mg, params.knight_mobility_bonus[i].eg, (i == 8) ? "" : ",");
	}
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.bishop_mobility_bonus = {");
	for (int i = 0; i < 14; i++) {
		if (!(i % 8))
			fprintf(output, "\n\t\t");
		fprintf(output, "P(%5d,%5d)%s ", params.bishop_mobility_bonus[i].mg, params.bishop_mobility_bonus[i].eg, (i == 13) ? "" : ",");
	}
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.rook_mobility_bonus = {");
	for (int i = 0; i < 15; i++) {
		if (!(i % 8))
			fprintf(output, "\n\t\t");
		fprintf(output, "P(%5d,%5d)%s ", params.rook_mobility_bonus[i].mg, params.rook_mobility_bonus[i].eg, (i == 14) ? "" : ",");
	}
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.queen_mobility_bonus = {");
	for (int i = 0; i < 28; i++) {
		if (!(i % 8))
			fprintf(output, "\n\t\t");
		fprintf(output, "P(%5d,%5d)%s ", params.queen_mobility_bonus[i].mg, params.queen_mobility_bonus[i].eg, (i == 27) ? "" : ",");
	}
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.virtual_mobility_bonus = {");
	for (int i = 0; i < 28; i++) {
		if (!(i % 8))
			fprintf(output, "\n\t\t");
		fprintf(output, "P(%5d,%5d)%s ", params.virtual_mobility_bonus[i].mg, params.virtual_mobility_bonus[i].eg, (i == 27) ? "" : ",");
	}
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.minor_pawn_cover = P(%5d,%5d),\n", params.minor_pawn_cover.mg, params.minor_pawn_cover.eg);

	fprintf(output, "\t.passed_pawn_bonus = {\n\t\t");
	for (int column = 0; column < COLUMN_COUNT; column++)
		fprintf(output, "P(%5d,%5d)%s ", params.passed_pawn_bonus[column].mg, params.passed_pawn_bonus[column].eg, (column == 7) ? "" : ",");
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.connected_pawn_bonus = {\n\t\t");
	for (int row = 0; row < ROW_COUNT; row++)
		fprintf(output, "P(%5d,%5d)%s ", params.connected_pawn_bonus[row].mg, params.connected_pawn_bonus[row].eg, (row == 7) ? "" : ",");
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.neighbor_pawn_bonus = {\n\t\t");
	for (int row = 0; row < ROW_COUNT; row++)
		fprintf(output, "P(%5d,%5d)%s ", params.neighbor_pawn_bonus[row].mg, params.neighbor_pawn_bonus[row].eg, (row == 7) ? "" : ",");
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.doubled_pawn_bonus = {\n\t\t");
	for (int column = 0; column < COLUMN_COUNT; column++)
		fprintf(output, "P(%5d,%5d)%s ", params.doubled_pawn_bonus[column].mg, params.doubled_pawn_bonus[column].eg, (column == 7) ? "" : ",");
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.isolated_pawn_bonus = {\n\t\t");
	for (int column = 0; column < COLUMN_COUNT; column++)
		fprintf(output, "P(%5d,%5d)%s ", params.isolated_pawn_bonus[column].mg, params.isolated_pawn_bonus[column].eg, (column == 7) ? "" : ",");
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.knight_outpost_bonus = P(%5d,%5d),\n", params.knight_outpost_bonus.mg, params.knight_outpost_bonus.eg);

	fprintf(output, "\t.bishop_pair_bonus = P(%5d,%5d),\n", params.bishop_pair_bonus.mg, params.bishop_pair_bonus.eg);
	fprintf(output, "\t.bad_bishop_bonus = P(%5d,%5d),\n", params.bad_bishop_bonus.mg, params.bad_bishop_bonus.eg);

	fprintf(output, "\t.open_rook_file_bonus = { P(%5d,%5d), P(%5d,%5d) },\n", params.open_rook_file_bonus[0].mg, params.open_rook_file_bonus[0].eg,
																			params.open_rook_file_bonus[1].mg, params.open_rook_file_bonus[1].eg);

	fprintf(output, "\t.connected_rook_bonus = { P(%5d,%5d), P(%5d,%5d) },\n", params.connected_rook_bonus[0].mg, params.connected_rook_bonus[0].eg,
																			 params.connected_rook_bonus[1].mg, params.connected_rook_bonus[1].eg);

	fprintf(output, "\t.connected_queen_bonus = { P(%5d,%5d), P(%5d,%5d), P(%5d,%5d) },\n", params.connected_queen_bonus[0].mg, params.connected_queen_bonus[0].eg,
																						  params.connected_queen_bonus[1].mg, params.connected_queen_bonus[1].eg,
																						  params.connected_queen_bonus[2].mg, params.connected_queen_bonus[2].eg);


	fprintf(output, "\t.king_shield_bonus = {\n\t\t");
	for (int column = 0; column < COLUMN_COUNT; column++)
		fprintf(output, "P(%5d,%5d)%s ", params.king_shield_bonus[column].mg, params.king_shield_bonus[column].eg, (column == 7) ? "" : ",");
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.king_storm_bonus = {\n\t\t");
	for (int column = 0; column < COLUMN_COUNT; column++)
		fprintf(output, "P(%5d,%5d)%s ", params.king_storm_bonus[column].mg, params.king_storm_bonus[column].eg, (column == 7) ? "" : ",");
	fprintf(output, "\n\t},\n");

	fprintf(output, "\t.open_king_file = { P(%5d,%5d), P(%5d,%5d) },\n", params.open_king_file[0].mg, params.open_king_file[0].eg,
																	   params.open_king_file[1].mg, params.open_king_file[1].eg);

	fprintf(output, "\t.king_safety = {\n");
	for (int i = 0; i < 10; i++) {
		fprintf(output, "\t\t");
		for (int j = 0; j < 10; j++) {
			int index = 10 * i + j;

			fprintf(output, "P(%5d,%5d)%s ", params.king_safety[index].mg, params.king_safety[index].eg, (index == 99) ? "" : ",");
		}
		fprintf(output, "\n");
	}
	fprintf(output, "\t},\n");

	fprintf(output, "\t.tempo = P(%5d,%5d),\n", params.tempo.mg, params.tempo.eg);

	fprintf(output, "};\n");
	fflush(output);
}

void init_pawn_coefficients(bitboard *board, eval_parameters *coefficients, eval_parameters_fp params, phased_eval_fp *score, eval_context *context)
{
	uint64_t mask = board->pieces[WHITE_PAWN];

	int protected_count = __builtin_popcountll((mask >> 8) & (board->pieces[WHITE_BISHOP] | board->pieces[WHITE_KNIGHT]));

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/square value
		UPDATE_COEFFICIENTS(generic_piece_values, 1, +=, [PAWN / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, +=, [PAWN / 2][FLIP_SQUARE(sq)]);

		//Passed Pawns
		if ((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
			UPDATE_COEFFICIENTS(passed_pawn_bonus, 1, +=, [row]);

		//Doubled pawns
		if (__builtin_popcountll(board->pieces[WHITE_PAWN] & board_file[column]) > 1)
			UPDATE_COEFFICIENTS(doubled_pawn_bonus, 1, +=, [column]);

		uint64_t borderingMask = board->pieces[WHITE_PAWN] & bordering_files[column];
		//Isolated Pawns
		if (!borderingMask)
			UPDATE_COEFFICIENTS(isolated_pawn_bonus, 1, +=, [column]);
		//connected pawns
		else if (borderingMask & (board_rank[row - 1] | board_rank[row + 1]))
			UPDATE_COEFFICIENTS(connected_pawn_bonus, 1, +=, [row]);

		//Neighboring pawns
		int neighbors = __builtin_popcountll(borderingMask & board_rank[row]);

		if (neighbors && row > 1)
			UPDATE_COEFFICIENTS(neighbor_pawn_bonus, neighbors, +=, [row]);

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
		UPDATE_COEFFICIENTS(generic_piece_values, 1, -=, [PAWN / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, -=, [PAWN / 2][MIRROR_SQUARE(sq)]);

		//Passed Pawns
		if ((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
			UPDATE_COEFFICIENTS(passed_pawn_bonus, 1, -=, [mirrored_row]);

		//Doubled pawns
		if (__builtin_popcountll(board->pieces[BLACK_PAWN] & board_file[column]) > 1)
			UPDATE_COEFFICIENTS(doubled_pawn_bonus, 1, -=, [mirrored_column]);

		uint64_t borderingMask = board->pieces[BLACK_PAWN] & bordering_files[column];
		//Isolated Pawns
		if (!borderingMask)
			UPDATE_COEFFICIENTS(isolated_pawn_bonus, 1, -=, [mirrored_column]);
		//connected pawns
		else if (borderingMask & (board_rank[row - 1] | board_rank[row + 1]))
			UPDATE_COEFFICIENTS(connected_pawn_bonus, 1, -=, [mirrored_row]);

		//Neighboring pawns
		int neighbors = __builtin_popcountll(borderingMask & board_rank[row]);

		if (neighbors && mirrored_row > 1)
			UPDATE_COEFFICIENTS(neighbor_pawn_bonus, neighbors, -=, [mirrored_row]);

		mask &= mask - 1;
	}

	UPDATE_COEFFICIENTS(minor_pawn_cover, protected_count, +=, );
}

void init_knight_coefficients(bitboard *board, eval_parameters *coefficients, eval_parameters_fp params, phased_eval_fp *score, eval_context *context)
{
	uint64_t mask = board->pieces[WHITE_KNIGHT];

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/square value
		UPDATE_COEFFICIENTS(generic_piece_values, 1, +=, [KNIGHT / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, +=, [KNIGHT / 2][FLIP_SQUARE(sq)]);

		//mobility
		uint64_t moves = knight_moves(board->pieces_side[WHITE], sq);
		int move_count = __builtin_popcountll(moves);

		//king safety
		context->attack_weight[WHITE] += KNIGHT_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[BLACK]);

		UPDATE_COEFFICIENTS(knight_mobility_bonus, 1, +=, [move_count]);

		//outpost
		if (row >= 4 && row <= 6 &&
		   board->pieces[WHITE_PAWN] && board_rank[row - 1] &&
		   (bordering_files[column] & board->pieces[BLACK_PAWN] & (~((1ull << (sq + 7)) - 1))) == 0)
				UPDATE_COEFFICIENTS(knight_outpost_bonus, 1, +=, );

		mask &= mask - 1;
	}

	mask = board->pieces[BLACK_KNIGHT];
	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/square value
		UPDATE_COEFFICIENTS(generic_piece_values, 1, -=, [KNIGHT / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, -=, [KNIGHT / 2][MIRROR_SQUARE(sq)]);

		//mobility
		uint64_t moves = knight_moves(board->pieces_side[BLACK], sq);
		int move_count = __builtin_popcountll(moves);

		//king safety
		context->attack_weight[BLACK] += KNIGHT_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[WHITE]);

		UPDATE_COEFFICIENTS(knight_mobility_bonus, 1, -=, [move_count]);

		//outpost
		if (row >= 1 && row <= 3 &&
		   board->pieces[BLACK_PAWN] && board_rank[row + 1] &&
		   (bordering_files[column] & board->pieces[WHITE_PAWN] & (((1ull << (sq - 6)) - 1))) == 0)
				UPDATE_COEFFICIENTS(knight_outpost_bonus, 1, -=, );

		mask &= mask - 1;
	}
}

void init_bishop_coefficients(bitboard *board, eval_parameters *coefficients, eval_parameters_fp params, phased_eval_fp *score, eval_context *context)
{
	uint64_t mask = board->pieces[WHITE_BISHOP];

	//bad bishop-pawns
	int bad_pawns = 0;

	if (mask & LIGHT_SQUARES)
		bad_pawns += __builtin_popcountll(board->pieces[WHITE_PAWN] & LIGHT_SQUARES);
	if (mask & DARK_SQUARES)
		bad_pawns += __builtin_popcountll(board->pieces[WHITE_PAWN] & DARK_SQUARES);

	//bishop pair
	if (__builtin_popcountll(mask) >= 2)
		UPDATE_COEFFICIENTS(bishop_pair_bonus, 1, +=, );

	while (mask) {
		int sq = __builtin_ctzll(mask);

		//piece/square value
		UPDATE_COEFFICIENTS(generic_piece_values, 1, +=, [BISHOP / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, +=, [BISHOP / 2][FLIP_SQUARE(sq)]);

		//mobility
		uint64_t moves = bishop_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int move_count = __builtin_popcountll(moves);

		//king safety
		context->attack_weight[WHITE] += BISHOP_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[BLACK]);

		UPDATE_COEFFICIENTS(bishop_mobility_bonus, 1, +=, [move_count]);

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
		UPDATE_COEFFICIENTS(bishop_pair_bonus, 1, -=, );

	while (mask) {
		int sq = __builtin_ctzll(mask);

		//piece/sq value
		UPDATE_COEFFICIENTS(generic_piece_values, 1, -=, [BISHOP / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, -=, [BISHOP / 2][MIRROR_SQUARE(sq)]);

		//mobility
		uint64_t moves = bishop_moves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int move_count = __builtin_popcountll(moves);

		//king safety
		context->attack_weight[BLACK] += BISHOP_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[WHITE]);

		UPDATE_COEFFICIENTS(bishop_mobility_bonus, 1, -=, [move_count]);

		mask &= mask - 1;
	}

	UPDATE_COEFFICIENTS(bad_bishop_bonus, bad_pawns, +=, );
}

void init_rook_coefficients(bitboard *board, eval_parameters *coefficients, eval_parameters_fp params, phased_eval_fp *score, eval_context *context)
{
	int connected_rooks_by_row = 0;
	int connected_rooks_by_column = 0;

	uint64_t mask = board->pieces[WHITE_ROOK];

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/sq value
		UPDATE_COEFFICIENTS(generic_piece_values, 1, +=, [ROOK / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, +=, [ROOK / 2][FLIP_SQUARE(sq)]);

		//open rook file
		if ((board->pieces[WHITE_PAWN] & board_file[column]) == 0) {
			if ((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
				UPDATE_COEFFICIENTS(open_rook_file_bonus, 1, +=, [OPEN_FILE]);
			else
				UPDATE_COEFFICIENTS(open_rook_file_bonus, 1, +=, [SEMI_OPEN_FILE]);
		}

		//mobility
		uint64_t moves = rook_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int move_count = __builtin_popcountll(moves);

		//king safety
		context->attack_weight[WHITE] += ROOK_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[BLACK]);

		UPDATE_COEFFICIENTS(rook_mobility_bonus, 1, +=, [move_count]);

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
		UPDATE_COEFFICIENTS(generic_piece_values, 1, -=, [ROOK / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, -=, [ROOK / 2][MIRROR_SQUARE(sq)]);

		//open rook file
		if ((board->pieces[BLACK_PAWN] & board_file[column]) == 0) {
			if ((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
				UPDATE_COEFFICIENTS(open_rook_file_bonus, 1, -=, [OPEN_FILE]);
			else
				UPDATE_COEFFICIENTS(open_rook_file_bonus, 1, -=, [SEMI_OPEN_FILE]);
		}

		//mobility
		uint64_t moves = rook_moves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int move_count = __builtin_popcountll(moves);

		//king safety
		context->attack_weight[BLACK] += ROOK_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[WHITE]);

		UPDATE_COEFFICIENTS(rook_mobility_bonus, 1, -=, [move_count]);

		//rook rams
		uint64_t connections = rook_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		uint64_t connected_rooks = connections & board->pieces[BLACK_ROOK];

		connected_rooks_by_row -= __builtin_popcountll(connected_rooks & board_rank[row]);
		connected_rooks_by_column -= __builtin_popcountll(connected_rooks & board_file[column]);

		mask &= mask - 1;
	}

	UPDATE_COEFFICIENTS(connected_rook_bonus, connected_rooks_by_column, +=, [CONNECTED_COLUMN]);
	UPDATE_COEFFICIENTS(connected_rook_bonus, connected_rooks_by_row, +=, [CONNECTED_ROW]);
}

void init_queen_coefficients(bitboard *board, eval_parameters *coefficients, eval_parameters_fp params, phased_eval_fp *score, eval_context *context)
{
	int connected_sliders_by_row = 0;
	int connected_sliders_by_column = 0;
	int connected_sliders_by_diagonal = 0;

	uint64_t mask = board->pieces[WHITE_QUEEN];

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int column = get_column(sq);
		int row = get_row(sq);

		//piece/square value
		UPDATE_COEFFICIENTS(generic_piece_values, 1, +=, [QUEEN / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, +=, [QUEEN / 2][FLIP_SQUARE(sq)]);

		//mobility
		uint64_t moves = queen_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int move_count = __builtin_popcountll(moves);

		//king safety
		context->attack_weight[WHITE] += QUEEN_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[BLACK]);

		UPDATE_COEFFICIENTS(queen_mobility_bonus, 1, +=, [move_count]);

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
		UPDATE_COEFFICIENTS(generic_piece_values, 1, -=, [QUEEN / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, -=, [QUEEN / 2][MIRROR_SQUARE(sq)]);

		//mobility
		uint64_t moves = queen_moves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int move_count = __builtin_popcountll(moves);

		//king safety
		context->attack_weight[BLACK] += QUEEN_ATTACK_WEIGHT * __builtin_popcountll(moves & context->king_zone[WHITE]);

		UPDATE_COEFFICIENTS(queen_mobility_bonus, 1, -=, [move_count]);

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

	UPDATE_COEFFICIENTS(connected_queen_bonus, connected_sliders_by_row, +=, [CONNECTED_ROW]);
	UPDATE_COEFFICIENTS(connected_queen_bonus, connected_sliders_by_column, +=, [CONNECTED_COLUMN]);
	UPDATE_COEFFICIENTS(connected_queen_bonus, connected_sliders_by_diagonal, +=, [CONNECTED_DIAGONAL]);
}

void init_king_coefficients(bitboard *board, eval_parameters *coefficients, eval_parameters_fp params, phased_eval_fp *score, eval_context *context)
{
	uint64_t mask = board->pieces[WHITE_KING];

	int semi_open_file_count = 0;
	int open_file_count = 0;

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int row = get_row(sq);
		int column = get_column(sq);

		//piece/square value
		UPDATE_COEFFICIENTS(generic_piece_values, 1, +=, [KING / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, +=, [KING / 2][FLIP_SQUARE(sq)]);

		//virtual mobility
		uint64_t virtual_moves = queen_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtual_move_count = __builtin_popcountll(virtual_moves);

		UPDATE_COEFFICIENTS(virtual_mobility_bonus, 1, +=, [virtual_move_count]);

		if (row <= 1) {
			//pawn shield
			int pawn_shield_count = __builtin_popcountll(king_shield_mask[WHITE][column] & board->pieces[WHITE_PAWN]);

			UPDATE_COEFFICIENTS(king_shield_bonus, pawn_shield_count, +=, [column]);

			//pawn storm
			for (uint64_t storm_mask = king_storm_mask[column] & board->pieces[BLACK_PAWN]; storm_mask > 0; storm_mask &= storm_mask - 1) {
				int pawn_row = get_row(__builtin_ctzll(storm_mask));

				UPDATE_COEFFICIENTS(king_storm_bonus, 1, +=, [pawn_row]);
			}

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

	mask = board->pieces[BLACK_KING];
	while (mask) {
		int sq = __builtin_ctzll(mask);
		int row = get_row(sq);
		int column = get_column(sq);

		int mirrored_row = MIRROR_SQUARE(row);

		//piece/square value
		UPDATE_COEFFICIENTS(generic_piece_values, 1, -=, [KING / 2]);
		UPDATE_COEFFICIENTS(raw_piece_tables, 1, -=, [KING / 2][MIRROR_SQUARE(sq)]);

		//virtual mobility
		uint64_t virtual_moves = queen_moves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtual_move_count = __builtin_popcountll(virtual_moves);

		UPDATE_COEFFICIENTS(virtual_mobility_bonus, 1, -=, [virtual_move_count]);

		if (mirrored_row <= 1) {
			//pawn shield
			int pawn_shield_count = __builtin_popcountll(king_shield_mask[BLACK][column] & board->pieces[BLACK_PAWN]);

			UPDATE_COEFFICIENTS(king_shield_bonus, pawn_shield_count, -=, [column]);

			//pawn storm
			for (uint64_t storm_mask = king_storm_mask[column] & board->pieces[BLACK_PAWN]; storm_mask > 0; storm_mask &= storm_mask - 1) {
				int pawn_row = get_row(__builtin_ctzll(storm_mask));

				pawn_row = MIRROR_SQUARE(pawn_row);
				UPDATE_COEFFICIENTS(king_storm_bonus, 1, -=, [pawn_row]);
			}

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

	//king safety
	context->attack_weight[WHITE] = _min(context->attack_weight[WHITE], 99);
	context->attack_weight[BLACK] = _min(context->attack_weight[BLACK], 99);
	UPDATE_COEFFICIENTS(king_safety, 1, +=, [context->attack_weight[WHITE]]);
	UPDATE_COEFFICIENTS(king_safety, 1, -=, [context->attack_weight[BLACK]]);

	UPDATE_COEFFICIENTS(open_king_file, open_file_count, +=, [OPEN_FILE]);
	UPDATE_COEFFICIENTS(open_king_file, semi_open_file_count, +=, [SEMI_OPEN_FILE]);
}

void init_coefficients(bitboard *board, eval_parameters *coefficients, eval_parameters_fp params, phased_eval_fp *score)
{
	score->mg = 0.0;
	score->eg = 0.0;

	eval_context context = {
		.king_zone = {
			[WHITE] = king_zone[WHITE][board->king_square[WHITE]],
			[BLACK] = king_zone[BLACK][board->king_square[BLACK]]
		}
	};

	if (ISWHITE(board->turn))
		UPDATE_COEFFICIENTS(tempo, 1, +=, );
	else
		UPDATE_COEFFICIENTS(tempo, 1, -=, );

	init_pawn_coefficients(board, coefficients, params, score, &context);
	init_knight_coefficients(board, coefficients, params, score, &context);
	init_bishop_coefficients(board, coefficients, params, score, &context);
	init_rook_coefficients(board, coefficients, params, score, &context);
	init_queen_coefficients(board, coefficients, params, score, &context);
	init_king_coefficients(board, coefficients, params, score, &context);
}

void init_tuples(tuning_entry *entry, eval_parameters *coefficients)
{
	//This currently works since midgame & endgame coefficients are always equal.
	int length = 0;

	for (int i = 0; i < PARAMETER_COUNT; i++)
		if (coefficients->parameters[i].mg != 0)
			length++;

	entry->active_tuple_count = length;
	entry->active_tuples = calloc(length, sizeof(tuning_tuple));

	int insert_index = 0;

	for (int i = 0; i < PARAMETER_COUNT; i++)
		if (coefficients->parameters[i].mg != 0)
			entry->active_tuples[insert_index++] = (tuning_tuple) { i, coefficients->parameters[i].mg };
}

void init_single_entry(tuning_entry *entry, bitboard *board)
{
	int phase = 24;

	for (int pc = 2; pc < PIECE_COUNT - 2; pc++)
		phase -= gamephase_piece_values[pc] * __builtin_popcountll(board->pieces[pc]);
	phase = _min(24, _max(0, phase));

	entry->phase_factors.mg = 1.0 - phase / 24.0;
	entry->phase_factors.eg = phase / 24.0;

	entry->phase.eg = (phase * 256 + 12) / 24;
	entry->phase.mg = 256 - entry->phase.eg;

	//White relative
	eval_parameters coefficients = {0};

	init_coefficients(board, &coefficients, current_parameters, &entry->score);

	entry->heuristic_eval = (entry->phase.mg * entry->score.mg + entry->phase.eg * entry->score.eg) / 256;

	init_tuples(entry, &coefficients);
}

void init_data_entries(const char *data_path)
{
	bitboard board;
	char buffer[256];

	FILE *data = fopen(data_path, "r");

	if (!data) {
		printf("Cannot open data file.");
		exit(1);
	}

	int entry_count = 0;

	while (fgets(buffer, 256, data) != NULL) {
		if (entry_count % 100000 == 0)
			printf("\rCounting Entries: %d", entry_count);
		entry_count++;
	}

	tuner_entries = calloc(entry_count, sizeof(tuning_entry));
	tuner_entry_count = entry_count;

	rewind(data);
	int insert_index = 0;

	while (fgets(buffer, 256, data) != NULL) {
		double result = 0.0;
		char *eval = strchr(buffer, '[');

		sscanf(eval, "[%lf]\n", &result);
		*(eval - 1) = '\0';

		load_fen_string_to_board(&board, buffer, NULL);

		//White relative
		tuner_entries[insert_index].result = result;

		init_single_entry(&tuner_entries[insert_index++], &board);

		if (insert_index % 100000 == 0)
			printf("\rInitialized Entry %d/%d", insert_index, tuner_entry_count);
	}
	printf("\33[2K\r");

	fclose(data);
}

double get_error(tuning_entry *entries, double K)
{
	double sum_squared_error = 0.0;

	#pragma omp parallel shared(sum_squared_error)
	{
		#pragma omp for reduction(+:sum_squared_error)
		for (int i = 0; i < tuner_entry_count; i++) {
			double err = entries[i].result - sigmoidK(entries[i].heuristic_eval, K);

			sum_squared_error += err * err;
		}
	}

	return sum_squared_error / tuner_entry_count;
}

double compute_optimal_k(tuning_entry *entries)
{
	double start = -10.0;
	double end = 10.0;
	double step = 1.0;

	double current = start;

	printf("\33[2K\rCurrent K=%f", start);

	double current_error;
	double min_error = get_error(entries, start);

	for (int i = 0; i < 2; i++) {
		current = start - step;
		while (current < end) {
			current += step;
			current_error = get_error(entries, current);
			if (current_error <= min_error) {
				min_error = current_error;
				start = current;
				printf("\33[2K\rCurrent K=%f", start);
			}
		}

		end = start + step;
		start = start - step;
		step /= 10.0;
	}

	if (start == 0.0)
		start = 0.1;

	printf("\33[2K\r");
	return start;
}

//Technically updating 2. Endgame + midgame.
void update_single_gradient(tuning_entry *entry, eval_parameters_fp *gradient, double K)
{
	int eval = entry->heuristic_eval;
	double s = sigmoidK(eval, K);
	double err = (entry->result - s) * s * (1 - s);
	double mg_base = err * entry->phase_factors.mg;
	double eg_base = err * entry->phase_factors.eg;

	for (int i = 0; i < entry->active_tuple_count; i++) {
		int index = entry->active_tuples[i].index;
		int coeff = entry->active_tuples[i].coefficient;

		gradient->parameters[index].mg += mg_base * coeff;
		gradient->parameters[index].eg += eg_base * coeff;
	}
}

void compute_gradient(tuning_entry *entries, eval_parameters_fp *gradient,  double K)
{
	memset(gradient, 0, sizeof(eval_parameters_fp));

	#pragma omp parallel shared(gradient)
	{
		eval_parameters_fp local = {0};

		#pragma omp for
		for (int i = 0; i < tuner_entry_count; i++)
			update_single_gradient(&entries[i], &local, K);

		for (int i = 0; i < PARAMETER_COUNT; i++) {
			gradient->parameters[i].mg += local.parameters[i].mg;
			gradient->parameters[i].eg += local.parameters[i].eg;
		}
	}
}

void enforce_zero_center(phased_eval_fp *table, int size)
{
	double sum = 0.0;

	for (int i = 0; i < size; i++)
		sum += table[i].mg;

	double average = sum / size;

	for (int i = 0; i < size; i++)
		table[i].mg -= average;

	sum = 0.0;
	for (int i = 0; i < size; i++)
		sum += table[i].eg;

	average = sum / size;
	for (int i = 0; i < size; i++)
		table[i].eg -= average;
}

void enforce_monotonic_increasing(phased_eval_fp *table,  int size)
{
	for (int i = 1; i < size; i++) {
		table[i].mg = _max(table[i - 1].mg, table[i].mg);
		table[i].eg = _max(table[i - 1].eg, table[i].eg);
	}
}

void enforce_monotonic_decreasing(phased_eval_fp *table,  int size)
{
	for (int i = 1; i < size; i++) {
		table[i].mg = _min(table[i - 1].mg, table[i].mg);
		table[i].eg = _min(table[i - 1].eg, table[i].eg);
	}
}

void enforce_zero_min(phased_eval_fp *table, int size)
{
	double min_mg = DBL_MAX;
	double min_eg = DBL_MAX;

	for (int i = 0; i < size; i++) {
		min_mg = _min(min_mg, table[i].mg);
		min_eg = _min(min_eg, table[i].eg);
	}

	for (int i = 0; i < size; i++) {
		table[i].mg -= min_mg;
		table[i].eg -= min_eg;
	}
}

void refresh_evaluations(eval_parameters_fp *current_parameters, eval_parameters_fp *delta_parameters)
{
	for (int i = 0; i < PARAMETER_COUNT; i++) {
		current_parameters->parameters[i].mg += delta_parameters->parameters[i].mg;
		current_parameters->parameters[i].eg += delta_parameters->parameters[i].eg;
	}

	//When initial weights are all zero (excepting 100/300/300/500/900 piece values), they tend to bleed together.
	//
	//I don't want a piece's mobility weights or square tables being in the range of [150, 250] with a raw piece value that is
	//200 lower than what it should be.
	//
	//This is an attempt to enforce some kind of intuitive understanding of what
	//the weights are supposed to be doing.

	enforce_zero_center(&current_parameters->raw_piece_tables[PAWN / 2][8], 48);

	for (int pc = 1; pc < 6; pc++)
		enforce_zero_center(current_parameters->raw_piece_tables[pc], 64);

	enforce_zero_center(current_parameters->knight_mobility_bonus, 9);
	enforce_monotonic_increasing(current_parameters->knight_mobility_bonus, 9);

	enforce_zero_center(current_parameters->bishop_mobility_bonus, 14);
	enforce_monotonic_increasing(current_parameters->bishop_mobility_bonus, 14);

	enforce_zero_center(current_parameters->rook_mobility_bonus, 15);
	enforce_monotonic_increasing(current_parameters->rook_mobility_bonus, 15);

	enforce_zero_center(current_parameters->queen_mobility_bonus, 28);
	enforce_monotonic_increasing(current_parameters->queen_mobility_bonus, 28);

	enforce_zero_center(current_parameters->virtual_mobility_bonus, 28);
	enforce_monotonic_decreasing(current_parameters->virtual_mobility_bonus, 28);

	enforce_monotonic_increasing(&current_parameters->passed_pawn_bonus[1], 6);

	enforce_zero_min(current_parameters->king_safety, 100);
	enforce_monotonic_increasing(current_parameters->king_safety, 100);

	#pragma omp parallel for
	for (int entry_num = 0; entry_num < tuner_entry_count; entry_num++) {
		tuning_entry *entry = &tuner_entries[entry_num];

		entry->score.mg = 0;
		entry->score.eg = 0;
		for (int i = 0; i < entry->active_tuple_count; i++) {
			int index = entry->active_tuples[i].index;
			int coeff = entry->active_tuples[i].coefficient;

			entry->score.mg += coeff * current_parameters->parameters[index].mg;
			entry->score.eg += coeff * current_parameters->parameters[index].eg;
		}

		entry->heuristic_eval = (entry->phase.mg * entry->score.mg + entry->phase.eg * entry->score.eg) / 256;
	}
}

void tune(const char *data_path, const char *output_path, double forced_k, uint64_t epochs, double max_lr, double min_lr)
{
	printf("Tuning for %" PRId64"  epochs at LR=[%g ... %g]...\n", epochs, max_lr, min_lr);

	for (int i = 0; i < PARAMETER_COUNT; i++) {
		current_parameters.parameters[i].mg = (double) hce_params.parameters[i].mg;
		current_parameters.parameters[i].eg = (double) hce_params.parameters[i].eg;
	}

	printf("Initializing data entries...\n");
	init_data_entries(data_path);

	FILE *output = fopen(output_path, "w");

	if (!output) {
		printf("Cannot open output file.\n");
		exit(1);
	}

	double K;

	if (forced_k != 0) {
		//Enforce specific scale.
		K = forced_k;
		printf("Enforcing ");
	} else {
		//Retain same scale of preexisting terms
		printf("Calculating K...\n");
		K = compute_optimal_k(tuner_entries);
	}
	printf("K=%f\n", K);

	eval_parameters_fp first_moments = {0};
	eval_parameters_fp second_moments = {0};

	double lr = max_lr;
	int end_lr_epoch = _min(epochs, 2500);

	printf("Initial Error = %e\n", get_error(tuner_entries, K));
	print_parameters(output, &current_parameters);

	for (int epoch = 1; epoch < epochs; epoch++) {
		if (epoch < end_lr_epoch) {
			lr = min_lr + 0.5 * (max_lr -  min_lr) * (1.0 + cos(PI * (epoch) / end_lr_epoch));
			lr = _min(max_lr, _max(min_lr, lr));
		}

		eval_parameters_fp gradient = {0};

		compute_gradient(tuner_entries, &gradient, K);

		double bias_correction_1 = 1.0 - pow(ADAM_BETA1, epoch + 1);
		double bias_correction_2 = 1.0 - pow(ADAM_BETA2, epoch + 1);

		for (int i = 0; i < PARAMETER_COUNT; i++) {
			//Average: /= tuner_entry_count
			//MSE derivative: *= 2
			//Chain rule: *= -K / 400.0
			gradient.parameters[i].mg *= (-K / (200.0 * tuner_entry_count));
			gradient.parameters[i].eg *= (-K / (200.0 * tuner_entry_count));

			first_moments.parameters[i].mg = ADAM_BETA1 * first_moments.parameters[i].mg + (1.0 - ADAM_BETA1) * gradient.parameters[i].mg;
			second_moments.parameters[i].mg = ADAM_BETA2 * second_moments.parameters[i].mg + (1.0 - ADAM_BETA2) * gradient.parameters[i].mg * gradient.parameters[i].mg;

			first_moments.parameters[i].eg = ADAM_BETA1 * first_moments.parameters[i].eg + (1.0 - ADAM_BETA1) * gradient.parameters[i].eg;
			second_moments.parameters[i].eg = ADAM_BETA2 * second_moments.parameters[i].eg + (1.0 - ADAM_BETA2) * gradient.parameters[i].eg * gradient.parameters[i].eg;

			phased_eval_fp corrected_first_moment = P(first_moments.parameters[i].mg / bias_correction_1, first_moments.parameters[i].eg / bias_correction_1);
			phased_eval_fp corrected_second_moment = P(second_moments.parameters[i].mg / bias_correction_2, second_moments.parameters[i].eg / bias_correction_2);

			gradient.parameters[i].mg = -(lr * corrected_first_moment.mg) / (sqrt(corrected_second_moment.mg) + 1e-8);
			gradient.parameters[i].eg = -(lr * corrected_first_moment.eg) / (sqrt(corrected_second_moment.eg) + 1e-8);
		}

		refresh_evaluations(&current_parameters, &gradient);
		double error = get_error(tuner_entries, K);

		printf("\rEpoch %d (LR=%g): Error = %e", epoch, lr, error);

		if (epoch % 25 == 0) {
			printf("\n");
			print_parameters(output, &current_parameters);
		}
	}

	printf("\nTuning complete!\n");

	fclose(output);
}
