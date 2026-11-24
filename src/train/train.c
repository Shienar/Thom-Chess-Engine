#include "train/train.h"
#include "compatibility.h"
#include "board/bitboard.h"
#include "binpack/viri_binpack.h"
#include <stdio.h>
#include <time.h>
#include <math.h>

#ifdef USE_OPENCL
#include "train/opencl/gpu_cl.h"
#else
#include "train/hip/gpu_hip.h"
#endif

uint64_t max_cosine_anneal_timestamp = 10 * MINIBATCHES_PER_EPOCH;
float max_lr = 3e-3f;
float min_lr = 2.5e-5f;

const int lambda_warmup_phase = 5;
const float max_lambda = 0.9f;
const float min_lambda = 0.75f;
float lambda = max_lambda;

//Box-Muller Transform
double sample_normal_distribution(double mean, double stddev)
{
	double u1, u2;

	do { u1 = (double)rand() / RAND_MAX; } while (u1 <= 0.0);
	u2 = (double) rand() / RAND_MAX;
	return mean + stddev * (sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2));
}

void initialize_training_weights(training_weights *raw_weights)
{
	memset(raw_weights, 0, sizeof(training_weights));

	float stddev = sqrtf(2.0 / BITS_PER_KING_BUCKET);

	for (int i = 0; i < INPUT_BITS_PER_SIDE; i++)
		for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
			raw_weights->accumulator_weights[i][j] = _min(MAX_WEIGHT / 2, _max(MIN_WEIGHT / 2, sample_normal_distribution(0.0, stddev)));

	stddev = sqrtf(2.0 / ACCUMULATOR_NODES);
	for (int b = 0; b < OUTPUT_BUCKETS; b++)
		for (int i = 0; i < ACCUMULATOR_NODES; i++)
			raw_weights->output_weights[b][i] = _min(MAX_WEIGHT / 2, _max(MIN_WEIGHT / 2, sample_normal_distribution(0.0, stddev)));
}

void save_raw_weights(training_weights *weights, const char *path)
{
	assert(weights);
	FILE *output = fopen(path, "wb");

	fwrite(weights, sizeof(training_weights), 1, output);
	fclose(output);
}

void quantize_weights(training_weights *raw, nnue_weights *quantized)
{
	for (int i = 0; i < INPUT_BITS_PER_SIDE; i++)
		for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
			quantized->accumulator_weights[i][j] = (int16_t) lroundf(QA * (raw->factorizer_weights[i % BITS_PER_KING_BUCKET][j] + raw->accumulator_weights[i][j]));

	for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
		quantized->accumulator_bias[i] = (int16_t) lroundf(QA * raw->accumulator_bias[i]);

	for (int b = 0; b < OUTPUT_BUCKETS; b++) {
		for (int i = 0; i < ACCUMULATOR_NODES; i++)
			quantized->output_weights[b][i] = (int16_t) lroundf(QB * raw->output_weights[b][i]);
		quantized->output_bias[b] = (int16_t) lroundf(QA * QB * raw->output_bias[b]);
	}
}

void save_quantize_weights(nnue_weights *weights, const char *path)
{
	FILE *output = fopen(path, "wb");

	fwrite(weights, sizeof(nnue_weights), 1, output);
	fclose(output);
}


float sum_loss(float *loss)
{
	__m256 temp_sum = _mm256_setzero_ps();

	for (int i = 0; i < MINIBATCH_SIZE; i += 8)
		temp_sum = _mm256_add_ps(temp_sum, _mm256_loadu_ps(&loss[i]));

	__m128 sum128 = _mm_add_ps(_mm256_castps256_ps128((__m256)temp_sum), _mm256_extractf128_ps(temp_sum, 1));

	sum128 = _mm_hadd_ps(sum128, sum128);
	sum128 = _mm_hadd_ps(sum128, sum128);
	return _mm_cvtss_f32(sum128);
}

void prepare_minibatch_data(binpack_details *details, int input_group, bitboard *board,
							short *active_inputs_A, float *expected_outputs_A, char *output_buckets_A,
							short *active_inputs_B, float *expected_outputs_B, char *output_buckets_B)
{
	short *active_inputs = (input_group == INPUT_GROUP_A) ? active_inputs_A : active_inputs_B;
	float *expected_outputs = (input_group == INPUT_GROUP_A) ? expected_outputs_A : expected_outputs_B;
	char *output_buckets = (input_group == INPUT_GROUP_A) ? output_buckets_A : output_buckets_B;

	for (int entry_number = 0; entry_number < MINIBATCH_SIZE; entry_number++) {
		viri_score score;
		uint8_t result;

		binpack_next(details, board, &score, &result, 1, 0, FEN_SKIP_TRAINING);

		//Convert from white-relative
		if (ISBLACK(board->turn))
			score = -score;

		float relative_result = 0.5f;

		if (result == VICTOR_DRAW)
			relative_result = 0.5f;
		else if (result == VICTOR_WHITE) {
			if (board->turn == WHITE)
				relative_result = 1.0f;
			else
				relative_result = 0.0f;
		} else if (result == VICTOR_BLACK) {
			if (board->turn == BLACK)
				relative_result = 1.0f;
			else
				relative_result = 0.0f;
		}

		expected_outputs[entry_number] = wdl_blend(lambda, score, relative_result);

		uint64_t inputs[2 * PIECE_COUNT] = {0};

		int tracked_pieces_per_color = PIECE_COUNT / 2;

		for (int i = 0; i < PIECE_COUNT / 2; i++) {
			//White's perspective
			inputs[i] = board->pieces[2 * i];
			inputs[tracked_pieces_per_color + i] = board->pieces[2 * i + 1];

			//Black's perspective
			inputs[PIECE_COUNT + i] = FLIP_MASK(board->pieces[2 * i + 1]);
			inputs[PIECE_COUNT + tracked_pieces_per_color + i] = FLIP_MASK(board->pieces[2 * i]);
		}

		if (get_column(board->king_square[WHITE]) > 3)
			for (int p = 0; p < PIECE_COUNT; p++)
				inputs[p] = mirror_board(inputs[p]);
		if (get_column(board->king_square[BLACK]) > 3)
			for (int p = PIECE_COUNT; p < 2 * PIECE_COUNT; p++)
				inputs[p] = mirror_board(inputs[p]);

		for (int color = 0; color < 2; color++) {
			int base_index = (color == WHITE) ? king_buckets[board->king_square[WHITE]] : king_buckets[FLIP_SQUARE(board->king_square[BLACK])];

			base_index *= BITS_PER_KING_BUCKET;

			int tracked_inputs = 0;

			// First half matches side to move.
			// side = 0 if color matches board->turn. side indexes into [entry_number][side][tracked_inputs]
			int side = (color != board->turn);

			for (int piece = 0; piece < PIECE_COUNT; piece++) {
				uint64_t mask = inputs[PIECE_COUNT * color + piece];

				while (mask) {
					active_inputs[entry_number * 64 + 32 * side + tracked_inputs] = base_index + 64 * piece + __builtin_ctzll(mask);

					tracked_inputs++;
					mask &= (mask - 1);
				}
			}

			if (color == 0)
				output_buckets[entry_number] = (tracked_inputs - 2) / 4;

			//-1 terminator
			if (tracked_inputs < 32)
				active_inputs[entry_number * 64 + 32 * side + tracked_inputs] = -1;
		}
	}
}

void train(int epochs, float min_lr, float max_lr, const char *binpack_file_name, const char *kernel_file_name)
{
	epochs = _max(epochs, 1);
	min_lr = min_lr;
	max_lr = max_lr;
	max_cosine_anneal_timestamp = epochs * MINIBATCHES_PER_EPOCH;

	short *active_inputs_A = NULL;
	float *expected_outputs_A = NULL;
	char *output_buckets_A = NULL;

	short *active_inputs_B = NULL;
	float *expected_outputs_B = NULL;
	char *output_buckets_B = NULL;

	float *loss_buffer = NULL;

	int errorcode = init_kernels(kernel_file_name, &active_inputs_A, &expected_outputs_A, &output_buckets_A, &active_inputs_B, &expected_outputs_B, &output_buckets_B, &loss_buffer);

	if (errorcode) {
		printf("Failed to init kernels with error code %d\n", errorcode);
		exit(1);
	}

	binpack_details training_binpack;

	binpack_open(&training_binpack, binpack_file_name, 1);

	int lambda_warmup_phase = 5;

	int total_epochs = 0;
	float total_loss = 0.0; //accumulated value per iteration
	clock_t start_time, end_time;
	double duration_sec;
	int input_group = INPUT_GROUP_A;
	bitboard board;

	do {
		total_loss = 0.0;
		lambda = (total_epochs <= lambda_warmup_phase) ?
					((lambda_warmup_phase - total_epochs) * max_lambda + total_epochs * min_lambda) / lambda_warmup_phase :
					min_lambda;

		start_time = clock();
		for (int minibatch_number = 1; minibatch_number <= MINIBATCHES_PER_EPOCH; minibatch_number++) {
			prepare_minibatch_data(&training_binpack, input_group, &board,
									active_inputs_A, expected_outputs_A, output_buckets_A,
									active_inputs_B, expected_outputs_B, output_buckets_B);

			enqueue_kernels(input_group);
			wait_for_loss_read();

			float loss = sum_loss(loss_buffer);

			total_loss += loss;

			if (minibatch_number % 25 == 0)
				printf("\33[2K\r\tAnalyzed block %d/%d; Loss = %e", minibatch_number, MINIBATCHES_PER_EPOCH, 0.5 * loss / MINIBATCH_SIZE);
			input_group ^= 1;
		}
		end_time = clock();

		//Loss = 0.5 * MSE
		duration_sec = (double) (end_time - start_time) / CLOCKS_PER_SEC;
		total_loss = 0.5f * total_loss/(MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH);
		printf("\33[2K\rEpoch %d Loss = %e (%.1fs at %d pos/sec)\n", ++total_epochs, total_loss, duration_sec, (int) ((MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH) / duration_sec));

		save_gpu_weights();
	} while (total_epochs < epochs);

	free_kernels();
	binpack_close(&training_binpack);

}
