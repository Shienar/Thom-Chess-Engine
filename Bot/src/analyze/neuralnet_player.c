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

static inline int horizontalSIMDSum(__m256i vector)
{
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(vector), _mm256_extracti128_si256(vector, 1));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_BADC)); // A = A + B; C = C + D;
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_CDAB)); // A = A + C = (original) A + B + C + D
    return _mm_cvtsi128_si32(sum128);
}

//__m256i stores 32 8-bit ints (epi8 = extended packed 8-bit integer (signed))
void calculateLayer_IntBytes(int8_t* inputValues, int8_t* outputValues, int numInputs, int numOutputs, int8_t weights[numOutputs][numInputs], int inputOffset, int8_t* biasWeights,  int applyCReLU)
{
    int totalSum1, totalSum2, totalSum3, totalSum4, totalSum5, totalSum6, totalSum7, totalSum8;
    __m256i v_one = _mm256_set1_epi16(1);
    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex+=8)
    {
        __m256i v_output1 = _mm256_setzero_si256(); //si256 = 256-bit signed integer.
        __m256i v_output2 = _mm256_setzero_si256();
        __m256i v_output3 = _mm256_setzero_si256();
        __m256i v_output4 = _mm256_setzero_si256();
        __m256i v_output5 = _mm256_setzero_si256();
        __m256i v_output6 = _mm256_setzero_si256();
        __m256i v_output7 = _mm256_setzero_si256();
        __m256i v_output8 = _mm256_setzero_si256();


        //All layer lengths are divisible by 32 so no overflow.
        for(int inputIndex = 0; inputIndex < numInputs; inputIndex+=32)
        {
            __m256i v_inputBatch = _mm256_loadu_si256((__m256i const*) &inputValues[inputIndex]);

            //Weights are an array of float w[OUTPUT NODES][INPUT NODES]
            __m256i v_weightsBatch1 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 0][inputIndex + inputOffset]);
            __m256i v_weightsBatch2 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 1][inputIndex + inputOffset]);
            __m256i v_weightsBatch3 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 2][inputIndex + inputOffset]);
            __m256i v_weightsBatch4 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 3][inputIndex + inputOffset]);
            __m256i v_weightsBatch5 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 4][inputIndex + inputOffset]);
            __m256i v_weightsBatch6 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 5][inputIndex + inputOffset]);
            __m256i v_weightsBatch7 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 6][inputIndex + inputOffset]);
            __m256i v_weightsBatch8 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 7][inputIndex + inputOffset]);

            //Multiply inputs by weights.
                //Outputs 16-bit results. Neighboring values get summed.
                //Extend to 32-bit results, sum neighboring values.
            __m256i v_tempProduct1 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch1), v_one);
            __m256i v_tempProduct2 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch2), v_one);
            __m256i v_tempProduct3 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch3), v_one);
            __m256i v_tempProduct4 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch4), v_one);
            __m256i v_tempProduct5 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch5), v_one);
            __m256i v_tempProduct6 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch6), v_one);
            __m256i v_tempProduct7 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch7), v_one);
            __m256i v_tempProduct8 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch8), v_one);
            
            //Add temp products to intermediate registers.
            v_output1 = _mm256_add_epi32(v_output1, v_tempProduct1);
            v_output2 = _mm256_add_epi32(v_output2, v_tempProduct2);
            v_output3 = _mm256_add_epi32(v_output3, v_tempProduct3);
            v_output4 = _mm256_add_epi32(v_output4, v_tempProduct4);
            v_output5 = _mm256_add_epi32(v_output5, v_tempProduct5);
            v_output6 = _mm256_add_epi32(v_output6, v_tempProduct6);
            v_output7 = _mm256_add_epi32(v_output7, v_tempProduct7);
            v_output8 = _mm256_add_epi32(v_output8, v_tempProduct8);
        }

        totalSum1 = horizontalSIMDSum(v_output1);
        totalSum2 = horizontalSIMDSum(v_output2);
        totalSum3 = horizontalSIMDSum(v_output3);
        totalSum4 = horizontalSIMDSum(v_output4);
        totalSum5 = horizontalSIMDSum(v_output5);
        totalSum6 = horizontalSIMDSum(v_output6);
        totalSum7 = horizontalSIMDSum(v_output7);
        totalSum8 = horizontalSIMDSum(v_output8);
        if(biasWeights)
        {
            totalSum1+= biasWeights[outputIndex + 0];
            totalSum2+= biasWeights[outputIndex + 1];
            totalSum3+= biasWeights[outputIndex + 2];
            totalSum4+= biasWeights[outputIndex + 3];
            totalSum5+= biasWeights[outputIndex + 4];
            totalSum6+= biasWeights[outputIndex + 5];
            totalSum7+= biasWeights[outputIndex + 6];
            totalSum8+= biasWeights[outputIndex + 7];
        }
        
        //Clamp to int8 max/min values and store.
        __m128i sums = _mm_setr_epi32(totalSum1, totalSum2, totalSum3, totalSum4);
        __m128i other_sums = _mm_setr_epi32(totalSum5, totalSum6, totalSum7, totalSum8);
        sums = _mm_packs_epi32(sums, other_sums); 
        sums = _mm_packs_epi16(sums, sums);
        _mm_storel_epi64((__m128i*)&outputValues[outputIndex], sums);
    }

    if(applyCReLU)
    {
        __m256i v_min = _mm256_setzero_si256();
        __m256i v_max = _mm256_set1_epi8(127);
        for(int i = 0; i < numOutputs; i+=32)
        {
            SIMD_SCReLU(&outputValues[i], v_min, v_max);
        }
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
    

    __m256i v_min = _mm256_setzero_si256();
    __m256i v_max = _mm256_set1_epi8(127);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i+=32)
    {
        //h2[i] = SCReLU((tempH2[0][i] + tempH2[1][i]), 0, 127);
        _mm256_storeu_si256((__m256i*)&h2[i], _mm256_add_epi8(_mm256_loadu_si256((__m256i const*) &tempH2[0][i]), _mm256_loadu_si256((__m256i const*) &tempH2[1][i])));
        SIMD_SCReLU(&h2[i], v_min, v_max);
    }

    calculateLayer_IntBytes(h2, h3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, playerNNUE->weights3, 0, playerNNUE->weights3_bias, 1);
    calculateLayer_IntBytes(h3, &outputNode, THIRD_HIDDEN_LAYER_NODES, OUTPUT_LAYER_NODES, &playerNNUE->weights4, 0, &playerNNUE->weights4_bias, 0);

    //Change from [0, 127] to [-64, 63]
    // (It's trained to generate a value in the range but could theoretically generate one outside of it)
    if(centerAtZero) outputNode-=64;

    return outputNode;
}