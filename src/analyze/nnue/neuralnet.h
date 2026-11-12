#pragma once

#include <stdint.h>
#include "board/board.h"

#define QA 255
#define QB 64
#define QA_RSHIFT 8
#define QB_RSHIFT 6
#define EVAL_SCALE 400

#define INPUT_BITS_PER_SIDE (BITS_PER_KING_BUCKET * KING_BUCKETS)
#define ACCUMULATOR_NODES 512
#define ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES / 2)
#define VECTOR_ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES_PER_SIDE / 4)

#define KING_BUCKETS 10
#define BITS_PER_KING_BUCKET 768

#define OUTPUT_BUCKETS 8

typedef struct accumulator {
    int16_t rawValues[2][ACCUMULATOR_NODES_PER_SIDE];
} accumulator;

/**
 * First half of indices are on king buckets on files a-d.
 * Second half of indices are on king buckets on files e-h.
 * 
 * Full refreshes won't be necessary when moving within a 
 * king bucket on the same half of board.
 */
typedef struct accumulatorRefreshTable {
    bitboard boards[2][2 * KING_BUCKETS];
    accumulator accumulators[2][2 * KING_BUCKETS];
    uint8_t initialized[2][2 * KING_BUCKETS];
} accumulatorRefreshTable;

typedef struct __attribute__((aligned(64))) nnue_weights {
    int16_t accumulator_weights[INPUT_BITS_PER_SIDE][ACCUMULATOR_NODES_PER_SIDE];
    int16_t accumulator_bias[ACCUMULATOR_NODES_PER_SIDE];
    int16_t output_weights[OUTPUT_BUCKETS][ACCUMULATOR_NODES];
    int16_t output_bias[OUTPUT_BUCKETS];
} nnue_weights;

typedef struct __attribute__((aligned(64))) training_weights {
    float factorizer_weights[BITS_PER_KING_BUCKET][ACCUMULATOR_NODES_PER_SIDE];
    float accumulator_weights[INPUT_BITS_PER_SIDE][ACCUMULATOR_NODES_PER_SIDE];
    float accumulator_bias[ACCUMULATOR_NODES_PER_SIDE];
    float output_weights[OUTPUT_BUCKETS][ACCUMULATOR_NODES];
    float output_bias[OUTPUT_BUCKETS];
} training_weights;

//https://www.chessprogramming.org/Flipping_Mirroring_and_Rotating#Vertical
#define k1 0x5555555555555555
#define k2 0x3333333333333333
#define k4 0x0f0f0f0f0f0f0f0f
static inline uint64_t mirrorBoard(uint64_t x)
{
   x = ((x >> 1) & k1) | ((x & k1) << 1);
   x = ((x >> 2) & k2) | ((x & k2) << 2);
   x = ((x >> 4) & k4) | ((x & k4) << 4);
   return x;
}

#define FLIP_MASK(x) __builtin_bswap64(x)

extern int kingBuckets[64];

extern int isNetworkLoaded;
extern int useNNUE;
extern const unsigned char weights_bin_start[];
extern const unsigned char weights_bin_end[];

extern nnue_weights* weights;
extern const int OUTPUT_BUCKET_DIVISOR;

void initNNUE();
int forwardPropagate(struct threadContext* context, int ply);