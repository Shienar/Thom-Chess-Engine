#ifndef NEURALNETWORK
#define NEURALNETWORK

#include <math.h>
#include <stdint.h>
#include <immintrin.h>
#include "board/bitboard.h"
#include "analyze/nnue/accumulator.h"

#define PI 3.141592653589793

#define EVAL_SCALE 400
#define FLIP_MASK(x) __builtin_bswap64(x)

extern int isNetworkLoaded;
extern int useNNUE;
extern const unsigned char weights_bin_start[];
extern const unsigned char weights_bin_end[];

extern nnue_weights* weights;

void initNNUE();

int forwardPropagate(bitboard* board, accumulator* acc);

#endif