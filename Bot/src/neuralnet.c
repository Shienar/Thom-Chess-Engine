#include "../include/neuralnet.h"
#include "../include/debug.h"
#include <math.h>
#include <tmmintrin.h> 
#include <float.h>

uint64_t inputNodes[1280] = {0};
float accumulator[2][ACCUMULATOR_NODES_PER_SIDE] = {0};
network_weights_training* trainingNNUE = NULL;
network_weights_playing* playerNNUE = NULL;

void iterateTrainingWeights(void (*func)(float*, float*), network_weights_training* trainingWeights, float* context) 
{
    if(!func || !trainingWeights)
    {
        DEBUG("Passed null arguments to iterator.")
        return;
    }
    for(int i = 0; i < INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            func(&trainingWeights->weights1[i][j], context);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) func(&trainingWeights->weights1_bias[i], context);

    for(int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            func(&trainingWeights->weights2[i][j], context);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) func(&trainingWeights->weights2_bias[i], context);
    
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            func(&trainingWeights->weights3[i][j], context);
        }
    }
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) func(&trainingWeights->weights3_bias[i], context);
    
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        func(&trainingWeights->weights4[i], context);
    }
    func(&trainingWeights->weights4_bias, context);
}

/**
 * Box-Muller transform.
 */
void sampleNormalDistribution(float* dest, float* standardDeviation) 
{
    double u1; 
    do { u1 = (double)rand() / (double) RAND_MAX; } while(u1 == 0);
    *dest = *standardDeviation * sqrt(-2.0 * log(u1)) * cos(2 * 6.283185307179586 *  (double)rand()/(double)RAND_MAX);
}

void load_trainingWeights()
{
    if(!trainingNNUE) trainingNNUE = CALLOC(1, sizeof(network_weights_training));

    FILE* input = fopen("import/NNUE_Training.bin", "rb");
    if(input)
    {
        fread(trainingNNUE, sizeof(network_weights_training), 1, input);
        fclose(input);
    }
    else
    {
        DEBUG("Failed to load neural network from file.\n");

        float standardDeviation = sqrt(2/INPUT_BITS);
        iterateTrainingWeights(sampleNormalDistribution, trainingNNUE, &standardDeviation);

    }
}

void save_trainingWeights()
{
    FILE* output = fopen("import/NNUE_Training.bin", "wb");
    if(output)
    {
        fwrite(trainingNNUE, sizeof(network_weights_training), 1, output);
    }
    else
    {
        DEBUG("Failed to write neural network to file.")
    }
    fclose(output);
}

void load_playingWeights()
{
    if(!playerNNUE) playerNNUE = CALLOC(1, sizeof(network_weights_training));

    FILE* input = fopen("import/NNUE_Player.bin", "rb");
    if(input)
    {
        fread(playerNNUE, sizeof(network_weights_training), 1, input);
        fclose(input);
    }
    else
    {
        load_trainingWeights();
        quantizeWeights(trainingNNUE, playerNNUE);

        FREE(trainingNNUE);
        trainingNNUE = NULL;
    }
}

void save_playingWeights()
{
    FILE* output = fopen("import/NNUE_Player.bin", "wb");
    if(output)
    {
        fwrite(playerNNUE, sizeof(network_weights_playing), 1, output);
    }
    else
    {
        DEBUG("Failed to write neural network to file.")
    }
    fclose(output);
}

void findAbsMax(float* comparedValue, float* max)
{
    if(fabsf(*comparedValue) > *max) *max = fabsf(*comparedValue);
}

void quantizeWeights(network_weights_training* inputFloats, network_weights_playing* outputBytes)
{
    if(!inputFloats || !outputBytes) 
    {
        DEBUG("Cannot quantize with null pointers.")
        return;
    }

    float maxValue = -FLT_MAX;
    iterateTrainingWeights(findAbsMax, inputFloats, &maxValue);
    
    float scalingFactor = maxValue / 127;
    for(int i = 0; i < INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            outputBytes->weights1[i][j] = (uint8_t) roundf(inputFloats->weights1[i][j] / scalingFactor);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) outputBytes->weights1_bias[i] = (uint8_t) roundf(inputFloats->weights1_bias[i] / scalingFactor);

    for(int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights2[i][j] = (uint8_t) roundf(inputFloats->weights2[i][j] / scalingFactor);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) outputBytes->weights2_bias[i] = (uint8_t) roundf(inputFloats->weights2_bias[i] / scalingFactor);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights3[i][j] = (uint8_t) roundf(inputFloats->weights3[i][j] / scalingFactor);
        }
    }
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) outputBytes->weights3_bias[i] = (uint8_t) roundf(inputFloats->weights3_bias[i] / scalingFactor);
    
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        outputBytes->weights4[i] = (uint8_t) roundf(inputFloats->weights4[i] / scalingFactor);
    }
    outputBytes->weights4_bias = (uint8_t) roundf(inputFloats->weights4_bias / scalingFactor);
}

int CReLU(int16_t val, int16_t min, int16_t max)
{
    if(val <= min) return min;
    if(val >= max) return max;
    return val;
}

void calculateLayer(float* inputValues, float* outputValues, int numInputs, int numOutputs, float* weights, float* biasWeights,  int applyCReLU)
{
    
}