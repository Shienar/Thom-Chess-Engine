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
    if(!playerNNUE) playerNNUE = calloc(1, sizeof(network_weights_playing));

    FILE* input = fopen("import/NNUE_Player.bin", "rb");
    if(input)
    {
        fread(playerNNUE, sizeof(network_weights_playing), 1, input);
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

            free(trainingNNUE);
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
void calculateHiddenLayer(uint8_t* inputValues, uint8_t* outputValues, int numInputs, int numOutputs, int8_t weights[numOutputs][numInputs], int32_t* biasWeights)
{
    int totalSum1, totalSum2, totalSum3, totalSum4;
    __m256i v_one = _mm256_set1_epi16(1);
    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex+=4)
    {
        __m256i v_output1 = _mm256_setzero_si256(); //si256 = 256-bit signed integer.
        __m256i v_output2 = _mm256_setzero_si256();
        __m256i v_output3 = _mm256_setzero_si256();
        __m256i v_output4 = _mm256_setzero_si256();


        //All layer lengths are divisible by 32 so no overflow.
        for(int inputIndex = 0; inputIndex < numInputs; inputIndex+=32)
        {
            __m256i v_inputBatch = _mm256_loadu_si256((__m256i const*) &inputValues[inputIndex]);

            //Weights are an array of float w[OUTPUT NODES][INPUT NODES]
            __m256i v_weightsBatch1 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 0][inputIndex]);
            __m256i v_weightsBatch2 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 1][inputIndex]);
            __m256i v_weightsBatch3 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 2][inputIndex]);
            __m256i v_weightsBatch4 = _mm256_loadu_si256((__m256i const*) &weights[outputIndex + 3][inputIndex]);

            //Multiply inputs by weights.
            //Outputs 16-bit results. Neighboring values get summed.
            //Extend to 32-bit results, sum neighboring values.
            __m256i v_tempProduct1 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch1), v_one);
            __m256i v_tempProduct2 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch2), v_one);
            __m256i v_tempProduct3 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch3), v_one);
            __m256i v_tempProduct4 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch4), v_one);
            
            //Add temp products to intermediate registers.
            v_output1 = _mm256_add_epi32(v_output1, v_tempProduct1);
            v_output2 = _mm256_add_epi32(v_output2, v_tempProduct2);
            v_output3 = _mm256_add_epi32(v_output3, v_tempProduct3);
            v_output4 = _mm256_add_epi32(v_output4, v_tempProduct4);
        }

        totalSum1 = horizontalSIMDSum(v_output1) + biasWeights[outputIndex + 0];
        totalSum2 = horizontalSIMDSum(v_output2) + biasWeights[outputIndex + 1];
        totalSum3 = horizontalSIMDSum(v_output3) + biasWeights[outputIndex + 2];
        totalSum4 = horizontalSIMDSum(v_output4) + biasWeights[outputIndex + 3];

        //QB = 64 == 2 ^ 6
        totalSum1 >>= 6;
        totalSum2 >>= 6;
        totalSum3 >>= 6;
        totalSum4 >>= 6;
        
        outputValues[outputIndex + 0] = max(min(totalSum1, 127), 0);
        outputValues[outputIndex + 1] = max(min(totalSum2, 127), 0);
        outputValues[outputIndex + 2] = max(min(totalSum3, 127), 0);
        outputValues[outputIndex + 3] = max(min(totalSum4, 127), 0);
    }
}

int calculateOutputLayer(uint8_t* inputValues, int8_t weights[THIRD_HIDDEN_LAYER_NODES], int32_t bias)
{
    int sum = 0;
    __m256i v_one = _mm256_set1_epi16(1);

    __m256i v_output = _mm256_setzero_si256(); //si256 = 256-bit signed integer.

    //All layer lengths are divisible by 32 so no overflow.
    for(int inputIndex = 0; inputIndex < THIRD_HIDDEN_LAYER_NODES; inputIndex+=32)
    {
        __m256i v_inputBatch = _mm256_loadu_si256((__m256i const*) &inputValues[inputIndex]);

        //Weights are an array of float w[INPUT NODES]
        __m256i v_weightsBatch = _mm256_loadu_si256((__m256i const*) &weights[inputIndex]);

        //Multiply inputs by weights.
        //Outputs 16-bit results. Neighboring values get summed.
        //Extend to 32-bit results, sum neighboring values.
        __m256i v_tempProduct1 = _mm256_madd_epi16(_mm256_maddubs_epi16(v_inputBatch, v_weightsBatch), v_one);
        
        //Add temp products to intermediate registers.
        v_output = _mm256_add_epi32(v_output, v_tempProduct1);
    }

    sum = horizontalSIMDSum(v_output) + bias;

    //QB = 64 == 2 ^ 6
    sum >>= 6;
    
    return sum;
}

int32_t forwardPropagate(int turn, accumulator* acc)
{
    uint8_t tempAccumulator[ACCUMULATOR_NODES];
    if(ISWHITE(turn))
    {
        memcpy(&tempAccumulator[0], &acc->accumulator[WHITE][0], sizeof(uint8_t) * ACCUMULATOR_NODES_PER_SIDE);
        memcpy(&tempAccumulator[1], &acc->accumulator[BLACK][0], sizeof(uint8_t) * ACCUMULATOR_NODES_PER_SIDE);
    }
    else
    {
        memcpy(&tempAccumulator[0], &acc->accumulator[BLACK][0], sizeof(uint8_t) * ACCUMULATOR_NODES_PER_SIDE);
        memcpy(&tempAccumulator[1], &acc->accumulator[WHITE][0], sizeof(uint8_t) * ACCUMULATOR_NODES_PER_SIDE);
    }

    uint8_t h2[SECOND_HIDDEN_LAYER_NODES];
    calculateHiddenLayer(tempAccumulator, h2, ACCUMULATOR_NODES, SECOND_HIDDEN_LAYER_NODES, playerNNUE->weights2, playerNNUE->weights2_bias);

    uint8_t h3[THIRD_HIDDEN_LAYER_NODES];
    calculateHiddenLayer(h2, h3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, playerNNUE->weights3, playerNNUE->weights3_bias);

    return calculateOutputLayer(h3, playerNNUE->weights4, playerNNUE->weights4_bias);
}