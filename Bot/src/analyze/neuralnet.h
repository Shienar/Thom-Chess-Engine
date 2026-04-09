#ifndef NEURALNETWORK
#define NEURALNETWORK

#include "../structs.h"
#include "accumulator.h"
#include <math.h>
#include <stdint.h>
#include <immintrin.h>

#define ADAM_LEARNING_RATE 1e-4
#define ADAM_BETA1 0.9
#define ADAM_BETA2 0.999
#define ADAM_EPSILON 1e-8
#define ADAM_WEIGHT_DECAY 0.02

#define POSITIONS_PER_FILE 16384
#define FILE_COUNT 6104

#define FLIP_SQUARE(x) (x^56)
#define FLIP_MASK(x) __builtin_bswap64(x)

//Leaky SCReLU seems to give better results for training.
#define SCReLU(val, min, max) ((val <= min) ? min : ((val * val >= max) ? max : val*val))
#define SCReLU_Leaky(val, min, max) ((val <= min) ? 0.01 * val : ((val >= max) ? max + 0.01 * (val - max) : val*val))
#define SCReLU_Leaky_Derivative(val, min, max) ((val <= min || val >= max) ? (0.01) : (2.0*val))
static inline void SIMD_SCReLU(int8_t* val, __m256i v_min, __m256i v_max)
{
    __m256i v_val = _mm256_loadu_si256((const __m256i*) val);

    __m256i v_low = _mm256_unpacklo_epi8(v_val, _mm256_setzero_si256());
    __m256i v_high = _mm256_unpackhi_epi8(v_val, _mm256_setzero_si256());
    
    v_low = _mm256_mullo_epi16(v_low, v_low);
    v_high = _mm256_mullo_epi16(v_high, v_high);

    v_val = _mm256_packus_epi16(v_low, v_high);
    v_val = _mm256_max_epi8(v_val, v_max);
    v_val = _mm256_min_epi8(v_val, v_min);

    _mm256_storeu_si256((__m256i*) val, v_val);
}
static inline void SIMD_SCReLU_Float(float* val, __m256 v_min, __m256 v_max, __m256 v_grad)
{
    __m256 v_val = _mm256_loadu_ps(val);

    __m256 v_low = _mm256_mul_ps(v_val, v_grad);
    v_val = _mm256_mul_ps(v_val, v_val);
    __m256 v_high = _mm256_sub_ps(v_val, v_max);
    v_high = _mm256_fmadd_ps(v_high, v_grad, v_max);

    __m256 v_maskLow = _mm256_cmp_ps(v_val, v_min, _CMP_LE_OQ); //Fill a float's bits in mask with 1 if float <= min
    __m256 v_maskHigh = _mm256_cmp_ps(v_val, v_max, _CMP_GE_OQ); //Fill a float's bits in mask with 1 if float >= max

    //If the mask is 1, copy v_low/v_high in. Else, copy in v_val
    v_val = _mm256_blendv_ps(v_val, v_low, v_maskLow); 
    v_val = _mm256_blendv_ps(v_val, v_high, v_maskHigh);

    _mm256_storeu_ps(val, v_val);
}


extern network_weights_training* trainingNNUE;
extern network_weights_playing* playerNNUE;

/* Binary  file storage. */
void load_trainingWeights();
void save_trainingWeights();
 
void load_playingWeights();
void save_playingWeights();

void quantizeWeights(network_weights_training* inputFloats, network_weights_playing* outputBytes);

/**
 * Uses SIMD to calculate and populate outputValues.
 */
void calculateLayer_Floats(float* inputValues, float* outputValues, int numInputs, int numOutputs, float weights[numOutputs][numInputs], int inputOffset, float* biasWeights,  int applyCReLU);
void calculateLayer_IntBytes(int8_t* inputValues, int8_t* outputValues, int numInputs, int numOutputs, int8_t weights[numOutputs][numInputs], int inputOffset, int8_t* biasWeights,  int applyCReLU);

float forwardPropagate_Float(int turn, accumulator_training* floatAccumulator, int centerAndScaleAtZero);
int8_t forwardPropagate_Int(int turn, accumulator_playing* byteAccumulator, int centerAtZero);

// Adaptive Movement Estimation (ADAM)
// https://arxiv.org/abs/1412.6980v8
void train(int saveEveryNBlocks, int maxIterations, float maxAllowedError, accumulator_training* floatAccumulator);

void generateTrainingData(int depth, int maxTime, accumulator_training* floatAccumulator);
#endif