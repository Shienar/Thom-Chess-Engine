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
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            outputInts->weights1[i][j] = (int16_t) max(min(INT16_MAX, lroundf(inputFloats->weights1[i][j] * QA)), INT16_MIN);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            outputInts->weights2[j][i] = (int8_t) max(min(INT8_MAX, lroundf(inputFloats->weights2[j][i] * QB)), INT8_MIN);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            outputInts->weights3[j][i] = (int8_t) max(min(INT8_MAX, lroundf(inputFloats->weights3[j][i] * QB)), INT8_MIN);
        }
    }
    for(int b = 0; b < OUTPUT_BUCKETS; b++)
    {
        for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
        {
            outputInts->weights4[b][i] = (int8_t) max(min(INT8_MAX, lroundf(inputFloats->weights4[b][i] * QB)), INT8_MIN);
        }
    }

    //bias 1-4
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) outputInts->weights1_bias[i] = (int16_t) max(min(INT16_MAX, lroundf(inputFloats->weights1_bias[i] * QA)), INT16_MIN);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) outputInts->weights2_bias[i] = (int32_t) max(min(INT32_MAX, lroundf(inputFloats->weights2_bias[i] * QA * QB)), INT32_MIN);
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) outputInts->weights3_bias[i] = (int32_t) max(min(INT32_MAX, lroundf(inputFloats->weights3_bias[i] * QA * QB)), INT32_MIN);
    for(int i = 0; i < OUTPUT_BUCKETS; i++) outputInts->weights4_bias[i] = (int32_t) max(min(INT32_MAX, lroundf(inputFloats->weights4_bias[i] * QA * QB)), INT32_MIN);
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
    //Output Scale = 2^4
    return (output >> 10);
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

/*** Training Weights ***/

void loadTrainingData(CompactPosition data, bitboard* board, float* expectedOutput)
{

    // bit 0 = turn
    // bit 1 = win for side to move
    // bit 2 = loss for side to move
    // bit 3 = draw
    board->turn = data.flags & 1;

    float result = 0.5;
    if(data.flags & 2) result = 1.0;
    else if(data.flags & 4) result = 0.0; 
    *expectedOutput = LAMBDA * (SIGMOID(data.evaluation / EVAL_SCALE)) + (1.0f - LAMBDA) * result;

    //Clear board (HalfKP only cares about piece positions)
    memset(board->pieceArr, EMPTY_PIECE, 64 * sizeof(uint8_t));
    memset(board->pieces, 0, 12 * sizeof(uint64_t));
    memset(board->pieces_side, 0, 2 * sizeof(uint64_t));
    board->pieces_all = 0;

    uint64_t mask = data.occupancy;
    int offset = 0;
    while(mask)
    {
        int sq = __builtin_ctzll(mask);
        int byteIndex = offset / 2;
        int piece = data.pieces[byteIndex];
        if(offset % 2) piece = piece&0xF;
        else piece >>= 4;

        uint64_t sqMask = (1ull << sq);
        board->pieces_all |= sqMask;
        board->pieces_side[COLOR(piece)] |= sqMask;
        board->pieces[piece] |= sqMask;

        if(ISKING(piece))
        {
            if(ISBLACK(piece)) board->kingSquare_b = sq;
            else board->kingSquare_w = sq;
        }

        offset++;
        mask &= (mask - 1);
    }
}

void shuffle_long(uint64_t* arr, int count)
{
    for(int i = count-1; i > 0; i--)
    {
        int j = rand()%i;
        uint64_t temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}


static inline float sumLoss(float* loss)
{
    __m256 tempSum1 = _mm256_setzero_ps();
    __m256 tempSum2 = _mm256_setzero_ps();
    for(int i = 0; i < MINIBATCH_SIZE; i+=16)
    {
        tempSum1 = _mm256_add_ps(tempSum1, _mm256_loadu_ps(&loss[i]));
        tempSum2 = _mm256_add_ps(tempSum2, _mm256_loadu_ps(&loss[i + 8]));
    }
    tempSum1 = _mm256_add_ps(tempSum1, tempSum2);

    __m128 sum128 = _mm_add_ps(_mm256_castps256_ps128((__m256)tempSum1), _mm256_extractf128_ps(tempSum1, 1));
    
    sum128 = _mm_add_ps(sum128, _mm_movehl_ps(sum128, sum128)); 
    sum128 = _mm_add_ss(sum128, _mm_shuffle_ps(sum128, sum128, _MM_SHUFFLE(1, 1, 1, 1))); 
    return _mm_cvtss_f32(sum128);
}

void train(int maxIterations, float maxAllowedError)
{
    FILE* trainingDataFile = fopen(PROJECT_CWD "/import/trainingData.bin", "rb");
    if(!trainingDataFile)
    {
        DEBUG_ERROR("Failed to open training data file.");
        return;
    }
    fseek_64(trainingDataFile, 0, SEEK_END);
    uint64_t file_size = ftell_64(trainingDataFile);
    rewind(trainingDataFile);

    uint64_t positionCount = file_size / sizeof(CompactPosition);

    //Used to skip to random position in the .bin file.
    //The division in blockcount effectively truncates extraneous positions that don't fit within a full MINIBATCH_SIZE * MINIBATCHES_PER_SHUFFLE_BLOCK block.
    int blockCount = positionCount /  (MINIBATCH_SIZE * FEN_SKIP);
    uint64_t* blockIndices = calloc(blockCount,  sizeof(uint64_t));
    for (int i = 0; i < blockCount; i++) 
    {
        blockIndices[i] = (MINIBATCH_SIZE * FEN_SKIP) * i * sizeof(CompactPosition);
    }
    
    short* activeInputs_A = NULL;
    float* expectedOutputs_A = NULL;

    short* activeInputs_B = NULL;
    float* expectedOutputs_B = NULL;
    float* loss_buffer = NULL;
    
    int cl_errorcode = initHIP(raw_weights, &activeInputs_A, &expectedOutputs_A, &activeInputs_B, &expectedOutputs_B, &loss_buffer);
    if(cl_errorcode != hipSuccess) 
    {
        DEBUG_ERROR("Failed to init kernels - Error Code: %d\n%s", cl_errorcode, hipGetErrorString(cl_errorcode));
        fclose(trainingDataFile);
        free(blockIndices);
        return;
    }

    int totalIterations = 0;

    float totalLoss = 0.0; //accumulated value per iteration

    clock_t startTime, endTime;
    double duration_sec;

    //Read data for next few minibatches, then shuffle data amongst them.
    CompactPosition* batchData = calloc(MINIBATCH_SIZE, sizeof(CompactPosition));
    CompactPosition* unsortedData = calloc(MINIBATCH_SIZE * FEN_SKIP, sizeof(CompactPosition));

    FILE* validationData = fopen(PROJECT_CWD  "/import/validationData.bin", "rb");

    fseek_64(validationData, 0, SEEK_END);
    int validationEntries = ftell_64(validationData) / sizeof(CompactPosition);
    validationEntries -= (validationEntries % MINIBATCH_SIZE);
    int validationBlocks = validationEntries / MINIBATCH_SIZE;
    float validationLoss = 0.0;
    rewind(validationData);

    int inputGroup = INPUT_GROUP_A;

    startTime = clock();
    for(int i = 0; i < validationBlocks; i++)
    {
        size_t size = fread(batchData, sizeof(CompactPosition), MINIBATCH_SIZE, validationData);
        if(size < MINIBATCH_SIZE) { DEBUG_ERROR("Failed reading validation Data"); continue; }
        inputGroup ^= 1;

        #pragma omp parallel
        {
            bitboard* board = create_board(NULL);
            
            #pragma omp for schedule(static)
            for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
            {
                if(inputGroup == INPUT_GROUP_A)  loadTrainingData(batchData[entryNumber], board, &expectedOutputs_A[entryNumber]);
                else loadTrainingData(batchData[entryNumber], board, &expectedOutputs_B[entryNumber]);

                uint64_t inputs[2 * TRACKED_PIECES] = {0};

                inputs[0] = board->pieces[WHITE_PAWN];
                inputs[1] = board->pieces[WHITE_KNIGHT];
                inputs[2] = board->pieces[WHITE_BISHOP];
                inputs[3] = board->pieces[WHITE_ROOK];
                inputs[4] = board->pieces[WHITE_QUEEN];
                inputs[5] = board->pieces[BLACK_PAWN];
                inputs[6] = board->pieces[BLACK_KNIGHT];
                inputs[7] = board->pieces[BLACK_BISHOP];
                inputs[8] = board->pieces[BLACK_ROOK];
                inputs[9] = board->pieces[BLACK_QUEEN];

                inputs[10] = FLIP_MASK(board->pieces[BLACK_PAWN]);
                inputs[11] = FLIP_MASK(board->pieces[BLACK_KNIGHT]);
                inputs[12] = FLIP_MASK(board->pieces[BLACK_BISHOP]);
                inputs[13] = FLIP_MASK(board->pieces[BLACK_ROOK]);
                inputs[14] = FLIP_MASK(board->pieces[BLACK_QUEEN]);
                inputs[15] = FLIP_MASK(board->pieces[WHITE_PAWN]);
                inputs[16] = FLIP_MASK(board->pieces[WHITE_KNIGHT]);
                inputs[17] = FLIP_MASK(board->pieces[WHITE_BISHOP]);
                inputs[18] = FLIP_MASK(board->pieces[WHITE_ROOK]);
                inputs[19] = FLIP_MASK(board->pieces[WHITE_QUEEN]);

                if(getColumn(board->kingSquare_w) > 3)
                {
                    for(int p = 0; p < TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                }
                if(getColumn(board->kingSquare_b) > 3)
                {
                    for(int p = TRACKED_PIECES; p < 2 * TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                }

                for(int color = 0; color < 2; color++)
                {
                    int baseIndex = (color == WHITE) ? kingBuckets[board->kingSquare_w] * BITS_PER_BUCKET : kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * BITS_PER_BUCKET;
                    
                    int trackedInputs = 0;

                    //First half matches side to move.
                    // side = 0 if color matches board->turn
                    int side = (color != board->turn);
                    
                    for(int piece = 0; piece < TRACKED_PIECES; piece++)
                    {
                        uint64_t mask = inputs[TRACKED_PIECES * color + piece];
                        while(mask)
                        {
                            if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                            else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                            trackedInputs++;
                            mask&=(mask - 1);
                        }
                    }
                    
                    //-1 padding
                    while(trackedInputs < 32)
                    {
                        if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = -1;
                        else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = -1;
                        trackedInputs++;
                    }
                }
            }
            
            free(board);
        }

        enqueueKernels(inputGroup, 0);
        hipEventSynchronize(hip_events.readLoss);


        validationLoss+=sumLoss(loss_buffer);
    }
    endTime = clock();
    duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC;
    validationLoss /= validationEntries;
    float minimumLoss = validationLoss;

    //Yellow text
    //Validation pos/sec is roughly double that of training. It doesn't have to perform backprop.
    printf("Initial Validation Loss = \033[0;33m%e\033[0m (%.1fs at %d pos/sec)\n", validationLoss, duration_sec, (int) (validationEntries / duration_sec));

    do{
        totalLoss = 0.0;

        shuffle_long(blockIndices, blockCount);
        startTime = clock();
        for(int minibatchNumber = 0; minibatchNumber < MINIBATCHES_PER_EPOCH; minibatchNumber++)
        {    

            int offset = minibatchNumber%FEN_SKIP;
            if(offset == 0)
            {
                fseek_64(trainingDataFile, blockIndices[minibatchNumber / FEN_SKIP], SEEK_SET);
                size_t size = fread(unsortedData, sizeof(CompactPosition), MINIBATCH_SIZE * FEN_SKIP, trainingDataFile);
                if(size < MINIBATCH_SIZE * FEN_SKIP) { DEBUG_ERROR("Failed reading trainingData from file."); continue; }
            }

            for(int i = 0; i < MINIBATCH_SIZE; i++)
            {
                batchData[i] = unsortedData[offset + i * FEN_SKIP];
            }

            inputGroup ^= 1;

            #pragma omp parallel
            {
                bitboard* board = create_board(NULL);
                
                #pragma omp for schedule(static)
                for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
                {
                    if(inputGroup == INPUT_GROUP_A)  loadTrainingData(batchData[entryNumber], board, &expectedOutputs_A[entryNumber]);
                    else loadTrainingData(batchData[entryNumber], board, &expectedOutputs_B[entryNumber]);

                    uint64_t inputs[2 * TRACKED_PIECES] = {0};

                    inputs[0] = board->pieces[WHITE_PAWN];
                    inputs[1] = board->pieces[WHITE_KNIGHT];
                    inputs[2] = board->pieces[WHITE_BISHOP];
                    inputs[3] = board->pieces[WHITE_ROOK];
                    inputs[4] = board->pieces[WHITE_QUEEN];
                    inputs[5] = board->pieces[BLACK_PAWN];
                    inputs[6] = board->pieces[BLACK_KNIGHT];
                    inputs[7] = board->pieces[BLACK_BISHOP];
                    inputs[8] = board->pieces[BLACK_ROOK];
                    inputs[9] = board->pieces[BLACK_QUEEN];

                    inputs[10] = FLIP_MASK(board->pieces[BLACK_PAWN]);
                    inputs[11] = FLIP_MASK(board->pieces[BLACK_KNIGHT]);
                    inputs[12] = FLIP_MASK(board->pieces[BLACK_BISHOP]);
                    inputs[13] = FLIP_MASK(board->pieces[BLACK_ROOK]);
                    inputs[14] = FLIP_MASK(board->pieces[BLACK_QUEEN]);
                    inputs[15] = FLIP_MASK(board->pieces[WHITE_PAWN]);
                    inputs[16] = FLIP_MASK(board->pieces[WHITE_KNIGHT]);
                    inputs[17] = FLIP_MASK(board->pieces[WHITE_BISHOP]);
                    inputs[18] = FLIP_MASK(board->pieces[WHITE_ROOK]);
                    inputs[19] = FLIP_MASK(board->pieces[WHITE_QUEEN]);

                    if(getColumn(board->kingSquare_w) > 3)
                    {
                        for(int p = 0; p < TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }
                    if(getColumn(board->kingSquare_b) > 3)
                    {
                        for(int p = TRACKED_PIECES; p < 2 * TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }


                    for(int color = 0; color < 2; color++)
                    {
                        int baseIndex = (color == 0) ? kingBuckets[board->kingSquare_w] * BITS_PER_BUCKET : kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * BITS_PER_BUCKET;

                        int trackedInputs = 0;

                        //First half matches side to move.
                        // side = 0 if color matches board->turn
                        int side = (color != board->turn);
                        
                        for(int piece = 0; piece < TRACKED_PIECES; piece++)
                        {
                            uint64_t mask = inputs[TRACKED_PIECES * color + piece];
                            while(mask)
                            {
                                if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                                else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                                trackedInputs++;
                                mask&=(mask - 1);
                            }
                        }
                        
                        //-1 padding
                        while(trackedInputs < 32)
                        {
                            if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = -1;
                            else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = -1;
                            trackedInputs++;
                        }
                    }
                }
                
                free(board);
            }

            enqueueKernels(inputGroup, 1);
            hipEventSynchronize(hip_events.readLoss);
            
            float loss = sumLoss(loss_buffer);
            totalLoss+=loss;

            if((minibatchNumber + 1) % 10 == 0) printf("\33[2K\r\tAnalyzed block %d/%d; Loss = %e", minibatchNumber + 1, MINIBATCHES_PER_EPOCH, loss / MINIBATCH_SIZE);
        }
        totalIterations++;
        endTime = clock();
        duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC; 

        //If we comment out hipEventSynchronize, we end up with 17.4 million positions/second.
        //Commenting out enqueueKernels as well nets us 18.4 million positions/second.
        //This is the upper limit bounded by the CPU I/O, and it is far above the current GPU-bound pos/sec.
        printf("\33[2K\rEpoch %d Loss = %e (%.1fs at %d pos/sec)", totalIterations, totalLoss/(MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH), duration_sec, (int) ((MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH) / duration_sec));

        //Validation
        rewind(validationData);
        startTime = clock();
        for(int i = 0; i < validationBlocks; i++)
        {
            size_t size = fread(batchData, sizeof(CompactPosition), MINIBATCH_SIZE, validationData);
            if(size < MINIBATCH_SIZE) { DEBUG_ERROR("Failed reading validation Data"); continue; }
            inputGroup ^= 1;

            #pragma omp parallel
            {
                bitboard* board = create_board(NULL);
                
                #pragma omp for schedule(static)
                for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
                {
                    if(inputGroup == INPUT_GROUP_A)  loadTrainingData(batchData[entryNumber], board, &expectedOutputs_A[entryNumber]);
                    else loadTrainingData(batchData[entryNumber], board, &expectedOutputs_B[entryNumber]);

                    uint64_t inputs[2 * TRACKED_PIECES] = {0};

                    inputs[0] = board->pieces[WHITE_PAWN];
                    inputs[1] = board->pieces[WHITE_KNIGHT];
                    inputs[2] = board->pieces[WHITE_BISHOP];
                    inputs[3] = board->pieces[WHITE_ROOK];
                    inputs[4] = board->pieces[WHITE_QUEEN];
                    inputs[5] = board->pieces[BLACK_PAWN];
                    inputs[6] = board->pieces[BLACK_KNIGHT];
                    inputs[7] = board->pieces[BLACK_BISHOP];
                    inputs[8] = board->pieces[BLACK_ROOK];
                    inputs[9] = board->pieces[BLACK_QUEEN];

                    inputs[10] = FLIP_MASK(board->pieces[BLACK_PAWN]);
                    inputs[11] = FLIP_MASK(board->pieces[BLACK_KNIGHT]);
                    inputs[12] = FLIP_MASK(board->pieces[BLACK_BISHOP]);
                    inputs[13] = FLIP_MASK(board->pieces[BLACK_ROOK]);
                    inputs[14] = FLIP_MASK(board->pieces[BLACK_QUEEN]);
                    inputs[15] = FLIP_MASK(board->pieces[WHITE_PAWN]);
                    inputs[16] = FLIP_MASK(board->pieces[WHITE_KNIGHT]);
                    inputs[17] = FLIP_MASK(board->pieces[WHITE_BISHOP]);
                    inputs[18] = FLIP_MASK(board->pieces[WHITE_ROOK]);
                    inputs[19] = FLIP_MASK(board->pieces[WHITE_QUEEN]);

                    if(getColumn(board->kingSquare_w) > 3)
                    {
                        for(int p = 0; p < TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }
                    if(getColumn(board->kingSquare_b) > 3)
                    {
                        for(int p = TRACKED_PIECES; p < 2 * TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }

                    for(int color = 0; color < 2; color++)
                    {
                        int baseIndex = (color == WHITE) ? kingBuckets[board->kingSquare_w] * BITS_PER_BUCKET : kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * BITS_PER_BUCKET;

                        int trackedInputs = 0;

                        //First half matches side to move.
                        // side = 0 if color matches board->turn
                        int side = (color != board->turn);
                        
                        for(int piece = 0; piece < TRACKED_PIECES; piece++)
                        {
                            uint64_t mask = inputs[TRACKED_PIECES * color + piece];
                            while(mask)
                            {
                                if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                                else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                                trackedInputs++;
                                mask&=(mask - 1);
                            }
                        }

                        //-1 padding
                        while(trackedInputs < 32)
                        {
                            if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = -1;
                            else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = -1;
                            trackedInputs++;
                        }
                    }
                }
                
                free(board);
            }
        
            enqueueKernels(inputGroup, 0);
            hipEventSynchronize(hip_events.readLoss);

            validationLoss+=sumLoss(loss_buffer);
        }
        
        endTime = clock();
        duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC;

        validationLoss /= validationEntries;

        if(validationLoss < minimumLoss)
        {
            getWeights(raw_weights);
            saveRawWeights();
            minimumLoss = validationLoss;

            //Green text
            printf("; Validation Loss = \033[0;32m%e\033[0m", validationLoss);
        }
        else 
        {
            //Red text
            printf("; Validation Loss = \033[0;31m%e\033[0m", validationLoss);
        }
        printf(" (%.1fs at %d pos/sec)\n", duration_sec, (int) (validationEntries / duration_sec));
    } while(validationLoss > maxAllowedError && totalIterations < maxIterations);

    
    fclose(trainingDataFile);
    fclose(validationData);
    free(blockIndices);
    free(batchData);
    free(unsortedData);

    freeHIP();
}