#include "analyze/nnue/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/search.h"
#include <float.h>
#include <string.h>

training_weights* raw_weights = NULL;
#ifdef RELEASE
quantized_weights* int_weights = (quantized_weights*) int_weights_bin;
#else
quantized_weights* int_weights = NULL;
#endif


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

    FILE* input = fopen(RAW_PATH, "rb");
    if(input)
    {
        size_t size = fread(raw_weights, sizeof(training_weights), 1, input);
        fclose(input);
        if(size == 1) return;
    }
    
    DEBUG_ERROR("Failed to load raw neural network weights from file.");

    //Biases get left at 0.0 from calloc.

    double standardDeviation = sqrt(2.0/32.0);
    
    for(int i = 0; i < HALF_INPUT_BITS; i++)
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            sampleNormalDistribution(&raw_weights->weights1[i][j], standardDeviation);

    standardDeviation = sqrt(2.0 / ACCUMULATOR_NODES);
    for(int b = 0; b < OUTPUT_BUCKETS; b++)
        for(int i = 0; i < ACCUMULATOR_NODES; i++)
            sampleNormalDistribution(&raw_weights->weights2[b][i], standardDeviation);

    saveRawWeights();
}

void saveRawWeights()
{
    FILE* output = fopen(RAW_PATH, "wb");
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

    //weights
    for(int i = 0; i < HALF_INPUT_BITS; i++)
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            outputInts->weights1[i][j] = (int16_t) clamp(lroundf(inputFloats->weights1[i][j] * QA), INT16_MIN, INT16_MAX);
    
    for(int b = 0; b < OUTPUT_BUCKETS; b++)
        for(int i = 0; i < ACCUMULATOR_NODES; i++)
                outputInts->weights2[b][i] = (int8_t) clamp(lroundf(inputFloats->weights2[b][i] * QB), INT8_MIN, INT8_MAX);
            
    //bias 1-4
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) 
        outputInts->weights1_bias[i] = (int16_t) clamp(lroundf(inputFloats->weights1_bias[i] * QA), INT16_MIN, INT16_MAX);

    for(int b = 0; b < OUTPUT_BUCKETS; b++)
        outputInts->weights2_bias[b] = (int32_t) clamp(lroundf(inputFloats->weights2_bias[b] * QA * QB), INT32_MIN, INT32_MAX);
}

void loadQuantizedWeights()
{
    if(int_weights) return;
    int_weights = calloc(1, sizeof(quantized_weights));

    FILE* input = fopen(QUANTIZED_PATH, "rb");
    if(input)
    {
        size_t size = fread(int_weights, sizeof(quantized_weights), 1, input);
        fclose(input);
        if(size == 1) return;
    }

    DEBUG_ERROR("Failed to load quantized neural network weights from file.");
    #ifdef TRAIN
    loadRawWeights();
    quantizeWeights(raw_weights, int_weights);
    saveQuantizedWeights();
    #else
    printf("Failed to load quantized neural network weights from file.\n");
    exit(1);
    #endif
}

void saveQuantizedWeights()
{
    FILE* output = fopen(QUANTIZED_PATH, "wb");
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

    for(size_t i = 0; i < size; i++) 
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
    print_weight_stats("weights1", &raw_weights->weights1[0][0], sizeof(raw_weights->weights1) / sizeof(float));
    print_weight_stats("weights1_bias", raw_weights->weights1_bias, sizeof(raw_weights->weights1_bias) / sizeof(float));
    print_weight_stats("weights2", &raw_weights->weights2[0][0], sizeof(raw_weights->weights2) / sizeof(float));
    print_weight_stats("weights2_bias", &raw_weights->weights2_bias[0],sizeof(raw_weights->weights2_bias) / sizeof(float));
}

/*** Inference ***/

int calculateOutputLayer(uint8_t* inputValuesA, uint8_t* inputValuesB, int8_t weights[ACCUMULATOR_NODES], int32_t bias)
{
    __m256i v_output = _mm256_setzero_si256();

    for(int inputIndex = 0; inputIndex < ACCUMULATOR_NODES_PER_SIDE; inputIndex+=16)
    {
        __m256i v_input  = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)&inputValuesA[inputIndex]));
        __m256i v_weight = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[inputIndex]));
        
        v_output = _mm256_add_epi32(v_output, _mm256_madd_epi16(v_input, v_weight));
    }
    
    for(int inputIndex = 0; inputIndex < ACCUMULATOR_NODES_PER_SIDE; inputIndex+=16)
    {
        __m256i v_input  = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)&inputValuesB[inputIndex]));
        __m256i v_weight = _mm256_cvtepi8_epi16(_mm_loadu_si128((const __m128i*)&weights[ACCUMULATOR_NODES_PER_SIDE + inputIndex]));
        
        v_output = _mm256_add_epi32(v_output, _mm256_madd_epi16(v_input, v_weight));
    }

    //Reduce from 8 ints to one int.
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(v_output), _mm256_extracti128_si256(v_output, 1));
    
    //_MM_SHUFFLE() reorganizes from default indices (3, 2, 1, 0)
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(2, 3, 0, 1))); //[2 + 3, 3 + 2, 0 + 1, 1 + 0]
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_SHUFFLE(1, 0, 3, 2))); //[0 + 1 + 2 + 3, 0 + 1 + 2 + 3, 0 + 1 + 2 + 3, 0 + 1 + 2 + 3]
    
    //QB downscaling
    //No activation
    //Output Scaling - Why this this necessary? If I train my network with an output scaled to (QA / 16) and downshift here,
    //my engine will be significantly stronger than if I don't. More testing/information is needed, maybe my engine's pruning
    //values work better with this version since it has a smaller range. A lack of training output scaling will fail completely,
    //but why does (QA / 16) work so much better than QA?
    int output = _mm_cvtsi128_si32(sum128) + bias;
    return (output >> (QB_RSHIFT + OUTPUT_SCALE_RSHIFT));
}

int forwardPropagate(bitboard* board, accumulator* acc)
{
    int bucket = (__builtin_popcountll(board->pieces_all) - 1) / 4;

    int output = calculateOutputLayer(acc->accumulator[board->turn], 
                                      acc->accumulator[FLIP_COLOR(board->turn)], 
                                      int_weights->weights2[bucket], 
                                      int_weights->weights2_bias[bucket]);

    return clamp(output, -(MIN_MATE_SCORE - 1), MIN_MATE_SCORE - 1);
}