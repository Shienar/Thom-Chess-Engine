#ifndef NEURALNETWORK
#define NEURALNETWORK

#include <math.h>
#include <stdint.h>
#include <immintrin.h>
#include "../board/bitboard.h"

#define QA 255
#define QB 64

#define PI 3.141592653589793

#define ADAM_BETA1 0.9
#define ADAM_BETA2 0.999
#define ADAM_EPSILON 1e-8
#define ADAM_WEIGHT_DECAY 1e-3

#define EVAL_SCALE 400.0f
#define LAMBDA 0.9f
#define SIGMOID(x) (1.0 / (1.0 + exp(-(x))))

#define MINIBATCH_SIZE 16384
#define FILE_COUNT 30518

#define FLIP_SQUARE(x) (x^56)
#define FLIP_MASK(x) __builtin_bswap64(x)

extern network_weights_playing* playerNNUE;
extern network_weights_training* trainingNNUE;

/* Binary  file storage. */
void load_trainingWeights();
void save_trainingWeights();
 
void load_playingWeights();
void save_playingWeights();

void quantizeWeights(network_weights_training* inputFloats, network_weights_playing* outputBytes);

int32_t forwardPropagate(int turn, accumulator* acc);

/**
 * Adaptive Moment Estimation 
 *  - Weight Decay
 *  - Sparse adjustments on input layer.
 * https://arxiv.org/abs/1412.6980v8
 */
void train(int saveEveryNBlocks, int maxIterations, float maxAllowedError);
#endif