#ifndef NEURALNETWORK
#define NEURALNETWORK

#include "../structs.h"
#include "accumulator.h"
#include <math.h>
#include <stdint.h>

#define ADAM_LEARNING_RATE 1e-3
#define ADAM_BETA1 0.9
#define ADAM_BETA2 0.999
#define ADAM_EPSILON 1e-8

#define POSITIONS_PER_FILE 10000
#define FILE_COUNT 10000

#define FLIP_SQUARE(x) (x^56)
#define FLIP_MASK(x) __builtin_bswap64(x)

//#define SCReLU(val, min, max) ((val <= min) ? min : ((val >= max) ? max : val*val))
#define SCReLU(val, min, max) ((val <= min) ? 0.01 * val : ((val >= max) ? max + 0.01 * (val - max) : val*val))
#define SCReLU_Derivative(val, min, max) ((val <= min || val >= max) ? (0.0) : (2.0*val))

extern network_weights_training* trainingNNUE;
extern network_weights_playing* playerNNUE;

/* Binary  file storage. */
void load_trainingWeights();
void save_trainingWeights();

void load_playingWeights();
void save_playingWeights();

void quantizeWeights(network_weights_training* inputFloats, network_weights_playing* outputBytes);

/**
 * Uses SIMD to calculate and populate outputValues.
 */
void calculateLayer_Floats(float* inputValues, float* outputValues, int numInputs, int numOutputs, float weights[numOutputs][numInputs], int inputOffset, float* biasWeights,  int applyCReLU);
void calculateLayer_IntBytes(int8_t* inputValues, int8_t* outputValues, int numInputs, int numOutputs, int8_t weights[numOutputs][numInputs], int inputOffset, int8_t* biasWeights,  int applyCReLU);

float forwardPropagate_Float(int turn, accumulator_training* floatAccumulator, int centerAndScaleAtZero);
int8_t forwardPropagate_Int(int turn, accumulator_playing* byteAccumulator, int centerAtZero);

// Adaptive Movement Estimation (ADAM)
// https://arxiv.org/abs/1412.6980v8
void train(int saveEveryNBlocks, int maxIterations, float maxAllowedError, accumulator_training* floatAccumulator);

void generateTrainingData(int depth, int maxTime, accumulator_training* floatAccumulator);
#endif