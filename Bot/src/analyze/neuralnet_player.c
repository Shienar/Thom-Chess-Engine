#include "neuralnet.h"
#include "../debug.h"
#include "../board/bitboard.h"
#include "../board/moves.h"
#include "engine.h"
#include <math.h>
#include <float.h>
#include <immintrin.h>
#include <windows.h>

network_weights_playing* playerNNUE = NULL;

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
        if(trainingNNUE) 
        {
            quantizeWeights(trainingNNUE, playerNNUE);
            save_trainingWeights();
        }
        else
        {
            load_trainingWeights();
            quantizeWeights(trainingNNUE, playerNNUE);
            save_trainingWeights();

            FREE(trainingNNUE);
            trainingNNUE = NULL;
        }
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
        DEBUG("Failed to write neural network to file.");
    }
    fclose(output);
}

//__m256i stores 32 8-bit ints (epi8 = extended packed 8-bit integer (signed))
void calculateLayer_IntBytes(int8_t* inputValues, int8_t* outputValues, int numInputs, int numOutputs, int8_t weights[numOutputs][numInputs], int inputOffset, int8_t* biasWeights,  int applyCReLU)
{
    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex++)
    {
        __m256i output = _mm256_setzero_si256(); //si256 = 256-bit signed integer.

        //All layer lengths are divisible by 32 so no overflow.
        for(int inputIndex = 0; inputIndex < numInputs; inputIndex+=32)
        {
            __m256i inputBatch = _mm256_loadu_si256((__m256i const*) &inputValues[inputIndex]);

            //Weights are an array of float w[OUTPUT NODES][INPUT NODES]
            __m256i weightsBatch = _mm256_loadu_si256((__m256i const*) &weights[outputIndex][inputIndex + inputOffset]);

            //Multiply inputs by weights.
            //Outputs 32-bit results. Neighboring values get summed.
            __m256i tempProduct = _mm256_maddubs_epi16 (inputBatch, weightsBatch);

            //Extend to 32-bit results. Neighboring values_mm512_dpbusd_epi32(acc, a, b); get summed.
            tempProduct = _mm256_madd_epi16(tempProduct, _mm256_set1_epi16(1));
            
            //Add temp products to intermediate registers.
            output = _mm256_add_epi32(output, tempProduct);
        }

        //Sum the output integers.
        __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(output), _mm256_extracti128_si256(output, 1));
        sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_BADC)); // A = A + B; C = C + D;
        sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_CDAB)); // A = A + C = (original) A + B + C + D
        int totalSum =  _mm_cvtsi128_si32(sum128);

        if(biasWeights) totalSum+= biasWeights[outputIndex];

        if(totalSum >= INT8_MAX) totalSum = INT8_MAX;
        else if(totalSum <= INT8_MIN) totalSum = INT8_MIN;
        
        outputValues[outputIndex] = totalSum;

        if(applyCReLU) outputValues[outputIndex] = SCReLU(outputValues[outputIndex], 0, 127);
    }
}

int8_t forwardPropagate_Int(int turn, accumulator_playing* byteAccumulator, int centerAtZero)
{
    //Assume accumulator has already been updated.
    int8_t h2[SECOND_HIDDEN_LAYER_NODES] = {0};
    int8_t h3[THIRD_HIDDEN_LAYER_NODES] = {0};
    int8_t outputNode = 0;

    int8_t tempH2[2][SECOND_HIDDEN_LAYER_NODES];

    //accumulator[0] = white
    //accumulator[1] = black
    //trainingNNUE->weights2[0 to ACCUMULATOR_NODES_PER_SIDE] = current side to move
    //trainingNNUE->weights2[ACCUMULATOR_NODES_PER_SIDE to 2* ACCUMULATOR_NODES_PER_SIDE -1] = opponent's side
    if(ISWHITE(turn))
    {
        calculateLayer_IntBytes(byteAccumulator->accumulator[0], tempH2[0], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, playerNNUE->weights2, 0, playerNNUE->weights2_bias, 0);
        calculateLayer_IntBytes(byteAccumulator->accumulator[1], tempH2[1], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, playerNNUE->weights2, ACCUMULATOR_NODES_PER_SIDE, NULL, 0);
    }
    else
    {
        calculateLayer_IntBytes(byteAccumulator->accumulator[0], tempH2[0], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, playerNNUE->weights2, ACCUMULATOR_NODES_PER_SIDE, playerNNUE->weights2_bias, 0);
        calculateLayer_IntBytes(byteAccumulator->accumulator[1], tempH2[1], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, playerNNUE->weights2, 0, NULL, 0);
    }
    

    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        h2[i] = SCReLU((tempH2[0][i] + tempH2[1][i]), 0, 127);
    }

    calculateLayer_IntBytes(h2, h3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, playerNNUE->weights3, 0, playerNNUE->weights3_bias, 1);
    calculateLayer_IntBytes(h3, &outputNode, THIRD_HIDDEN_LAYER_NODES, OUTPUT_LAYER_NODES, &playerNNUE->weights4, 0, &playerNNUE->weights4_bias, 0);

    //Change from [0, 127] to [-64, 63]
    // (It's trained to generate a value in the range but could theoretically generate one outside of it)
    if(centerAtZero) outputNode-=64;

    return outputNode;
}