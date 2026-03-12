#include "../include/neuralnet.h"
#include "../include/debug.h"
#include "../include/bitboard.h"
#include "../include/moves.h"
#include "../include/engine.h"
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
        DEBUG("Failed to write neural network to file.")
    }
    fclose(output);
}

int8_t SCReLU_Int(int8_t val, int8_t min, int8_t max)
{
    if(val <= min) return min*min;
    if(val >= max) return max*max;
    return val*val;
}

//__m256i stores 32 8-bit ints (epi8 = extended packed 8-bit integer (signed))
void calculateLayer_IntBytes(int8_t* inputValues, int8_t* outputValues, int numInputs, int numOutputs, int8_t weights[numInputs][numOutputs], int8_t* biasWeights,  int applyCReLU)
{
    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex++)
    {
        __m256i intermediate1 = _mm256_setzero_si256(); //si256 = 256-bit signed integer.
        __m256i intermediate2 = _mm256_setzero_si256();
        __m256i intermediate3 = _mm256_setzero_si256();
        __m256i intermediate4 = _mm256_setzero_si256();

        //All layer lengths are divisible by 32 so no overflow.
        for(int inputIndex = 0; inputIndex < numInputs; inputIndex+=32)
        {
            __m256i inputBatch1 = _mm256_loadu_si256((__m256i const*) &inputValues[inputIndex]);
            __m256i inputBatch2 = _mm256_loadu_si256((__m256i const*) &inputValues[inputIndex + 8]);
            __m256i inputBatch3 = _mm256_loadu_si256((__m256i const*) &inputValues[inputIndex + 16]);
            __m256i inputBatch4 = _mm256_loadu_si256((__m256i const*) &inputValues[inputIndex + 24]);

            //Weights are an array of float w[INPUT NODES][OUTPUTS NODES]
            __m256i weightsBatch1 = _mm256_loadu_si256((__m256i const*) &weights[inputIndex][outputIndex]);
            __m256i weightsBatch2 = _mm256_loadu_si256((__m256i const*) &weights[inputIndex + 8][outputIndex]);
            __m256i weightsBatch3 = _mm256_loadu_si256((__m256i const*) &weights[inputIndex + 16][outputIndex]);
            __m256i weightsBatch4 = _mm256_loadu_si256((__m256i const*) &weights[inputIndex + 24][outputIndex]);

            //Multiply inputs by weights.
            //Outputs 16-bit results. Neighboring values get summed.
            __m256i tempProduct1 = _mm256_maddubs_epi16 (inputBatch1, weightsBatch1);
            __m256i tempProduct2 = _mm256_maddubs_epi16 (inputBatch2, weightsBatch2);
            __m256i tempProduct3 = _mm256_maddubs_epi16 (inputBatch3, weightsBatch3);
            __m256i tempProduct4 = _mm256_maddubs_epi16 (inputBatch4, weightsBatch4);
            
            //Add temp products to intermediate registers.
            intermediate1 = _mm256_add_epi16(intermediate1, tempProduct1);
            intermediate2 = _mm256_add_epi16(intermediate2, tempProduct2);
            intermediate3 = _mm256_add_epi16(intermediate3, tempProduct3);
            intermediate4 = _mm256_add_epi16(intermediate4, tempProduct4);
        }

        //Add the four registers together. Sum stored in intermediate1.
        intermediate1 = _mm256_add_epi16(intermediate1, intermediate2);
        intermediate3 = _mm256_add_epi16(intermediate3, intermediate4);
        intermediate1 = _mm256_add_epi16(intermediate1, intermediate3);

        /**
         * Simplify the 256-bit register into an 8-bit sum.
         * It currently holds 16 16-bit values.
         * intermediate1 = [s15, s14, s13, s12, s11, s10, s9, s8,| s7, s6, s5, s4, s3, s2, s1, s0]
         * 
         * Instruction 1: Sum the upper and lower halves to create a merged 128 bit register of 8 16-bit values.
         *      sum128 = [(s15 +s7), (s14 + s6), (s13 + s5), (s12 + s4), | (s11 +s3), (s10 + s2), (s9 + s1), (s8 + s0)]
         *      - Concisely described as [s7, s6, s5, s4, | s3, s2, s1, s0] below
         * Instruction 2: Add it to itself, shifted 8 bytes to the right
         *      - Operand 1: [s7, s6, s5, s4, | s3, s2, s1, s0]
         *      - Operand 2: [0, 0, 0, 0, | s7, s6, s5, s4]
         *      - Sum: [s7, s6, s5, s4, | (s3 + s7), (s2 + s6), (s1 + s5), (s0 + s4)]
         * Instruction 3: Add it to itself again, shifted 4 bytes to the right
         *      - Operand 1: [s7, s6, s5, s4, | (s3 + s7), (s2 + s6), (s1 + s5), (s0 + s4)]
         *      - Operand 2: [0, 0, s7, s6, | s5, s4, (s3 + s7), (s2 + s6)]
         *      - Sum: [s7, s6, (s5 + s7), (s4 + s6), | (s3 + s4 + s7), (s2 + s4 + s6), (s1 + s3 + s5 + s7), (s0 + s2 + s4 + s6)]
         * Instruction 4: Add it to itself again, shifted 2 bytes to the right
         *      - Operand 1: [s7, s6, (s5 + s7), (s4 + s6), | (s3 + s4 + s7), (s2 + s4 + s6), (s1 + s3 + s5 + s7), (s0 + s2 + s4 + s6)]
         *      - Operand 2: [0, s7, s6, (s5 + s7), | (s4 + s6), (s3 + s4 + s7), (s2 + s4 + s6), (s1 + s3 + s5 + s7)]
         *      - Sum: [s7, (s6 + s7), (s5 + s6 + s7), (s4 + s5 + s6 + s7), | (s3 + s4 + s4 + s6 s7), (s2 + s3 + s4 + s4 + s6 + s7), (s1 + s2 + s3 + s4 + s5 + s6 + s7), (s0 + s1 + s2 + s3 + s4 + s5 + s6 + s7)]
         * Instruction 5: Extract the rightmost value (total sum)
         */
        __m128i sum128 = _mm_add_epi16(_mm256_castsi256_si128(intermediate1), _mm256_extracti128_si256(intermediate1, 1));
        sum128 = _mm_add_epi16(sum128, _mm_srli_si128(sum128, 8));
        sum128 = _mm_add_epi16(sum128, _mm_srli_si128(sum128, 4));
        sum128 = _mm_add_epi16(sum128, _mm_srli_si128(sum128, 2));
        int16_t totalSum = _mm_extract_epi16(sum128, 0);

        if(biasWeights) totalSum+= biasWeights[outputIndex];

        if(totalSum >= INT8_MAX) totalSum = INT8_MAX;
        else if(totalSum <= INT8_MIN) totalSum = INT8_MIN;
        
        outputValues[outputIndex] = totalSum;

        if(applyCReLU) outputValues[outputIndex] = SCReLU_Int(outputValues[outputIndex], 0, 1);
    }
}

int8_t forwardPropagate_Int(int turn)
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
        calculateLayer_IntBytes(playerNNUE->accumulator[0], tempH2[0], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, playerNNUE->weights2, playerNNUE->weights2_bias, 0);
        calculateLayer_IntBytes(playerNNUE->accumulator[1], tempH2[1], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, &playerNNUE->weights2[ACCUMULATOR_NODES_PER_SIDE], NULL, 0);
    }
    else
    {
        calculateLayer_IntBytes(playerNNUE->accumulator[0], tempH2[0], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, &playerNNUE->weights2[ACCUMULATOR_NODES_PER_SIDE], playerNNUE->weights2_bias, 0);
        calculateLayer_IntBytes(playerNNUE->accumulator[1], tempH2[1], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, playerNNUE->weights2, NULL, 0);
    }
    

    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        //The above step added the biases twice.
        h2[i] = SCReLU_Int(tempH2[0][i] + tempH2[1][i], 0, 1);
    }

    calculateLayer_IntBytes(h2, h3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, playerNNUE->weights3, playerNNUE->weights3_bias, 1);
    calculateLayer_IntBytes(h3, &outputNode, THIRD_HIDDEN_LAYER_NODES, OUTPUT_LAYER_NODES, &playerNNUE->weights4, &playerNNUE->weights4_bias, 0);

    return outputNode;
}