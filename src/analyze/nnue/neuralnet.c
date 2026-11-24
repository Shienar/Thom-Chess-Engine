#include "analyze/nnue/neuralnet.h"
#include "board/bitboard.h"
#include "analyze/search.h"
#include <immintrin.h>

/**
 * - Index X contains the king bucket index at square X.
 * - Created for white's perspective; must be flipped for black.
 * - Mirrored accross middle of board, split between d/e files.
 *	  - When king column > d, the squares & masks of all pieces get mirrored.
 *
 * topleft: a1
 * bottomright: h8
 */
int king_buckets[64] = {
	0, 1, 2, 3,	 3, 2, 1, 0,
	4, 4, 5, 5,	 5, 5, 4, 4,
	6, 6, 6, 6,	 6, 6, 6, 6,
	7, 7, 7, 7,	 7, 7, 7, 7,
	8, 8, 8, 8,	 8, 8, 8, 8,
	8, 8, 8, 8,	 8, 8, 8, 8,
	9, 9, 9, 9,	 9, 9, 9, 9,
	9, 9, 9, 9,	 9, 9, 9, 9,
};

int is_network_loaded = 1;
int use_nnue = 1;
nnue_weights *weights = (nnue_weights *)weights_bin_start;

void init_nnue(void)
{
	is_network_loaded = (weights_bin_end - weights_bin_start) > 0;
	if (is_network_loaded) {
		const size_t parameter_count = sizeof(nnue_weights) / sizeof(int16_t);
		int16_t *ptr = (int16_t *) weights;

		for (int i = 0; i < parameter_count; i++)
			ptr[i] = little_endian_16(ptr[i]);
	} else
		use_nnue = 0;
}

//Lizard SCReLU.
//https://chessprogramming.org/NNUE#lizard-screlu
int calculate_output_layer(int16_t *input_values_a, int16_t *input_values_b, int16_t weights[ACCUMULATOR_NODES], int16_t bias)
{
	const __m256i v_zero = _mm256_setzero_si256();
	const __m256i v_max = _mm256_set1_epi16(QA);

	__m256i v_output = _mm256_setzero_si256();

	for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 16) {
		__m256i v_us = _mm256_loadu_si256((const __m256i *)&input_values_a[i]);
		__m256i v_them = _mm256_loadu_si256((const __m256i *)&input_values_b[i]);

		__m256i v_weights_us = _mm256_loadu_si256((const __m256i *)&weights[i]);
		__m256i v_weights_them = _mm256_loadu_si256((const __m256i *)&weights[ACCUMULATOR_NODES_PER_SIDE + i]);

		//Clamp
		v_us = _mm256_min_epi16(_mm256_max_epi16(v_us, v_zero), v_max);
		v_them = _mm256_min_epi16(_mm256_max_epi16(v_them, v_zero), v_max);

		//SCReLU + Forward pass weight multiplication.
		__m256i v_us_output = _mm256_madd_epi16(_mm256_mullo_epi16(v_weights_us, v_us), v_us);
		__m256i v_them_output = _mm256_madd_epi16(_mm256_mullo_epi16(v_weights_them, v_them), v_them);

		v_output = _mm256_add_epi32(v_output, v_us_output);
		v_output = _mm256_add_epi32(v_output, v_them_output);
	}

	//Horizontal reduction:
	__m128i v_sum128 = _mm_add_epi32(_mm256_castsi256_si128(v_output), _mm256_extracti128_si256(v_output, 1));

	v_sum128 = _mm_add_epi32(v_sum128, _mm_shuffle_epi32(v_sum128, _MM_SHUFFLE(1, 0, 3, 2)));
	v_sum128 = _mm_add_epi32(v_sum128, _mm_shuffle_epi32(v_sum128, _MM_SHUFFLE(2, 3, 0, 1)));

	//QA * QA * QB
	int output = _mm_cvtsi128_si32(v_sum128);

	//QA * QB
	output >>= QA_RSHIFT;
	output += bias;

	//EVAL_SCALE * QA * QB
	output *= EVAL_SCALE;

	//EVAL_SCALE
	return output >> (QA_RSHIFT + QB_RSHIFT);
}

void lazy_accumulator_update(thread_context *context, int ply)
{
	for (int next_ply = context->last_clean_ply + 1; next_ply <= ply; next_ply++) {
		bitboard *cur_board = &context->board_stack[next_ply - 1];
		bitboard *next_board = &context->board_stack[next_ply];
		move *current_move = &context->move_stack[next_ply - 1];

		int piece = (cur_board->piece_arr[current_move->start_square]);
		int captured_piece = (cur_board->piece_arr[current_move->end_square]);
		int is_ep = 0;

		if (captured_piece == EMPTY_PIECE && ISPAWN(piece) && current_move->end_square == cur_board->ep_square) {
			captured_piece = FLIP_COLOR(piece);
			is_ep = 1;
		}

		update_move_accumulator(next_board, *current_move, captured_piece, is_ep, &context->accumulator_stack[next_ply - 1], &context->accumulator_stack[next_ply], context->refresh_table);
		context->is_acc_clean[next_ply] = 1;
		context->last_clean_ply = ply;
	}
}

const int OUTPUT_BUCKET_DIVISOR = (32 + OUTPUT_BUCKETS - 1) / OUTPUT_BUCKETS;
int forward_propagate(thread_context *context, int ply)
{
	bitboard *board = &context->board_stack[ply];
	accumulator *acc = &context->accumulator_stack[ply];

	lazy_accumulator_update(context, ply);

	int bucket = (__builtin_popcountll(board->pieces_all) - 2) / OUTPUT_BUCKET_DIVISOR;
	int output = calculate_output_layer(acc->raw_values[board->turn],
									  acc->raw_values[FLIP_COLOR(board->turn)],
									  weights->output_weights[bucket],
									  weights->output_bias[bucket]);

	return _min(MIN_MATE_SCORE - 1, _max(-(MIN_MATE_SCORE - 1), output));
}
