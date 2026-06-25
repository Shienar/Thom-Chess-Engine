#include "analyze/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/engine.h"
#include "gpu/gpu_funcs.h"
#include "omp.h"
#include <float.h>
#include <string.h>

training_weights* raw_weights = NULL;
quantized_weights* int_weights = NULL;

/*** Creating/loading weights ***/
//Box-Muller transform.
void sampleNormalDistribution(float* dest, double standardDeviation) 
{
    do{
        double u1; 
        do { u1 = (double)rand() / (double) RAND_MAX; } while(u1 == 0);
        *dest = standardDeviation * sqrt(-2.0 * log(u1)) * cos(2 * PI *  (double)rand()/(double)RAND_MAX);
    }while(*dest == 0.0f);
}

void loadRawWeights()
{
    if(raw_weights) return;
    raw_weights = calloc(1, sizeof(training_weights));

    FILE* input = fopen(PROJECT_CWD "/import/raw.nnue", "rb");
    if(input)
    {
        size_t size = fread(raw_weights, sizeof(training_weights), 1, input);
        fclose(input);
        if(size == 1) return;
    }
    
    DEBUG_ERROR("Failed to load raw neural network weights from file.");

    //Biases get left at 0.0 from calloc.

    double standardDeviation = sqrt(2.0/30.0);
    
    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            sampleNormalDistribution(&raw_weights->weights1[i][j], standardDeviation);
        }
    }

    standardDeviation = sqrt(2.0 / 512.0);
    for(int i = 0; i < ACCUMULATOR_NODES; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            sampleNormalDistribution(&raw_weights->weights2[j][i], standardDeviation);
        }
    }

    standardDeviation = sqrt(2.0 / 32.0);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            sampleNormalDistribution(&raw_weights->weights3[j][i], standardDeviation);
        }
    }

    for(int b = 0; b < OUTPUT_BUCKETS; b++)
        {
            for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
            {
                sampleNormalDistribution(&raw_weights->weights4[b][i], standardDeviation);
            }
        }
}

void saveRawWeights()
{
    FILE* output = fopen(PROJECT_CWD "/import/raw.nnue", "wb");
    if(output) 
    {
        fwrite(raw_weights, sizeof(training_weights), 1, output);
        fclose(output);
    }
    else DEBUG_ERROR("Failed to write neural network to file.");
}

void quantizeWeights(training_weights* inputFloats, quantized_weights* outputInts)
{
    assert(inputFloats);
    assert(outputInts);

    //weights 1-4
    for(int i = 0; i < HALF_INPUT_BITS; i++)
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            outputInts->weights1[i][j] = (int16_t) clamp(lroundf(inputFloats->weights1[i][j] * QA), INT16_MIN, INT16_MAX);

    for(int i = 0; i < ACCUMULATOR_NODES; i++)
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
            outputInts->weights2[j][i] = (int8_t) clamp(lroundf(inputFloats->weights2[j][i] * QB), INT8_MIN, INT8_MAX);
            
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
            outputInts->weights3[j][i] = (int8_t) clamp(lroundf(inputFloats->weights3[j][i] * QB), INT8_MIN, INT8_MAX);
            
    for(int b = 0; b < OUTPUT_BUCKETS; b++)
        for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
            outputInts->weights4[b][i] = (int8_t) clamp(lroundf(inputFloats->weights4[b][i] * QB), INT8_MIN, INT8_MAX);

    //bias 1-4
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) 
        outputInts->weights1_bias[i] = (int16_t) clamp(lroundf(inputFloats->weights1_bias[i] * QA), INT16_MIN, INT16_MAX);

    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) 
        outputInts->weights2_bias[i] = (int32_t) clamp(lroundf(inputFloats->weights2_bias[i] * QA * QB), INT32_MIN, INT32_MAX);

    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) 
        outputInts->weights3_bias[i] = (int32_t) clamp(lroundf(inputFloats->weights3_bias[i] * QA * QB), INT32_MIN, INT32_MAX);

    for(int i = 0; i < OUTPUT_BUCKETS; i++) 
        outputInts->weights4_bias[i] = (int32_t) clamp(lroundf(inputFloats->weights4_bias[i] * QA * QB), INT32_MIN, INT32_MAX);
}

void loadQuantizedWeights()
{
    if(int_weights) return;
    int_weights = calloc(1, sizeof(quantized_weights));

    FILE* input = fopen(PROJECT_CWD "/import/quantized_1.nnue", "rb");
    if(input)
    {
        size_t size = fread(int_weights, sizeof(quantized_weights), 1, input);
        fclose(input);
        if(size == 1) return;
    }

    DEBUG_ERROR("Failed to load raw neural network weights from file.");
    loadRawWeights();
    quantizeWeights(raw_weights, int_weights);
    saveQuantizedWeights();
}

void saveQuantizedWeights()
{
    FILE* output = fopen(PROJECT_CWD "/import/quantized.nnue", "wb");
    if(output)
    {
        fwrite(int_weights, sizeof(quantized_weights), 1, output);
        fclose(output);
    }
    else DEBUG_ERROR("Failed to write neural network to file.");
}

void print_weight_stats(const char* name, const float* data, size_t size) 
{
    assert(name);
    assert(data);
    assert(size > 0);

    float max_val = -FLT_MAX;
    float min_val = FLT_MAX;
    float abs_max = 0.0f;
    float abs_min = FLT_MAX;
    
    double mean = 0.0;
    double M2 = 0.0;
    double abs_mean = 0.0;
    double abs_M2 = 0.0;

    size_t nanCount = 0;
    size_t zeroCount = 0;
    size_t infinityCount = 0;

    for (size_t i = 0; i < size; i++) 
    {
        //Raw
        float val = data[i];

        if(isnan(val)) nanCount++;
        if(isinf(val)) infinityCount++;
        if(!val) zeroCount++;

        if(val > max_val) max_val = val;
        if(val < min_val) min_val = val;
        
        double delta = val - mean;
        mean += delta / (i + 1);
        double delta2 = val - mean;
        M2 += delta * delta2;

        //Absolute
        float abs_val = fabsf(val);

        if(abs_val > abs_max) abs_max = abs_val;
        if(abs_val < abs_min) abs_min = abs_val;

        double abs_delta = abs_val - abs_mean;
        abs_mean += abs_delta / (i + 1);
        double abs_delta2 = abs_val - abs_mean;
        abs_M2 += abs_delta * abs_delta2;
    }

    double variance = (size > 1) ? M2 / (size - 1) : 0.0;

    double abs_variance = (size > 1) ? abs_M2 / (size - 1) : 0.0;

    printf("===============================\n");
    printf("%s (Count: %" PRIu64 ")\n", name, size);
    printf("-------------------------------\n");
    printf("  Raw Min:        %11.6f | Abs Min:      %11.6f\n", min_val, abs_min);
    printf("  Raw Max:        %11.6f | Abs Max:      %11.6f\n", max_val, abs_max);
    printf("  Raw Mean:       %11.6f | Abs Mean:     %11.6f\n", mean, abs_mean);
    printf("  Raw Variance:   %11.6f | Abs Variance: %11.6f\n", variance, abs_variance);
    if(nanCount) printf("  NaNs: %" PRIu64 "\n", nanCount);
    if(zeroCount) printf("  Zeros: %" PRIu64 "\n", zeroCount);
    if(infinityCount) printf("  Infinities: %" PRIu64 "\n", infinityCount);
}

void print_network_statistics() 
{
    if(!raw_weights) return;

    print_weight_stats("weights1", &raw_weights->weights1[0][0], sizeof(raw_weights->weights1) / sizeof(float));
    print_weight_stats("weights1_bias", raw_weights->weights1_bias, sizeof(raw_weights->weights1_bias) / sizeof(float));
    print_weight_stats("weights2", &raw_weights->weights2[0][0], sizeof(raw_weights->weights2) / sizeof(float));
    print_weight_stats("weights2_bias", raw_weights->weights2_bias,sizeof(raw_weights->weights2_bias) / sizeof(float));
    print_weight_stats("weights3", &raw_weights->weights3[0][0], sizeof(raw_weights->weights3) / sizeof(float));
    print_weight_stats("weights3_bias",raw_weights->weights3_bias, sizeof(raw_weights->weights3_bias) / sizeof(float));
    print_weight_stats("weights4", &raw_weights->weights4[0][0], sizeof(raw_weights->weights4) / sizeof(float));
    print_weight_stats("weights4_bias", raw_weights->weights4_bias, sizeof(raw_weights->weights4_bias) / sizeof(float));
}

/*** Inference ***/

void calculateHiddenLayer(uint8_t* inputValuesA, uint8_t* inputValuesB, uint8_t* outputValues, 
                            int numInputsA, int numInputsB, int numOutputs, 
                            int8_t weights[numOutputs][numInputsA + numInputsB], int32_t* biasWeights)
{
    const __m128i v_min = _mm_setzero_si128();
    const __m128i v_max = _mm_set1_epi32(QA);

    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex+=4)
    {
        __m256i v_output1 = _mm256_setzero_si256();
        __m256i v_output2 = _mm256_setzero_si256();
        __m256i v_output3 = _mm256_setzero_si256();
        __m256i v_output4 = _mm256_setzero_si256();

        for(int inputIndex = 0; inputIndex < numInputsA; inputIndex+=16)
        { 
            // Load 16 bytes of inputs and weights, widen to int16
            __m256i v_input  =  _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)&inputValuesA[inputIndex]));
            __m256i v_weight1 = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[outputIndex + 0][inputIndex]));
            __m256i v_weight2 = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[outputIndex + 1][inputIndex]));
            __m256i v_weight3 = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[outputIndex + 2][inputIndex]));
            __m256i v_weight4 = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[outputIndex + 3][inputIndex]));

            v_output1 = _mm256_add_epi32(v_output1, _mm256_madd_epi16(v_input, v_weight1));
            v_output2 = _mm256_add_epi32(v_output2, _mm256_madd_epi16(v_input, v_weight2));
            v_output3 = _mm256_add_epi32(v_output3, _mm256_madd_epi16(v_input, v_weight3));
            v_output4 = _mm256_add_epi32(v_output4, _mm256_madd_epi16(v_input, v_weight4));
        }

        if(inputValuesB)
        {
            for(int weightIndex = numInputsA, loadIndex = 0; weightIndex < numInputsA + numInputsB; weightIndex +=16, loadIndex+=16)
            {
                __m256i v_input  =  _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)&inputValuesB[loadIndex]));
                __m256i v_weight1 = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[outputIndex + 0][weightIndex]));
                __m256i v_weight2 = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[outputIndex + 1][weightIndex]));
                __m256i v_weight3 = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[outputIndex + 2][weightIndex]));
                __m256i v_weight4 = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[outputIndex + 3][weightIndex]));

                v_output1 = _mm256_add_epi32(v_output1, _mm256_madd_epi16(v_input, v_weight1));
                v_output2 = _mm256_add_epi32(v_output2, _mm256_madd_epi16(v_input, v_weight2));
                v_output3 = _mm256_add_epi32(v_output3, _mm256_madd_epi16(v_input, v_weight3));
                v_output4 = _mm256_add_epi32(v_output4, _mm256_madd_epi16(v_input, v_weight4));
            
            }
        }

        //reduce sums to 4 32-bit ints, including bias
        __m128i s1 = _mm_add_epi32(_mm256_castsi256_si128(v_output1), _mm256_extracti128_si256(v_output1, 1));
        __m128i s2 = _mm_add_epi32(_mm256_castsi256_si128(v_output2), _mm256_extracti128_si256(v_output2, 1));
        __m128i s3 = _mm_add_epi32(_mm256_castsi256_si128(v_output3), _mm256_extracti128_si256(v_output3, 1));
        __m128i s4 = _mm_add_epi32(_mm256_castsi256_si128(v_output4), _mm256_extracti128_si256(v_output4, 1));

        __m128i s12 = _mm_hadd_epi32(s1, s2); // [s1_01, s1_23, s2_01, s2_23]
        __m128i s34 = _mm_hadd_epi32(s3, s4); // [s3_01, s3_23, s4_01, s4_23]

        __m128i final_sums = _mm_hadd_epi32(s12, s34);  //[s1_0123, s2_0123, s3_0123, s4_0123]
        final_sums = _mm_add_epi32(final_sums, _mm_loadu_si128((const __m128i*)&biasWeights[outputIndex]));

        //scale down by QB (64 = 2^6)
        final_sums = _mm_srai_epi32(final_sums, 6);

        //CReLU
        final_sums = _mm_max_epi32(_mm_min_epi32(final_sums, v_max), v_min);

        //Store the 4 ints as uint8_t
        final_sums = _mm_packs_epi32(final_sums, final_sums); 
        final_sums = _mm_packus_epi16(final_sums, final_sums);
        _mm_storeu_si32(&outputValues[outputIndex], final_sums);
    }
}

int calculateOutputLayer(uint8_t* h3, int8_t weights[THIRD_HIDDEN_LAYER_NODES], int32_t bias)
{
    __m256i v_output = _mm256_setzero_si256(); 

    for(int inputIndex = 0; inputIndex < THIRD_HIDDEN_LAYER_NODES; inputIndex+=16)
    {
        __m256i v_input  = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)&h3[inputIndex]));
        __m256i v_weight = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[inputIndex]));
        
        v_output = _mm256_add_epi32(v_output, _mm256_madd_epi16(v_input, v_weight));
    }

    //Reduce from 8 ints to one int.
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(v_output), _mm256_extracti128_si256(v_output, 1));
    
    //_MM_SHUFFLE() reorganizes from default indices (3, 2, 1, 0)
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(2, 3, 0, 1))); //[2 + 3, 3 + 2, 0 + 1, 1 + 0]
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(1, 0, 3, 2))); //[0 + 1 + 2 + 3, 0 + 1 + 2 + 3, 0 + 1 + 2 + 3, 0 + 1 + 2 + 3]
    
    //QB downscaling
    //No activation
    int output = _mm_cvtsi128_si32(sum128) + bias;

    //QB = 2^6.
    //Output Scale = 2^3
    return (output >> 9);
}

int forwardPropagate(bitboard* board, accumulator* acc)
{
    int turn = board->turn;

    int bucket = 0;

    uint8_t* side_us;
    uint8_t* side_them;

    if(ISWHITE(turn))
    {
        side_us = acc->accumulator[WHITE];
        side_them = acc->accumulator[BLACK];
    }
    else
    {
        side_us = acc->accumulator[BLACK];
        side_them = acc->accumulator[WHITE];
    }

    uint8_t h2[SECOND_HIDDEN_LAYER_NODES];
    calculateHiddenLayer(side_us, side_them, h2, ACCUMULATOR_NODES_PER_SIDE, ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, int_weights->weights2, int_weights->weights2_bias);

    uint8_t h3[THIRD_HIDDEN_LAYER_NODES];
    calculateHiddenLayer(h2, NULL, h3, SECOND_HIDDEN_LAYER_NODES, 0, THIRD_HIDDEN_LAYER_NODES, int_weights->weights3, int_weights->weights3_bias);

    return calculateOutputLayer(h3, int_weights->weights4[bucket], int_weights->weights4_bias[bucket]);
}