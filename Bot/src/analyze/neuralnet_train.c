#include "neuralnet.h"
#include "../debug.h"
#include "../board/bitboard.h"
#include "../board/moves.h"
#include "engine.h"
#include <float.h>
#include <windows.h>
#include "../gpu/gpu_funcs.h"
#include "omp.h"

network_weights_training* trainingNNUE = NULL;

//Box-Muller transform.
void sampleNormalDistribution(float* dest, double standardDeviation) 
{
    double u1; 
    do { u1 = (double)rand() / (double) RAND_MAX; } while(u1 == 0);
    *dest = standardDeviation * sqrt(-2.0 * log(u1)) * cos(2 * PI *  (double)rand()/(double)RAND_MAX);
}

void load_trainingWeights()
{
    if(!trainingNNUE) trainingNNUE = calloc(1, sizeof(network_weights_training));

    FILE* input = fopen("import/NNUE_Training.bin", "rb");
    if(input)
    {
        fread(trainingNNUE, sizeof(network_weights_training), 1, input);
        fclose(input);
    }
    else
    {
        DEBUG("Failed to load neural network from file.\n");


        double standardDeviation = sqrt(2.0/30.0);
        
        for(int i = 0; i < HALF_INPUT_BITS; i++)
        {
            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            {
                sampleNormalDistribution(&trainingNNUE->weights1[i][j], standardDeviation);
            }
        }
        for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) trainingNNUE->weights1_bias[i] = 0.0;

        standardDeviation = sqrt(2.0 / 512.0);
        for(int i = 0; i < ACCUMULATOR_NODES; i++)
        {
            for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
            {
                sampleNormalDistribution(&trainingNNUE->weights2[j][i], standardDeviation);
            }
        }
        for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) trainingNNUE->weights2_bias[i] = 0.0;

        standardDeviation = sqrt(2.0 / 32.0);
        for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
        {
            for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
            {
                sampleNormalDistribution(&trainingNNUE->weights3[j][i], standardDeviation);
            }
        }
        for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) trainingNNUE->weights3_bias[i] = 0.0;

        for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
        {
            sampleNormalDistribution(&trainingNNUE->weights4[i], standardDeviation);
        }
        
        standardDeviation = sqrt(2.0 / 256.0);
        for(int i = 0; i < OUTPUT_BUCKETS; i++) 
        {
            trainingNNUE->weights4_bias[i] = 0.0;
            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            {
                sampleNormalDistribution(&trainingNNUE->directWeights[i][j], standardDeviation);
            }
        }

    }
}

void save_trainingWeights()
{
    FILE* output = fopen("import/NNUE_Training.bin", "wb");
    if(output) fwrite(trainingNNUE, sizeof(network_weights_training), 1, output);
    else DEBUG("Failed to write neural network to file.");
    fclose(output);
}

void quantizeWeights(network_weights_training* inputFloats, network_weights_playing* outputBytes)
{
    assert(inputFloats);
    assert(outputBytes);

    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            outputBytes->weights1[i][j] = (int16_t) max(min(INT16_MAX, lroundf(inputFloats->weights1[i][j] * QA)), INT16_MIN);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) outputBytes->weights1_bias[i] = (int16_t) max(min(INT16_MAX, lroundf(inputFloats->weights1_bias[i] * QA)), INT16_MIN);

    for(int i = 0; i < OUTPUT_BUCKETS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            outputBytes->directWeights[i][j] = (int16_t) max(min(INT8_MAX, lroundf(inputFloats->directWeights[i][j] * QA)), INT8_MIN);
        }
    }

    for(int i = 0; i < ACCUMULATOR_NODES; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights2[j][i] = (int8_t) max(min(INT8_MAX, lroundf(inputFloats->weights2[j][i] * QB)), INT8_MIN);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) outputBytes->weights2_bias[i] = (int32_t) max(min(INT32_MAX, lroundf(inputFloats->weights2_bias[i] * QA * QB)), INT32_MIN);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights3[j][i] = (int8_t) max(min(INT8_MAX, lroundf(inputFloats->weights3[j][i] * QB)), INT8_MIN);
        }
    }
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) outputBytes->weights3_bias[i] = (int32_t) max(min(INT32_MAX, lroundf(inputFloats->weights3_bias[i] * QA * QB * QB)), INT32_MIN);
    
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        outputBytes->weights4[i] = (int8_t) max(min(INT8_MAX, lroundf(inputFloats->weights4[i] * QB)), INT8_MIN);
    }
    for(int i = 0; i < OUTPUT_BUCKETS; i++) outputBytes->weights4_bias[i] = (int32_t) max(min(INT32_MAX, lroundf(inputFloats->weights4_bias[i] * QA * QB * QB * QB)), INT32_MIN);
}

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

void shuffle_struct(CompactPosition* arr, int count)
{
    for(int i = count-1; i > 0; i--)
    {
        int j = rand()%i;
        CompactPosition temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

void train(int maxIterations, float maxAllowedError)
{
    FILE* trainingDataFile = fopen("./import/trainingData.bin", "rb");
    if(!trainingDataFile)
    {
        DEBUG("\nFailed to open training data file.");
        exit(EXIT_FAILURE);
    }
    _fseeki64(trainingDataFile, 0, SEEK_END);
    uint64_t file_size = _ftelli64(trainingDataFile);
    rewind(trainingDataFile);

    uint64_t positionCount = file_size / sizeof(CompactPosition);

    //Used to skip to random position in the .bin file.
    //The division in blockcount effectively truncates extraneous positions that don't fit within a full MINIBATCH_SIZE * 8 block.
    int blockCount = positionCount /  (MINIBATCH_SIZE * MINIBATCHES_PER_SHUFFLE_BLOCK);
    uint64_t* blockIndices = calloc(blockCount,  sizeof(uint64_t));
    for (int i = 0; i < blockCount; i++) 
    {
        blockIndices[i] = (MINIBATCH_SIZE * MINIBATCHES_PER_SHUFFLE_BLOCK) * i * sizeof(CompactPosition);
    }

    short* activeInputs_A = _aligned_malloc(64 * MINIBATCH_SIZE * sizeof(short), 4096); 
    float* expectedOutputs_A = _aligned_malloc(MINIBATCH_SIZE * sizeof(float), 4096);
    char* outputBuckets_A = _aligned_malloc(MINIBATCH_SIZE * sizeof(char), 4096);
    
    short* activeInputs_B = _aligned_malloc(64 * MINIBATCH_SIZE * sizeof(short), 4096); 
    float* expectedOutputs_B = _aligned_malloc(MINIBATCH_SIZE * sizeof(float), 4096);
    char* outputBuckets_B = _aligned_malloc(MINIBATCH_SIZE * sizeof(char), 4096);
    
    initOpenCL(trainingNNUE, activeInputs_A, expectedOutputs_A, outputBuckets_A, activeInputs_B, expectedOutputs_B, outputBuckets_B);

    int totalIterations = 0;

    double totalSumSquaredError = 0.0; //accumulated value per iteration

    //Read data for next few minibatches, then shuffle data amongst them.
    CompactPosition* batchData = calloc(MINIBATCHES_PER_SHUFFLE_BLOCK * MINIBATCH_SIZE, sizeof(CompactPosition));

    FILE* validationData = fopen("./import/validationData.bin", "rb");

    _fseeki64(validationData, 0, SEEK_END);
    int validationEntries = _ftelli64(validationData) / sizeof(CompactPosition);
    validationEntries -= (validationEntries % MINIBATCH_SIZE);
    int validationBlocks = validationEntries / MINIBATCH_SIZE;
    double validationMSE = 0.0;
    rewind(validationData);

    int inputGroup = INPUT_GROUP_A;

    for(int i = 0; i < validationBlocks; i++)
    {
        fread(batchData, sizeof(CompactPosition), MINIBATCH_SIZE, validationData);
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
    
        double tempSSE = 0.0;
        enqueueKernels(inputGroup, &tempSSE, 0);

        clWaitForEvents(1, &readEvent);

        clReleaseEvent(readEvent);

        validationMSE+=tempSSE;
    }

    validationMSE /= validationEntries;
    double minimumMSE = validationMSE;

    //Yellow text
    printf("Initial Validation MSE = \033[0;33m%e\033[0m\n", validationMSE);

    do{
        totalSumSquaredError = 0.0;

        shuffle_long(blockIndices, blockCount);
        for(int minibatchNumber = 0; minibatchNumber < MINIBATCHES_PER_EPOCH; minibatchNumber++)
        {
            if(minibatchNumber % MINIBATCHES_PER_SHUFFLE_BLOCK == 0)
            {
                _fseeki64(trainingDataFile, blockIndices[minibatchNumber / MINIBATCHES_PER_SHUFFLE_BLOCK], SEEK_SET);
                fread(batchData, sizeof(CompactPosition), MINIBATCH_SIZE * MINIBATCHES_PER_SHUFFLE_BLOCK, trainingDataFile);
                shuffle_struct(batchData, MINIBATCH_SIZE * MINIBATCHES_PER_SHUFFLE_BLOCK);
            }
            CompactPosition* myBatchData = &batchData[MINIBATCH_SIZE * (minibatchNumber % MINIBATCHES_PER_SHUFFLE_BLOCK)];

            inputGroup ^= 1;

            #pragma omp parallel
            {
                bitboard* board = create_board();
                
                #pragma omp for schedule(static)
                for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
                {
                    if(inputGroup == INPUT_GROUP_A)  loadTrainingData(myBatchData[entryNumber], board, &expectedOutputs_A[entryNumber]);
                    else loadTrainingData(myBatchData[entryNumber], board, &expectedOutputs_B[entryNumber]);

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

            double sumSquaredError = 0.0;

            enqueueKernels(inputGroup, &sumSquaredError, 1);
            clWaitForEvents(1, &readEvent);
            clReleaseEvent(readEvent);
            
            totalSumSquaredError+=sumSquaredError;

            printf("\33[2K\r\tAnalyzed block %d/%d; MSE = %e", minibatchNumber + 1, MINIBATCHES_PER_EPOCH, sumSquaredError / MINIBATCH_SIZE);
        }
        totalIterations++;
        
        printf("\33[2K\rEpoch %d MSE = %e", totalIterations, totalSumSquaredError/(MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH));

        //Validation
        rewind(validationData);
        for(int i = 0; i < validationBlocks; i++)
        {
            fread(batchData, sizeof(CompactPosition), MINIBATCH_SIZE, validationData);
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
        
            double tempSSE = 0.0;
            enqueueKernels(inputGroup, &tempSSE, 0);

            clWaitForEvents(1, &readEvent);

            clReleaseEvent(readEvent);

            validationMSE+=tempSSE;
        }

        validationMSE /= validationEntries;

        if(validationMSE < minimumMSE)
        {
            getWeights(trainingNNUE);
            save_trainingWeights();
            minimumMSE = validationMSE;

            //Green text
            printf("; Validation MSE = \033[0;32m%e\033[0m\n", validationMSE);
        }
        else 
        {
            //Red text
            printf("; Validation MSE = \033[0;31m%e\033[0m\n", validationMSE);
        }
    } while(validationMSE > maxAllowedError && totalIterations < maxIterations);

    
    fclose(trainingDataFile);
    fclose(validationData);
    free(blockIndices);
    free(batchData);
    _aligned_free(activeInputs_A);
    _aligned_free(expectedOutputs_A);
    _aligned_free(outputBuckets_A);
    _aligned_free(activeInputs_B);
    _aligned_free(expectedOutputs_B);
    _aligned_free(outputBuckets_B);

    freeOpenCL();
}