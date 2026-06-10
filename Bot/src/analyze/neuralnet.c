#include "analyze/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/engine.h"
#include "gpu/gpu_funcs.h"
#include "omp.h"
#include <float.h>
#include <string.h>

network_weights* nnue_weights = NULL;

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

void loadWeights()
{
    if(nnue_weights) return;
    nnue_weights = calloc(1, sizeof(network_weights));

    FILE* input = fopen(PROJECT_CWD "/import/weights.nnue", "rb");
    if(input)
    {
        size_t size = fread(nnue_weights, sizeof(network_weights), 1, input);
        fclose(input);
        if(size == 1) return;
    }
    
    DEBUG_ERROR("Failed to load neural network from file.");

    //Biases get left at 0.0 from calloc.

    double standardDeviation = sqrt(2.0/30.0);
    
    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            sampleNormalDistribution(&nnue_weights->weights1[i][j], standardDeviation);
        }
    }

    standardDeviation = sqrt(2.0 / 512.0);
    for(int i = 0; i < ACCUMULATOR_NODES; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            sampleNormalDistribution(&nnue_weights->weights2[j][i], standardDeviation);
        }
    }

    standardDeviation = sqrt(2.0 / 32.0);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            sampleNormalDistribution(&nnue_weights->weights3[j][i], standardDeviation);
        }
    }

    for(int b = 0; b < OUTPUT_BUCKETS; b++)
        {
            for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
            {
                sampleNormalDistribution(&nnue_weights->weights4[b][i], standardDeviation);
            }
        }
}

void saveWeights()
{
    FILE* output = fopen(PROJECT_CWD "/import/weights.nnue", "wb");
    if(output) fwrite(nnue_weights, sizeof(network_weights), 1, output);
    else DEBUG_ERROR("Failed to write neural network to file.");
    fclose(output);
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

        if (val > max_val) max_val = val;
        if (val < min_val) min_val = val;
        
        double delta = val - mean;
        mean += delta / (i + 1);
        double delta2 = val - mean;
        M2 += delta * delta2;

        //Absolute
        float abs_val = fabsf(val);

        if (abs_val > abs_max) abs_max = abs_val;
        if (abs_val < abs_min) abs_min = abs_val;

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
    assert(nnue_weights);

    print_weight_stats("weights1", &nnue_weights->weights1[0][0], sizeof(nnue_weights->weights1) / sizeof(float));
    print_weight_stats("weights1_bias", nnue_weights->weights1_bias, sizeof(nnue_weights->weights1_bias) / sizeof(float));
    print_weight_stats("weights2", &nnue_weights->weights2[0][0], sizeof(nnue_weights->weights2) / sizeof(float));
    print_weight_stats("weights2_bias", nnue_weights->weights2_bias,sizeof(nnue_weights->weights2_bias) / sizeof(float));
    print_weight_stats("weights3", &nnue_weights->weights3[0][0], sizeof(nnue_weights->weights3) / sizeof(float));
    print_weight_stats("weights3_bias",nnue_weights->weights3_bias, sizeof(nnue_weights->weights3_bias) / sizeof(float));
    print_weight_stats("weights4", &nnue_weights->weights4[0][0], sizeof(nnue_weights->weights4) / sizeof(float));
    print_weight_stats("weights4_bias", nnue_weights->weights4_bias, sizeof(nnue_weights->weights4_bias) / sizeof(float));
}

/*** Inference ***/
static inline float horizontalSIMDSum(__m256 vector)
{
    __m128 sum128 = _mm_add_ps(_mm256_castps256_ps128(vector), _mm256_extractf128_ps(vector, 1));
    sum128 = _mm_hadd_ps(sum128, sum128); //[A, B, C, D] -> [A + B, C + D, A + B, C + D]
    sum128 = _mm_hadd_ps(sum128, sum128); //[A + B, C + D, A + B, C + D] -> 4 copies of {A + B + C + D}
    return _mm_cvtss_f32(sum128);
}

void calculateHiddenLayer(float* inputValues, float* outputValues, int numInputs, int numOutputs, float weights[numOutputs][numInputs], float* biasWeights)
{
    float totalSum1, totalSum2, totalSum3, totalSum4;
    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex+=4)
    {
        __m256 v_output1 = _mm256_setzero_ps();
        __m256 v_output2 = _mm256_setzero_ps();
        __m256 v_output3 = _mm256_setzero_ps();
        __m256 v_output4 = _mm256_setzero_ps();

        //All layer lengths are divisible by 32 so no overflow.
        for(int inputIndex = 0; inputIndex < numInputs; inputIndex+=8)
        {
            __m256 v_inputBatch = _mm256_loadu_ps(&inputValues[inputIndex]);

            //Weights are an array of float w[OUTPUT NODES][INPUT NODES]
            __m256 v_weightsBatch1 = _mm256_loadu_ps(&weights[outputIndex + 0][inputIndex]);
            __m256 v_weightsBatch2 = _mm256_loadu_ps(&weights[outputIndex + 1][inputIndex]);
            __m256 v_weightsBatch3 = _mm256_loadu_ps(&weights[outputIndex + 2][inputIndex]);
            __m256 v_weightsBatch4 = _mm256_loadu_ps(&weights[outputIndex + 3][inputIndex]);

            //Multiply inputs by weights.
            //Add temp products to intermediate registers.
            v_output1 = _mm256_fmadd_ps(v_inputBatch, v_weightsBatch1, v_output1);
            v_output2 = _mm256_fmadd_ps(v_inputBatch, v_weightsBatch2, v_output2);
            v_output3 = _mm256_fmadd_ps(v_inputBatch, v_weightsBatch3, v_output3);
            v_output4 = _mm256_fmadd_ps(v_inputBatch, v_weightsBatch4, v_output4);
        }

        totalSum1 = horizontalSIMDSum(v_output1) + biasWeights[outputIndex + 0];
        totalSum2 = horizontalSIMDSum(v_output2) + biasWeights[outputIndex + 1];
        totalSum3 = horizontalSIMDSum(v_output3) + biasWeights[outputIndex + 2];
        totalSum4 = horizontalSIMDSum(v_output4) + biasWeights[outputIndex + 3];
        
        outputValues[outputIndex + 0] = _max(_min(totalSum1, 1.0f), 0.0f);
        outputValues[outputIndex + 1] = _max(_min(totalSum2, 1.0f), 0.0f);
        outputValues[outputIndex + 2] = _max(_min(totalSum3, 1.0f), 0.0f);
        outputValues[outputIndex + 3] = _max(_min(totalSum4, 1.0f), 0.0f);
    }
}

float calculateOutputLayer(float* h3, float weights[THIRD_HIDDEN_LAYER_NODES], float bias)
{
    __m256 v_output = _mm256_setzero_ps(); 

    for(int inputIndex = 0; inputIndex < THIRD_HIDDEN_LAYER_NODES; inputIndex+=8)
    {
        v_output = _mm256_fmadd_ps(_mm256_loadu_ps(&h3[inputIndex]), 
                                    _mm256_loadu_ps(&weights[inputIndex]), 
                                    v_output);
    }

    return horizontalSIMDSum(v_output) + bias;
}

float forwardPropagate(int turn, accumulator* acc, int pieceCount)
{
    int bucket = (pieceCount - 1) / 4;

    //create perspective-aligned accumulator (us vs them)
    float tempAccumulator[ACCUMULATOR_NODES];
    if(ISWHITE(turn))
    {
        memcpy(&tempAccumulator[0], &acc->accumulator[WHITE][0], sizeof(float) * ACCUMULATOR_NODES_PER_SIDE);
        memcpy(&tempAccumulator[ACCUMULATOR_NODES_PER_SIDE], &acc->accumulator[BLACK][0], sizeof(float) * ACCUMULATOR_NODES_PER_SIDE);
    }
    else
    {
        memcpy(&tempAccumulator[0], &acc->accumulator[BLACK][0], sizeof(float) * ACCUMULATOR_NODES_PER_SIDE);
        memcpy(&tempAccumulator[ACCUMULATOR_NODES_PER_SIDE], &acc->accumulator[WHITE][0], sizeof(float) * ACCUMULATOR_NODES_PER_SIDE);
    }

    float h2[SECOND_HIDDEN_LAYER_NODES];
    calculateHiddenLayer(tempAccumulator, h2, ACCUMULATOR_NODES, SECOND_HIDDEN_LAYER_NODES, nnue_weights->weights2, nnue_weights->weights2_bias);

    float h3[THIRD_HIDDEN_LAYER_NODES];
    calculateHiddenLayer(h2, h3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, nnue_weights->weights3, nnue_weights->weights3_bias);

    return calculateOutputLayer(h3, nnue_weights->weights4[bucket], nnue_weights->weights4_bias[bucket]);
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


static inline double sumLoss(double* loss)
{
    __m256d tempSum1 = _mm256_setzero_pd();
    __m256d tempSum2 = _mm256_setzero_pd();
    for(int i = 0; i < MINIBATCH_SIZE; i+=8)
    {
        tempSum1 = _mm256_add_pd(tempSum1, _mm256_loadu_pd(&loss[i]));
        tempSum2 = _mm256_add_pd(tempSum2, _mm256_loadu_pd(&loss[i + 4]));
    }
    tempSum1 = _mm256_add_pd(tempSum1, tempSum2);
    __m128d sum2 = _mm_add_pd(_mm256_castpd256_pd128((__m256d)tempSum1), _mm256_extractf128_pd(tempSum1, 1));
    return (_mm_cvtsd_f64(_mm_add_pd(sum2, _mm_permute_pd(sum2, 1))));
}

void train(int maxIterations, float maxAllowedError)
{
    FILE* trainingDataFile = fopen(PROJECT_CWD "/import/trainingData.bin", "rb");
    if(!trainingDataFile)
    {
        DEBUG_ERROR("Failed to open training data file.");
        exit(EXIT_FAILURE);
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

    //Aligned declarations
    align_alloc(short*, activeInputs_A, 64 * MINIBATCH_SIZE * sizeof(short), 4096);
    align_alloc(float*, expectedOutputs_A, MINIBATCH_SIZE * sizeof(float), 4096);
    align_alloc(char*, outputBuckets_A, 64 * MINIBATCH_SIZE * sizeof(char), 4096);
    
    align_alloc(short*, activeInputs_B, 64 * MINIBATCH_SIZE * sizeof(short), 4096);
    align_alloc(float*, expectedOutputs_B, MINIBATCH_SIZE * sizeof(float), 4096);
    align_alloc(char*, outputBuckets_B, 64 * MINIBATCH_SIZE * sizeof(char), 4096);
    
    align_alloc(double*, loss_buffer, MINIBATCH_SIZE * sizeof(double), 4096);
    
    initOpenCL(nnue_weights, activeInputs_A, expectedOutputs_A, outputBuckets_A, activeInputs_B, expectedOutputs_B, outputBuckets_B, loss_buffer);

    int totalIterations = 0;

    double totalLoss = 0.0; //accumulated value per iteration

    //Read data for next few minibatches, then shuffle data amongst them.
    CompactPosition* batchData = calloc(MINIBATCH_SIZE, sizeof(CompactPosition));
    CompactPosition* unsortedData = calloc(MINIBATCH_SIZE * FEN_SKIP, sizeof(CompactPosition));

    FILE* validationData = fopen(PROJECT_CWD  "/import/validationData.bin", "rb");

    fseek_64(validationData, 0, SEEK_END);
    int validationEntries = ftell_64(validationData) / sizeof(CompactPosition);
    validationEntries -= (validationEntries % MINIBATCH_SIZE);
    int validationBlocks = validationEntries / MINIBATCH_SIZE;
    double validationLoss = 0.0;
    rewind(validationData);

    int inputGroup = INPUT_GROUP_A;

    for(int i = 0; i < validationBlocks; i++)
    {
        size_t size = fread(batchData, sizeof(CompactPosition), MINIBATCH_SIZE, validationData);
        if(size < MINIBATCH_SIZE) { DEBUG_ERROR("Failed reading validation Data"); continue; }
        inputGroup ^= 1;

        #pragma omp parallel
        {
            bitboard* board = create_board();
            
            #pragma omp for schedule(static)
            for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
            {
                if(inputGroup == INPUT_GROUP_A)  loadTrainingData(batchData[entryNumber], board, &expectedOutputs_A[entryNumber]);
                else loadTrainingData(batchData[entryNumber], board, &expectedOutputs_B[entryNumber]);

                uint64_t inputs[24] = {0};

                inputs[0] = board->pieces[WHITE_PAWN];
                inputs[1] = board->pieces[WHITE_KNIGHT];
                inputs[2] = board->pieces[WHITE_BISHOP];
                inputs[3] = board->pieces[WHITE_ROOK];
                inputs[4] = board->pieces[WHITE_QUEEN];
                inputs[5] = board->pieces[WHITE_KING];
                inputs[6] = board->pieces[BLACK_PAWN];
                inputs[7] = board->pieces[BLACK_KNIGHT];
                inputs[8] = board->pieces[BLACK_BISHOP];
                inputs[9] = board->pieces[BLACK_ROOK];
                inputs[10] = board->pieces[BLACK_QUEEN];
                inputs[11] = board->pieces[BLACK_KING];

                inputs[12] = FLIP_MASK(board->pieces[BLACK_PAWN]);
                inputs[13] = FLIP_MASK(board->pieces[BLACK_KNIGHT]);
                inputs[14] = FLIP_MASK(board->pieces[BLACK_BISHOP]);
                inputs[15] = FLIP_MASK(board->pieces[BLACK_ROOK]);
                inputs[16] = FLIP_MASK(board->pieces[BLACK_QUEEN]);
                inputs[17] = FLIP_MASK(board->pieces[BLACK_KING]);
                inputs[18] = FLIP_MASK(board->pieces[WHITE_PAWN]);
                inputs[19] = FLIP_MASK(board->pieces[WHITE_KNIGHT]);
                inputs[20] = FLIP_MASK(board->pieces[WHITE_BISHOP]);
                inputs[21] = FLIP_MASK(board->pieces[WHITE_ROOK]);
                inputs[22] = FLIP_MASK(board->pieces[WHITE_QUEEN]);
                inputs[23] = FLIP_MASK(board->pieces[WHITE_KING]);

                if(getColumn(board->kingSquare_w) > 3)
                {
                    for(int p = 0; p < 12; p++) inputs[p] = mirrorBoard(inputs[p]);
                }
                if(getColumn(board->kingSquare_b) > 3)
                {
                    for(int p = 12; p < 24; p++) inputs[p] = mirrorBoard(inputs[p]);
                }

                for(int color = 0; color < 2; color++)
                {
                    int baseIndex = (color == 0) ? kingBuckets[board->kingSquare_w] * 768 : kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * 768;

                    int trackedInputs = 0;

                    //First half matches side to move.
                    // side = 0 if color matches board->turn
                    int side = (color != board->turn);
                    
                    for(int piece = 0; piece < 12; piece++)
                    {
                        uint64_t mask = inputs[12 * color + piece];
                        while(mask)
                        {
                            if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                            else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                            trackedInputs++;
                            mask&=(mask - 1);
                        }
                    }

                    
                    if(inputGroup == INPUT_GROUP_A) outputBuckets_A[entryNumber] = (trackedInputs - 1) / 4;
                    else outputBuckets_B[entryNumber] = (trackedInputs - 1) / 4;
                    
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

        clWaitForEvents(1, &readEvent);

        clReleaseEvent(readEvent);

        validationLoss+=sumLoss(loss_buffer);
    }

    validationLoss /= validationEntries;
    double minimumLoss = validationLoss;

    //Yellow text
    printf("Initial Validation Loss = \033[0;33m%e\033[0m\n", validationLoss);

    do{
        totalLoss = 0.0;

        shuffle_long(blockIndices, blockCount);
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
                bitboard* board = create_board();
                
                #pragma omp for schedule(static)
                for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
                {
                    if(inputGroup == INPUT_GROUP_A)  loadTrainingData(batchData[entryNumber], board, &expectedOutputs_A[entryNumber]);
                    else loadTrainingData(batchData[entryNumber], board, &expectedOutputs_B[entryNumber]);

                    uint64_t inputs[24] = {0};

                    inputs[0] = board->pieces[WHITE_PAWN];
                    inputs[1] = board->pieces[WHITE_KNIGHT];
                    inputs[2] = board->pieces[WHITE_BISHOP];
                    inputs[3] = board->pieces[WHITE_ROOK];
                    inputs[4] = board->pieces[WHITE_QUEEN];
                    inputs[5] = board->pieces[WHITE_KING];
                    inputs[6] = board->pieces[BLACK_PAWN];
                    inputs[7] = board->pieces[BLACK_KNIGHT];
                    inputs[8] = board->pieces[BLACK_BISHOP];
                    inputs[9] = board->pieces[BLACK_ROOK];
                    inputs[10] = board->pieces[BLACK_QUEEN];
                    inputs[11] = board->pieces[BLACK_KING];

                    inputs[12] = FLIP_MASK(board->pieces[BLACK_PAWN]);
                    inputs[13] = FLIP_MASK(board->pieces[BLACK_KNIGHT]);
                    inputs[14] = FLIP_MASK(board->pieces[BLACK_BISHOP]);
                    inputs[15] = FLIP_MASK(board->pieces[BLACK_ROOK]);
                    inputs[16] = FLIP_MASK(board->pieces[BLACK_QUEEN]);
                    inputs[17] = FLIP_MASK(board->pieces[BLACK_KING]);
                    inputs[18] = FLIP_MASK(board->pieces[WHITE_PAWN]);
                    inputs[19] = FLIP_MASK(board->pieces[WHITE_KNIGHT]);
                    inputs[20] = FLIP_MASK(board->pieces[WHITE_BISHOP]);
                    inputs[21] = FLIP_MASK(board->pieces[WHITE_ROOK]);
                    inputs[22] = FLIP_MASK(board->pieces[WHITE_QUEEN]);
                    inputs[23] = FLIP_MASK(board->pieces[WHITE_KING]);

                    if(getColumn(board->kingSquare_w) > 3)
                    {
                        for(int p = 0; p < 12; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }
                    if(getColumn(board->kingSquare_b) > 3)
                    {
                        for(int p = 12; p < 24; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }

                    for(int color = 0; color < 2; color++)
                    {
                        int baseIndex = (color == 0) ? kingBuckets[board->kingSquare_w] * 768 : kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * 768;

                        int trackedInputs = 0;

                        //First half matches side to move.
                        // side = 0 if color matches board->turn
                        int side = (color != board->turn);
                        
                        for(int piece = 0; piece < 12; piece++)
                        {
                            uint64_t mask = inputs[12 * color + piece];
                            while(mask)
                            {
                                if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                                else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                                trackedInputs++;
                                mask&=(mask - 1);
                            }
                        }
                        
                        if(inputGroup == INPUT_GROUP_A) outputBuckets_A[entryNumber] = (trackedInputs - 1) / 4;
                        else outputBuckets_B[entryNumber] = (trackedInputs - 1) / 4;
                        
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
            clWaitForEvents(1, &readEvent);
            clReleaseEvent(readEvent);
            
            double loss = sumLoss(loss_buffer);
            totalLoss+=loss;

            printf("\33[2K\r\tAnalyzed block %d/%d; Loss = %e", minibatchNumber + 1, MINIBATCHES_PER_EPOCH, loss / MINIBATCH_SIZE);
        }
        totalIterations++;
        
        printf("\33[2K\rEpoch %d Loss = %e", totalIterations, totalLoss/(MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH));

        //Validation
        rewind(validationData);
        for(int i = 0; i < validationBlocks; i++)
        {
            size_t size = fread(batchData, sizeof(CompactPosition), MINIBATCH_SIZE, validationData);
            if(size < MINIBATCH_SIZE) { DEBUG_ERROR("Failed reading validation Data"); continue; }
            inputGroup ^= 1;

            #pragma omp parallel
            {
                bitboard* board = create_board();
                
                #pragma omp for schedule(static)
                for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
                {
                    if(inputGroup == INPUT_GROUP_A)  loadTrainingData(batchData[entryNumber], board, &expectedOutputs_A[entryNumber]);
                    else loadTrainingData(batchData[entryNumber], board, &expectedOutputs_B[entryNumber]);

                    uint64_t inputs[24] = {0};

                    inputs[0] = board->pieces[WHITE_PAWN];
                    inputs[1] = board->pieces[WHITE_KNIGHT];
                    inputs[2] = board->pieces[WHITE_BISHOP];
                    inputs[3] = board->pieces[WHITE_ROOK];
                    inputs[4] = board->pieces[WHITE_QUEEN];
                    inputs[5] = board->pieces[WHITE_KING];
                    inputs[6] = board->pieces[BLACK_PAWN];
                    inputs[7] = board->pieces[BLACK_KNIGHT];
                    inputs[8] = board->pieces[BLACK_BISHOP];
                    inputs[9] = board->pieces[BLACK_ROOK];
                    inputs[10] = board->pieces[BLACK_QUEEN];
                    inputs[11] = board->pieces[BLACK_KING];

                    inputs[12] = FLIP_MASK(board->pieces[BLACK_PAWN]);
                    inputs[13] = FLIP_MASK(board->pieces[BLACK_KNIGHT]);
                    inputs[14] = FLIP_MASK(board->pieces[BLACK_BISHOP]);
                    inputs[15] = FLIP_MASK(board->pieces[BLACK_ROOK]);
                    inputs[16] = FLIP_MASK(board->pieces[BLACK_QUEEN]);
                    inputs[17] = FLIP_MASK(board->pieces[BLACK_KING]);
                    inputs[18] = FLIP_MASK(board->pieces[WHITE_PAWN]);
                    inputs[19] = FLIP_MASK(board->pieces[WHITE_KNIGHT]);
                    inputs[20] = FLIP_MASK(board->pieces[WHITE_BISHOP]);
                    inputs[21] = FLIP_MASK(board->pieces[WHITE_ROOK]);
                    inputs[22] = FLIP_MASK(board->pieces[WHITE_QUEEN]);
                    inputs[23] = FLIP_MASK(board->pieces[WHITE_KING]);

                    if(getColumn(board->kingSquare_w) > 3)
                    {
                        for(int p = 0; p < 12; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }
                    if(getColumn(board->kingSquare_b) > 3)
                    {
                        for(int p = 12; p < 24; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }

                    for(int color = 0; color < 2; color++)
                    {
                        int baseIndex = (color == 0) ? kingBuckets[board->kingSquare_w] * 768 : kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * 768;

                        int trackedInputs = 0;

                        //First half matches side to move.
                        // side = 0 if color matches board->turn
                        int side = (color != board->turn);
                        
                        for(int piece = 0; piece < 12; piece++)
                        {
                            uint64_t mask = inputs[12 * color + piece];
                            while(mask)
                            {
                                if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                                else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                                trackedInputs++;
                                mask&=(mask - 1);
                            }
                        }
                        
                        if(inputGroup == INPUT_GROUP_A) outputBuckets_A[entryNumber] = (trackedInputs - 1) / 4;
                        else outputBuckets_B[entryNumber] = (trackedInputs - 1) / 4;

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

            clWaitForEvents(1, &readEvent);

            clReleaseEvent(readEvent);

            validationLoss+=sumLoss(loss_buffer);
        }

        validationLoss /= validationEntries;

        if(validationLoss < minimumLoss)
        {
            getWeights(nnue_weights);
            saveWeights();
            minimumLoss = validationLoss;

            //Green text
            printf("; Validation Loss = \033[0;32m%e\033[0m\n", validationLoss);
        }
        else 
        {
            //Red text
            printf("; Validation Loss = \033[0;31m%e\033[0m\n", validationLoss);
        }
    } while(validationLoss > maxAllowedError && totalIterations < maxIterations);

    
    fclose(trainingDataFile);
    fclose(validationData);
    free(blockIndices);
    free(batchData);
    free(unsortedData);
    align_free(activeInputs_A);
    align_free(expectedOutputs_A);
    align_free(outputBuckets_A);
    align_free(activeInputs_B);
    align_free(expectedOutputs_B);
    align_free(outputBuckets_B);
    align_free(loss_buffer);

    freeOpenCL();
}