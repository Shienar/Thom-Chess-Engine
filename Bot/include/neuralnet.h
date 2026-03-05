#ifndef NEURALNETWORK
#define NEURALNETWORK

#include "structs.h"
#include <stdint.h>

#define FLIP(square) ((square)^56)
#define OTHER(color) ((color)^(WHITE|BLACK))

#define INPUT_BITS 81920
#define ACCUMULATOR_NODES_PER_SIDE 256
#define SECOND_HIDDEN_LAYER_NODES 32
#define THIRD_HIDDEN_LAYER_NODES 32
#define OUTPUT_LAYER_NODES 1

//1 bias node with value 1 as an extra input at each layer, with its own set of weights feeding into the next.
#define BIAS_WEIGHTS 1

/**
 * Stored from previous state.
 */
extern uint64_t inputNodes[1280];
extern float accumulator[2][ACCUMULATOR_NODES_PER_SIDE];

/**
 * Weights
 */
typedef struct network_weights_training {
    float weights1[INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    float weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    float weights2[2 * ACCUMULATOR_NODES_PER_SIDE][SECOND_HIDDEN_LAYER_NODES];
    float weights2_bias[SECOND_HIDDEN_LAYER_NODES];
    float weights3[SECOND_HIDDEN_LAYER_NODES][THIRD_HIDDEN_LAYER_NODES];
    float weights3_bias[THIRD_HIDDEN_LAYER_NODES];
    float weights4[THIRD_HIDDEN_LAYER_NODES];
    float weights4_bias;
} network_weights_training;
typedef struct network_weights_playing {
    int8_t weights1[INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    int8_t weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    int8_t weights2[2 * ACCUMULATOR_NODES_PER_SIDE][SECOND_HIDDEN_LAYER_NODES];
    int8_t weights2_bias[SECOND_HIDDEN_LAYER_NODES];
    int8_t weights3[SECOND_HIDDEN_LAYER_NODES][THIRD_HIDDEN_LAYER_NODES];
    int8_t weights3_bias[THIRD_HIDDEN_LAYER_NODES];
    int8_t weights4[THIRD_HIDDEN_LAYER_NODES];
    int8_t weights4_bias;
} network_weights_playing;

extern network_weights_training* trainingNNUE;
extern network_weights_playing* playerNNUE;

/* Binary  file storage. */
void load_trainingWeights();
void save_trainingWeights();

void load_playingWeights();
void save_playingWeights();

void quantizeWeights(network_weights_training* inputFloats, network_weights_playing* outputBytes);

//Clipped ReLU [0, 1] is used throughout
float CReLU_Float(float val, float min, float max);
int8_t CReLU_Int(int8_t val, int8_t min, int8_t max);

/**
 * Uses SIMD to calculate and populate outputValues.
 */
void calculateLayer_Floats(float* inputValues, float* outputValues, int numInputs, int numOutputs, float* weights, float* biasWeights,  int applyCReLU);
void calculateLayer_IntBytes(uint8_t* inputValues, uint8_t* outputValues, int numInputs, int numOutputs, uint8_t* weights, uint8_t* biasWeights,  int applyCReLU);

#endif