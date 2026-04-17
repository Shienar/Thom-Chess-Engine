#ifndef NEURALNETWORK
#define NEURALNETWORK

#include "../structs.h"
#include "accumulator.h"
#include <math.h>
#include <stdint.h>
#include <immintrin.h>

#define PI 3.141592653589793

//GreedyLR
//https://arxiv.org/abs/2512.14527
extern float learningRate;
#define STARTING_LR 5e-5f
#define MAX_LR 3e-3f
#define MIN_LR 1e-6f
#define LR_FACTOR 0.75f
#define PATIENCE 10
#define WINDOW_SIZE 50
#define LR_THRESHOLD 0.01f
#define WARMUP_PERIOD 10
#define COOLDOWN_PERIOD 10

#define ADAM_BETA1 0.9
#define ADAM_BETA2 0.999
#define ADAM_EPSILON 1e-8
#define ADAM_WEIGHT_DECAY 1e-5

#define POSITIONS_PER_FILE 16384
#define FILE_COUNT 6104

#define FLIP_SQUARE(x) (x^56)
#define FLIP_MASK(x) __builtin_bswap64(x)

//Leaky SCReLU is giving be the best results for training.
#define LEAK_FACTOR 0.01
#define SCReLU(val, min, max) ((val <= min) ? min : ((val * val >= max) ? max : val*val))
#define SCReLU_Leaky(val, min, max) ((val <= min) ? LEAK_FACTOR * val : ((val >= max) ? max + LEAK_FACTOR * (val - max) : val*val))
#define SCReLU_Leaky_Derivative(val, min, max) ((val <= min || val >= max) ? (LEAK_FACTOR) : (2.0*val))
static inline void SIMD_SCReLU(int8_t* val, __m256i v_min, __m256i v_max)
{
    __m256i v_val = _mm256_loadu_si256((const __m256i*) val);

    __m256i v_low = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(v_val));
    __m256i v_high = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(v_val, 1));
    
    v_low = _mm256_mullo_epi16(v_low, v_low);
    v_high = _mm256_mullo_epi16(v_high, v_high);

    v_val = _mm256_packus_epi16(v_low, v_high);
    v_val = _mm256_min_epi8(v_val, v_max);
    v_val = _mm256_max_epi8(v_val, v_min);

    _mm256_storeu_si256((__m256i*) val, v_val);
}
static inline void SIMD_SCReLU_Float(float* val, __m256 v_min, __m256 v_max, __m256 v_grad)
{
    __m256 v_val = _mm256_loadu_ps(val);

    __m256 v_low = _mm256_mul_ps(v_val, v_grad);
    __m256 v_high = _mm256_sub_ps(v_val, v_max);
    v_high = _mm256_fmadd_ps(v_high, v_grad, v_max);

    __m256 v_maskLow = _mm256_cmp_ps(v_val, v_min, _CMP_LE_OQ); //Fill a float's bits in mask with 1 if float <= min
    __m256 v_maskHigh = _mm256_cmp_ps(v_val, v_max, _CMP_GE_OQ); //Fill a float's bits in mask with 1 if float >= max
    
    v_val = _mm256_mul_ps(v_val, v_val);

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

/**
 * Adaptive Moment Estimation 
 *  - Weight Decay
 *  - Sparse adjustments on input layer.
 * https://arxiv.org/abs/1412.6980v8
 */
void train(int saveEveryNBlocks, int maxIterations, float maxAllowedError, accumulator_training* floatAccumulator);

void generateTrainingData(int depth, int maxTime, accumulator_training* floatAccumulator);
#endif