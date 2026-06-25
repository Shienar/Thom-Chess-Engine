#ifndef NEURALNETWORK
#define NEURALNETWORK

#include <math.h>
#include <stdint.h>
#include <immintrin.h>
#include "board/bitboard.h"

#define PI 3.141592653589793

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8f
#define ADAM_WEIGHT_DECAY 1e-2f

#define EVAL_SCALE 400.0f
#define OUTPUT_SCALE 8
#define LAMBDA 0.9f
#define SIGMOID(x) (1.0 / (1.0 + exp(-(x))))

#define MINIBATCH_SIZE 16384
#define MINIBATCHES_PER_EPOCH 6104 // 6,104 * 16,384 = 100,007,936

//Every FEN_SKIPth entry is saved. Offsets are set to minibatchNumber%FEN_SKIP
#define FEN_SKIP 25

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

/**
 * Ranger
 *      - https://arxiv.org/pdf/2106.13731
 * 
 *  - Adaptive Moment Estimation 
 *          - https://arxiv.org/abs/1412.6980v8
 *  - Weight Decay
 *  - RAdam
 *          - https://arxiv.org/pdf/1908.03265
 *  - Lookahead
 *          - https://proceedings.neurips.cc/paper_files/paper/2019/file/90fd4f88f588ae64038134f1eeaa023f-Paper.pdf
 */
void train( int maxIterations, float maxAllowedError);

#pragma pack(push, 1) // no padding
typedef struct {
    uint64_t occupancy;   // Bitboard of all pieces
    uint8_t  pieces[16];  // 4-bits per piece (bits then low bits)
    uint8_t  flags;  // LSB=Turn, bits 1,2,3 for Win/Loss/Draw
    int16_t  evaluation;  // Score
} CompactPosition;
#pragma pack(pop)

#endif