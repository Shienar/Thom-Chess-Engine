#ifndef NEURALNETWORK
#define NEURALNETWORK

#include <math.h>
#include <stdint.h>
#include <immintrin.h>
#include "board/bitboard.h"

#define RAW_PATH PROJECT_CWD "/import/raw.nnue"
#define QUANTIZED_PATH PROJECT_CWD "/import/quantized.nnue"
#define PI 3.141592653589793

#define FLIP_SQUARE(x) (x^56)
#define FLIP_MASK(x) __builtin_bswap64(x)

extern training_weights* raw_weights;
extern quantized_weights* int_weights;

/* Binary  file storage. */
void loadRawWeights();
void saveRawWeights();
void quantizeWeights(training_weights* inputFloats, quantized_weights* outputInts);
void loadQuantizedWeights();
void saveQuantizedWeights();

//https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm
void print_network_statistics();

int forwardPropagate(bitboard* board, accumulator* acc);

#endif