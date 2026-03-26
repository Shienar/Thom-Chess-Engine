#ifndef NEURALNETWORK
#define NEURALNETWORK

#include "../structs.h"
#include "accumulator.h"
#include <stdint.h>

#define NUMBER_OF_BLOCKS 1000 // Training data positions should be evenly divisible by this amount for simplicity. Excess data points get ignored.
#define LEARNING_RATE 0.1

#define FLIP_SQUARE(x) (x^56)
#define FLIP_MASK(x) __builtin_bswap64(x)

extern network_weights_training* trainingNNUE;
extern network_weights_playing* playerNNUE;

/* Binary  file storage. */
void load_trainingWeights();
void save_trainingWeights();

void load_playingWeights();
void save_playingWeights();

void quantizeWeights(network_weights_training* inputFloats, network_weights_playing* outputBytes);

//Clipped ReLU [0, 1] is used throughout
float SCReLU_Float(float val, float min, float max);
int8_t SCReLU_Int(int8_t val, int8_t min, int8_t max);

/**
 * Uses SIMD to calculate and populate outputValues.
 */
void calculateLayer_Floats(float* inputValues, float* outputValues, int numInputs, int numOutputs, float weights[numInputs][numOutputs], float* biasWeights,  int applyCReLU);
void calculateLayer_IntBytes(int8_t* inputValues, int8_t* outputValues, int numInputs, int numOutputs, int8_t weights[numInputs][numOutputs], int8_t* biasWeights,  int applyCReLU);

float forwardPropagate_Float(int turn, accumulator_training* floatAccumulator);
int8_t forwardPropagate_Int(int turn, accumulator_playing* byteAccumulator);

void backpropagate(int saveEveryNIterations, int maxIterations, float maxAllowedError, accumulator_training* floatAccumulator);

void generateTrainingData(int depth, int maxTime, int maxPositions, accumulator_training* floatAccumulator);
void updateTrainingData(int depth, int maxTime);
#endif