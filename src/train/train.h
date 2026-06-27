#ifndef NEURALNET_TRAIN
#define NEURALNET_TRAIN

#include "analyze/neuralnet.h"

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8f
#define ADAM_WEIGHT_DECAY 1e-2f

#define EVAL_SCALE 400.0f
#define OUTPUT_SCALE 16
#define LAMBDA 0.9f
#define SIGMOID(x) (1.0 / (1.0 + exp(-(x))))

#define MINIBATCH_SIZE 16384
#define MINIBATCHES_PER_EPOCH 6104 // 6,104 * 16,384 = 100,007,936

//Every FEN_SKIPth entry is saved. Offsets are set to minibatchNumber%FEN_SKIP
#define FEN_SKIP 25

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