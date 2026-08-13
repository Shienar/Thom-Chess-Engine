#include "analyze/nnue/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/search.h"
#include <float.h>
#include <string.h>

int isNetworkLoaded = 1;
int useNNUE = 1;
nnue_weights* weights = (nnue_weights*)weights_bin_start;

void initNNUE()
{
    isNetworkLoaded = (weights_bin_end - weights_bin_start) > 0;
    if(isNetworkLoaded)
    {
        for(int i = 0; i < HALF_INPUT_BITS; i++)
            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
                weights->weights1[i][j] = littleEndian16(weights->weights1[i][j]);
                
        for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
            weights->weights1_bias[i] = littleEndian16(weights->weights1_bias[i]);
        
            
        weights->weights2_bias = littleEndian32(weights->weights2_bias);
    }
    else
        useNNUE = 0;
}

//Lizard SCReLU.
//https://chessprogramming.org/NNUE#lizard-screlu
int calculateOutputLayer(int16_t* inputValuesA, int16_t* inputValuesB, int16_t weights[ACCUMULATOR_NODES], int16_t bias)
{
    const __m256i v_zero = _mm256_setzero_si256();
    const __m256i v_max = _mm256_set1_epi16(QA);

    __m256i v_output = _mm256_setzero_si256();

    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=16)
    {
        __m256i v_us = _mm256_loadu_si256((const __m256i*)&inputValuesA[i]);
        __m256i v_them = _mm256_loadu_si256((const __m256i*)&inputValuesB[i]);

        __m256i v_weights_us = _mm256_loadu_si256((const __m256i*)&weights[i]);
        __m256i v_weights_them = _mm256_loadu_si256((const __m256i*)&weights[ACCUMULATOR_NODES_PER_SIDE + i]);
        
        //Clamp
        v_us = _mm256_min_epi16( _mm256_max_epi16(v_us, v_zero), v_max);
        v_them = _mm256_min_epi16( _mm256_max_epi16(v_them, v_zero), v_max);


        //SCReLU + Forward pass weight multiplication.
        __m256i v_us_output = _mm256_madd_epi16(_mm256_mullo_epi16(v_weights_us, v_us ), v_us);
        __m256i v_them_output = _mm256_madd_epi16(_mm256_mullo_epi16(v_weights_them, v_them), v_them);

        v_output = _mm256_add_epi32(v_output, v_us_output);
        v_output = _mm256_add_epi32(v_output, v_them_output);
    }

    //Horizontal reduction:
    __m128i v_sum128 = _mm_add_epi32(_mm256_castsi256_si128(v_output), _mm256_extracti128_si256(v_output, 1));
    v_sum128 = _mm_add_epi32(v_sum128, _mm_shuffle_epi32(v_sum128, _MM_SHUFFLE(1, 0, 3, 2)));
    v_sum128 = _mm_add_epi32(v_sum128, _mm_shuffle_epi32(v_sum128, _MM_SHUFFLE(2, 3, 0, 1)));

    //QA * QA * QB
    int output = _mm_cvtsi128_si32(v_sum128);

    //QA * QB
    output >>= QA_RSHIFT;
    output += bias;

    //EVAL_SCALE * QA * QB
    output *= EVAL_SCALE;

    //EVAL_SCALE
    return output >> (QA_RSHIFT + QB_RSHIFT);
}

int forwardPropagate(bitboard* board, accumulator* acc)
{
    int output = calculateOutputLayer(acc->rawAccumulator[board->turn], 
                                      acc->rawAccumulator[FLIP_COLOR(board->turn)], 
                                      weights->weights2, 
                                      weights->weights2_bias);

    return clamp(output, -(MIN_MATE_SCORE - 1), MIN_MATE_SCORE - 1);
}