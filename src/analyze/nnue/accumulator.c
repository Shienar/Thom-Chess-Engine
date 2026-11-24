#include "analyze/nnue/accumulator.h"
#include <immintrin.h>
#include <string.h>

//full refresh of raw values.
void calculate_accumulator(uint64_t *input_nodes, int16_t *output_values, int king_bucket_offset)
{
	for (int output_index = 0; output_index < ACCUMULATOR_NODES_PER_SIDE; output_index += 16) {
		__m256i v_output = _mm256_loadu_si256((const __m256i *)&weights->accumulator_bias[output_index]);

		for (int piece = 0; piece < PIECE_COUNT; piece++) {
			uint64_t piece_mask = input_nodes[piece];
			int base_index = king_bucket_offset + piece * 64;

			while (piece_mask) {
				int feature_index =  base_index + __builtin_ctzll(piece_mask);

				v_output = _mm256_adds_epi16(v_output, _mm256_loadu_si256((const __m256i *)&weights->accumulator_weights[feature_index][output_index]));
				piece_mask &= (piece_mask - 1);
			}
		}
		_mm256_storeu_si256((__m256i *)&output_values[output_index], v_output);
	}
}

void load_input_accumulator(bitboard *board, accumulator *acc)
{
	assert(board);
	assert(acc);

	uint64_t inputs[2 * PIECE_COUNT] = {0};

	for (int i = 0; i < PIECE_TYPE_COUNT; i++) {
		//White's perspective
		inputs[i] = board->pieces[2 * i];
		inputs[PIECE_TYPE_COUNT + i] = board->pieces[2 * i + 1];

		//Black's perspective
		inputs[PIECE_COUNT + i] = FLIP_MASK(board->pieces[2 * i + 1]);
		inputs[PIECE_COUNT + PIECE_TYPE_COUNT + i] = FLIP_MASK(board->pieces[2 * i]);
	}

	if (get_column(board->king_square[WHITE]) > 3)
		for (int p = 0; p < PIECE_COUNT; p++)
			inputs[p] = mirror_board(inputs[p]);
	if (get_column(board->king_square[BLACK]) > 3)
		for (int p = PIECE_COUNT; p < 2 * PIECE_COUNT; p++)
			inputs[p] = mirror_board(inputs[p]);

	calculate_accumulator(&inputs[0],			acc->raw_values[WHITE], BITS_PER_KING_BUCKET * king_buckets[board->king_square[WHITE]]);
	calculate_accumulator(&inputs[PIECE_COUNT],  acc->raw_values[BLACK], BITS_PER_KING_BUCKET * king_buckets[FLIP_SQUARE(board->king_square[BLACK])]);
}

void update_move_accumulator(bitboard *board, move last_move, int captured_piece, int is_en_passant, accumulator *input_acc, accumulator *output_acc, accumulator_refresh_table *refresh_table)
{
	assert(board);
	assert(input_acc);
	assert(output_acc);

	//Null moves
	if (!last_move.raw) {
		memcpy(output_acc, input_acc, sizeof(accumulator));
		return;
	}

	int piece = (last_move.promote_to) ? PAWN | (FLIP_COLOR(board->turn)) : (!is_en_passant) ? (board->piece_arr[last_move.end_square]) : FLIP_COLOR(captured_piece);

	if (ISKING(piece) &&
		(((get_column(last_move.start_square) > 3) != (get_column(last_move.end_square) > 3)) ||
		 (ISWHITE(piece) && king_buckets[last_move.start_square] != king_buckets[last_move.end_square]) ||
		 (ISBLACK(piece) && king_buckets[FLIP_SQUARE(last_move.start_square)] != king_buckets[FLIP_SQUARE(last_move.end_square)]))) {
			update_accumulator_from_table(board, output_acc, refresh_table);
			return;
		}


	#ifdef VERIFY
		accumulator real_accum_values = {0};

		load_input_accumulator(board, &real_accum_values);
	#endif

	for (int side = WHITE; side <= BLACK; side++) {
		int ksq = (side == WHITE) ? board->king_square[WHITE] : FLIP_SQUARE(board->king_square[BLACK]);

		/** Handle moving piece **/
		int from_sq = last_move.start_square;
		int to_sq = last_move.end_square;
		int piece_offset = piece;

		if (side == BLACK) {
			to_sq = FLIP_SQUARE(to_sq);
			from_sq = FLIP_SQUARE(from_sq);
			piece_offset = FLIP_COLOR(piece_offset);
		}

		//Mirroring
		if (get_column(ksq) > 3) {
			from_sq = MIRROR_SQUARE(from_sq);
			to_sq = MIRROR_SQUARE(to_sq);
		}

		piece_offset = ISWHITE(piece_offset) ? PIECE(piece_offset) / 2 :  (PIECE_TYPE_COUNT) + PIECE(piece_offset) / 2;

		int from_idx, to_idx;

		if (last_move.promote_to) {
			int relative_color = (side == WHITE) ? COLOR(piece) : FLIP_COLOR(piece);
			int promote_piece_offset = ISWHITE(relative_color) ? PIECE(last_move.promote_to) / 2 :  (PIECE_TYPE_COUNT) + PIECE(last_move.promote_to) / 2;

			from_idx = (64 * ((PIECE_COUNT * king_buckets[ksq]) + piece_offset)) + from_sq;
			to_idx = (64 * ((PIECE_COUNT * king_buckets[ksq]) + promote_piece_offset)) + to_sq;
		} else {
			from_idx = (64 * ((PIECE_COUNT * king_buckets[ksq]) + piece_offset)) + from_sq;
			to_idx = (64 * ((PIECE_COUNT * king_buckets[ksq]) + piece_offset)) + to_sq;
		}

		/** Handle captured piece **/
		if (captured_piece != EMPTY_PIECE) {
			int captured_piece_offset = captured_piece;
			int captured_piece_square = last_move.end_square;

			if (is_en_passant) {
				if (ISWHITE(piece))
					captured_piece_square -= 8;
				else
					captured_piece_square += 8;
			}

			if (side == BLACK) {
				captured_piece_offset = FLIP_COLOR(captured_piece_offset);
				captured_piece_square = FLIP_SQUARE(captured_piece_square);
			}

			if (get_column(ksq) > 3)
				captured_piece_square = MIRROR_SQUARE(captured_piece_square);

			captured_piece_offset = ISWHITE(captured_piece_offset) ? PIECE(captured_piece_offset) / 2 :  (PIECE_TYPE_COUNT) + PIECE(captured_piece_offset) / 2;

			int capIdx = (64 * ((PIECE_COUNT * king_buckets[ksq]) + captured_piece_offset)) + captured_piece_square;

			for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j += 16) {
				__m256i v_acc  = _mm256_loadu_si256((__m256i const *)&input_acc->raw_values[side][j]);
				__m256i v_to   = _mm256_loadu_si256((__m256i const *)&weights->accumulator_weights[to_idx][j]);
				__m256i v_from = _mm256_loadu_si256((__m256i const *)&weights->accumulator_weights[from_idx][j]);
				__m256i v_cap  = _mm256_loadu_si256((__m256i const *)&weights->accumulator_weights[capIdx][j]);

				v_acc = _mm256_adds_epi16(v_acc, v_to);
				v_acc = _mm256_subs_epi16(v_acc, v_from);

				v_acc = _mm256_subs_epi16(v_acc, v_cap);

				_mm256_storeu_si256((__m256i *)&output_acc->raw_values[side][j], v_acc);
			}
		}
		/** Handle castled rook (Doesn't support Chess960) **/
		else if (ISKING(piece) && abs(last_move.start_square - last_move.end_square) == 2) {
			int castled_rook_from, castled_rook_to;
			int castled_rook_offset = ROOK | COLOR(piece);

			if (get_column(last_move.end_square) == 6) {
				//Kingside Castle
				castled_rook_from = last_move.start_square + 3;
				castled_rook_to = last_move.start_square + 1;
			} else {
				//Queenside Castle
				castled_rook_from = last_move.start_square - 4;
				castled_rook_to = last_move.start_square - 1;
			}

			if (side == BLACK) {
				castled_rook_offset = FLIP_COLOR(castled_rook_offset);
				castled_rook_from = FLIP_SQUARE(castled_rook_from);
				castled_rook_to = FLIP_SQUARE(castled_rook_to);
			}

			if (get_column(ksq) > 3) {
				castled_rook_from = MIRROR_SQUARE(castled_rook_from);
				castled_rook_to = MIRROR_SQUARE(castled_rook_to);
			}

			castled_rook_offset = ISWHITE(castled_rook_offset) ? PIECE(castled_rook_offset) / 2 :  (PIECE_TYPE_COUNT) + PIECE(castled_rook_offset) / 2;

			int castled_rook_from_idx = (64 * ((PIECE_COUNT * king_buckets[ksq]) + castled_rook_offset)) + castled_rook_from;
			int castled_rook_to_idx = (64 * ((PIECE_COUNT * king_buckets[ksq]) + castled_rook_offset)) + castled_rook_to;

			for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j += 16) {
				__m256i v_acc   = _mm256_loadu_si256((__m256i const *)&input_acc->raw_values[side][j]);

				__m256i v_to	= _mm256_loadu_si256((__m256i const *)&weights->accumulator_weights[to_idx][j]);
				__m256i v_from  = _mm256_loadu_si256((__m256i const *)&weights->accumulator_weights[from_idx][j]);

				v_acc = _mm256_adds_epi16(v_acc, v_to);
				v_acc = _mm256_subs_epi16(v_acc, v_from);

				v_to	= _mm256_loadu_si256((__m256i const *)&weights->accumulator_weights[castled_rook_to_idx][j]);
				v_from  = _mm256_loadu_si256((__m256i const *)&weights->accumulator_weights[castled_rook_from_idx][j]);

				v_acc = _mm256_adds_epi16(v_acc, v_to);
				v_acc = _mm256_subs_epi16(v_acc, v_from);

				_mm256_storeu_si256((__m256i *)&output_acc->raw_values[side][j], v_acc);
			}
		} else {
			for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j += 16) {
				__m256i v_acc   = _mm256_loadu_si256((__m256i const *)&input_acc->raw_values[side][j]);
				__m256i v_to	= _mm256_loadu_si256((__m256i const *)&weights->accumulator_weights[to_idx][j]);
				__m256i v_from  = _mm256_loadu_si256((__m256i const *)&weights->accumulator_weights[from_idx][j]);

				v_acc = _mm256_adds_epi16(v_acc, v_to);
				v_acc = _mm256_subs_epi16(v_acc, v_from);

				_mm256_storeu_si256((__m256i *)&output_acc->raw_values[side][j], v_acc);
			}
		}

		#ifdef VERIFY
			assert(memcmp(&real_accum_values.raw_values[side], &output_acc->raw_values[side], sizeof(output_acc->raw_values[side])) == 0);
		#endif
	}
}

void update_board_accumulator(bitboard *current_board, bitboard *accumulator_board, accumulator *acc, int color)
{
	assert(current_board);
	assert(accumulator_board);
	assert(acc);
	assert((ISWHITE(color) && king_buckets[current_board->king_square[color]] == king_buckets[accumulator_board->king_square[color]]) ||
		   (ISBLACK(color) && king_buckets[FLIP_SQUARE(current_board->king_square[color])] == king_buckets[FLIP_SQUARE(accumulator_board->king_square[color])]));
	assert((get_column(current_board->king_square[color]) > 3) == (get_column(accumulator_board->king_square[color]) > 3));

	uint64_t cur_board[PIECE_COUNT] = {0};
	uint64_t accum_board[PIECE_COUNT] = {0};

	int ksq;
	int tracked_pieces_per_color = PIECE_TYPE_COUNT;

	if (ISWHITE(color)) {
		ksq = accumulator_board->king_square[WHITE];
		for (int i = 0; i < PIECE_TYPE_COUNT; i++) {
			cur_board[i] = current_board->pieces[2 * i];
			cur_board[i + tracked_pieces_per_color] = current_board->pieces[2 * i + 1];

			accum_board[i] = accumulator_board->pieces[2 * i];
			accum_board[i + tracked_pieces_per_color] = accumulator_board->pieces[2 * i + 1];
		}
	} else {
		ksq = FLIP_SQUARE(accumulator_board->king_square[BLACK]);
		for (int i = 0; i < PIECE_TYPE_COUNT; i++) {
			cur_board[i] = FLIP_MASK(current_board->pieces[2 * i + 1]);
			cur_board[tracked_pieces_per_color + i] = FLIP_MASK(current_board->pieces[2 * i]);

			accum_board[i] = FLIP_MASK(accumulator_board->pieces[2 * i + 1]);
			accum_board[tracked_pieces_per_color + i] = FLIP_MASK(accumulator_board->pieces[2 * i]);
		}
	}

	//Mirroring
	if (get_column(ksq) > 3) {
		for (int i = 0; i < PIECE_COUNT; i++) {
			cur_board[i] = mirror_board(cur_board[i]);
			accum_board[i] = mirror_board(accum_board[i]);
		}
	}

	for (int piece = 0; piece < PIECE_COUNT; piece++) {
		uint64_t difference = cur_board[piece]^accum_board[piece];

		while (difference) {
			int square = __builtin_ctzll(difference);

			int feature_index = (64 * (PIECE_COUNT * king_buckets[ksq] + piece)) + square;

			char was_added = cur_board[piece]&(1ull << (square)) ? 1 : 0;

			int16_t *target_acc = acc->raw_values[color];
			int16_t *target_weights = weights->accumulator_weights[feature_index];

			if (was_added) {
				for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 16) {
					__m256i v_acc = _mm256_loadu_si256((__m256i const *)&target_acc[i]);
					__m256i v_weight = _mm256_loadu_si256((__m256i const *)&target_weights[i]);

					v_acc = _mm256_adds_epi16(v_acc, v_weight);

					_mm256_storeu_si256((__m256i *)&target_acc[i], v_acc);
				}
			} else {
				for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 16) {
					__m256i v_acc = _mm256_loadu_si256((__m256i const *)&target_acc[i]);
					__m256i v_w   = _mm256_loadu_si256((__m256i const *)&target_weights[i]);

					v_acc = _mm256_subs_epi16(v_acc, v_w);

					_mm256_storeu_si256((__m256i *)&target_acc[i], v_acc);
				}
			}

			difference &= (difference - 1);
		}
	}

	//We don't need the larger game rule based bitboard arrays.
	//Just copy in what will be used.
	memcpy(accumulator_board->pieces, current_board->pieces, sizeof(current_board->pieces));
	memcpy(accumulator_board->king_square, current_board->king_square, sizeof(current_board->king_square));
}

void update_accumulator_from_table(bitboard *board, accumulator *acc,  accumulator_refresh_table *refresh_table)
{
	assert(board);
	assert(acc);
	assert(refresh_table);

	//Black square is flipped for weight index tracking, but not here in refresh table indices.
	int king_sq_w = board->king_square[WHITE];
	int king_sq_b = FLIP_SQUARE(board->king_square[BLACK]);

	int bucket_idx_w = king_buckets[king_sq_w];
	int bucket_idx_b = king_buckets[king_sq_b];

	if (get_column(king_sq_w) > 3)
		bucket_idx_w += KING_BUCKETS;
	if (get_column(king_sq_b) > 3)
		bucket_idx_b += KING_BUCKETS;

	//Update the accumulator & board in table to match given board.
	if (!refresh_table->initialized[WHITE][bucket_idx_w]) {
		memcpy(refresh_table->boards[WHITE][bucket_idx_w].pieces, board->pieces, sizeof(board->pieces));
		memcpy(refresh_table->boards[WHITE][bucket_idx_w].king_square, board->king_square, sizeof(board->king_square));
		load_input_accumulator(&refresh_table->boards[WHITE][bucket_idx_w], &refresh_table->accumulators[WHITE][bucket_idx_w]);
		refresh_table->initialized[WHITE][bucket_idx_w] = 1;
	} else {
		update_board_accumulator(board, &refresh_table->boards[WHITE][bucket_idx_w], &refresh_table->accumulators[WHITE][bucket_idx_w], WHITE);
	}

	if (!refresh_table->initialized[BLACK][bucket_idx_b]) {
		memcpy(refresh_table->boards[BLACK][bucket_idx_b].pieces, board->pieces, sizeof(board->pieces));
		memcpy(refresh_table->boards[BLACK][bucket_idx_b].king_square, board->king_square, sizeof(board->king_square));
		load_input_accumulator(&refresh_table->boards[BLACK][bucket_idx_b], &refresh_table->accumulators[BLACK][bucket_idx_b]);
		refresh_table->initialized[BLACK][bucket_idx_b] = 1;
	} else {
		update_board_accumulator(board, &refresh_table->boards[BLACK][bucket_idx_b], &refresh_table->accumulators[BLACK][bucket_idx_b], BLACK);
	}

	//Copy updated table values back to board.
	memcpy(acc->raw_values[WHITE], refresh_table->accumulators[WHITE][bucket_idx_w].raw_values[WHITE], sizeof(int16_t) * ACCUMULATOR_NODES_PER_SIDE);
	memcpy(acc->raw_values[BLACK], refresh_table->accumulators[BLACK][bucket_idx_b].raw_values[BLACK], sizeof(int16_t) * ACCUMULATOR_NODES_PER_SIDE);
}
