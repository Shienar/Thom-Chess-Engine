#ifndef NEURALNET
#define NEURALNET

#include "matrix.h"

#define MINIMUM_ERROR 0.000000001

/**
 * Each layer's value is represented
 * as a column.
 */

/**
 * Half-King-Piece Relationship (HalfKP)
 *  - Inputs 0-40959 = player
 *  - Inputs 40960-81919 = opponent
 */
#define INPUT_NODES 81920

/**
 * Divided into two halves of 256 nodes per side.
 * Each half is connected to a corresponding half of the input layer.
 * 
 * The edge weights of both halves are identical.
 * Mirrored piece-square relations have the same weight.
 */
#define HIDDEN_LAYER1_NODES 512

/**
 * Fully connected to previous layers
 */
#define HIDDEN_LAYER2_NODES 32
#define HIDDEN_LAYER3_NODES 32

/**
 * Evaluation (centipawns)
 */
#define OUTPUT_NODES 1

matrix* weights1;
matrix* weights2;
matrix* weights3;
matrix* weights4;

/**
 * Initializes edge weights.
 */
void init_neuralnet();

/**
 * Error = 0.5 * sum of (expected_output_i - output_i)^2
 * outputs are column vectors
 */
double net_error(matrix* output, matrix* expected_output);

/**
 * Range of [0, 1]
 */
double relu(double x);

/**
 * Given a INPUT_NODES x 1 matrix, returns a OUTPUT_NODES x 1 matrix
 */
matrix* forward_propagate(matrix* input);

/**
 * Performs forward propagation then
 * backpropagates to update weight matrices
 * based on error.
 */
void back_propagate(input);

/**
 * 
 */
void build_network();


#endif