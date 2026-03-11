#ifndef NEURALNETWORK
#define NEURALNETWORK

#include "structs.h"
#include <stdint.h>

#define FLIP(square) ((square)^56)
#define OTHER(color) ((color)^(WHITE|BLACK))

#define INPUT_BITS 81920
#define HALF_INPUT_BITS 40960
#define ACCUMULATOR_NODES_PER_SIDE 256
#define SECOND_HIDDEN_LAYER_NODES 32
#define THIRD_HIDDEN_LAYER_NODES 32
#define OUTPUT_LAYER_NODES 1

#define LEARNING_RATE 0.1
#define POSITIONS_PER_FILE 1 //100000
#define NUMBER_OF_TRAINING_FILES 1 //1000

/**
 * Weights
 * 
 * Each index in the inputNodes array is a bitboard.
 * To find the bitboard of PIECE while COLOR's
 * king is on SQUARE, use the following formula.
 * 
 * i = (640 * ISBLACK(COLOR)) + (10 * SQUARE) + PIECE
 *  - PIECE
 *      - Ally Pawn = 0
 *      - Ally Knight = 1
 *      - Ally Bishop = 2
 *      - Ally Rook = 3
 *      - Ally Queen = 4
 *      - Enemy Pawn = 5
 *      - Enemy Knight = 6
 *      - Enemy Bishop = 7
 *      - Enemy Rook = 8
 *      - Enemy Queen = 9
 */
typedef struct network_weights_training {
    float weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    float weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    float weights2[2 * ACCUMULATOR_NODES_PER_SIDE][SECOND_HIDDEN_LAYER_NODES];
    float weights2_bias[SECOND_HIDDEN_LAYER_NODES];
    float weights3[SECOND_HIDDEN_LAYER_NODES][THIRD_HIDDEN_LAYER_NODES];
    float weights3_bias[THIRD_HIDDEN_LAYER_NODES];
    float weights4[THIRD_HIDDEN_LAYER_NODES];
    float weights4_bias;

    //Incrementally updates.
    uint64_t inputNodes[1280];
    float accumulator[2][ACCUMULATOR_NODES_PER_SIDE]; //[0][i] = white; [1][i] = black;
    
    //Saved for use in backpropagation.
    float h2[SECOND_HIDDEN_LAYER_NODES];
    float h3[THIRD_HIDDEN_LAYER_NODES];
    float outputNode;
} network_weights_training;
typedef struct network_weights_playing {
    int8_t weights1[HALF_INPUT_BITS][ACCUMULATOR_NODES_PER_SIDE];
    int8_t weights1_bias[ACCUMULATOR_NODES_PER_SIDE];
    int8_t weights2[2 * ACCUMULATOR_NODES_PER_SIDE][SECOND_HIDDEN_LAYER_NODES];
    int8_t weights2_bias[SECOND_HIDDEN_LAYER_NODES];
    int8_t weights3[SECOND_HIDDEN_LAYER_NODES][THIRD_HIDDEN_LAYER_NODES];
    int8_t weights3_bias[THIRD_HIDDEN_LAYER_NODES];
    int8_t weights4[THIRD_HIDDEN_LAYER_NODES];
    int8_t weights4_bias;

    uint64_t inputNodes[1280];
    int8_t accumulator[2][ACCUMULATOR_NODES_PER_SIDE]; //[0][i] = white; [1][i] = black;
} network_weights_playing;

#define TRAINING_NNUE 1 //Floats
#define PLAYER_NNUE 2 //Signed 8-bit ints
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

/**
 * Reinitializes input nodes and accumulator based off of a bitboard.
 */
void loadInputAccumulator(bitboard* board, int networkType);

/**
 * Incremental update of input nodes and accumulator.
 * Call after making or unmaking a move with the move that was
 * pushed or popped off of the stack.
 */
void updateMoveAccumulator(bitboard* board, move* lastMove, int networkType, int shouldUndoMove);

float forwardPropagate_Float();
int8_t forwardPropagate_Int();

void backpropagate(int saveEveryNIterations, int maxIterations, float maxAllowedError);

#endif