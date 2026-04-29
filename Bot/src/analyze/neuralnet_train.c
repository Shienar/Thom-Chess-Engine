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
        trainingNNUE->weights4_bias = 0.0;
    }
}

void save_trainingWeights()
{
    FILE* output = fopen("import/NNUE_Training.bin", "wb");
    if(output)
    {
        fwrite(trainingNNUE, sizeof(network_weights_training), 1, output);
    }
    else
    {
        DEBUG("Failed to write neural network to file.");
    }
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
    outputBytes->weights4_bias = (int32_t) max(min(INT32_MAX, lroundf(inputFloats->weights4_bias * QA * QB * QB * QB)), INT32_MIN);
}

void shuffle(int* arr, int count)
{
    for(int i = count-1; i > 0; i--)
    {
        int j = rand()%i;
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

void loadTrainingData(char* inputLine, bitboard* board, float* expectedOutput)
{
    char* ptr = NULL;
    char* FEN_String = strtok_s(inputLine, "|", &ptr);
    char* eval_String = strtok_s(NULL, "|", &ptr);
    char* result_String = strtok_s(NULL, "|", &ptr);

    float eval = 0.0f;
    float result = 0.0f;
    sscanf(eval_String, "%f", &eval);
    sscanf(result_String, "%f\n", &result);
    *expectedOutput = LAMBDA * (SIGMOID(eval / EVAL_SCALE)) + (1.0f - LAMBDA) * result;
    load_fen_string_to_board(board, FEN_String);
}

void train(int saveEveryNBlocks, int maxIterations, float maxAllowedError)
{

    int* blockNumbers = calloc(FILE_COUNT, sizeof(int));
    for(int i = 0; i < FILE_COUNT; i++)  blockNumbers[i] = i + 1;

    short* activeInputs_A = _aligned_malloc(64 * MINIBATCH_SIZE * sizeof(short), 4096); 
    float* expectedOutputs_A = _aligned_malloc(MINIBATCH_SIZE * sizeof(float), 4096);
    
    short* activeInputs_B = _aligned_malloc(64 * MINIBATCH_SIZE * sizeof(short), 4096); 
    float* expectedOutputs_B = _aligned_malloc(MINIBATCH_SIZE * sizeof(float), 4096);
    
    initOpenCL(trainingNNUE, activeInputs_A, expectedOutputs_A, activeInputs_B, expectedOutputs_B);

    int totalIterations = 0;

    double totalSumSquaredError = 0.0; //accumulated value per iteration

    char fileName[40] = {'\0'};
    char inputStrings[MINIBATCH_SIZE][105] = {'\0'};

    do{
        shuffle(blockNumbers, FILE_COUNT);
        totalSumSquaredError = 0.0;

        for(int blockIndex = 0; blockIndex < FILE_COUNT; blockIndex++)
        {
            int inputGroup = INPUT_GROUP(blockIndex);
            sprintf(fileName, "./training/trainingData_%d.txt", blockNumbers[blockIndex]);
            FILE* trainingData = fopen(fileName, "r");
            if(!trainingData)
            {
                DEBUG("\nFailed to open file: %s\n", fileName);
                continue;
            }
            if(inputGroup == INPUT_GROUP_A)
            {
                memset(activeInputs_A, 0, 64 * MINIBATCH_SIZE * sizeof(short));
            }
            else
            {
                memset(activeInputs_B, 0, 64 * MINIBATCH_SIZE * sizeof(short));
            }

            int count = 0;
            while(count < MINIBATCH_SIZE && fgets(inputStrings[count++], 105, trainingData));
            
            fclose(trainingData);

            #pragma omp parallel
            {
                bitboard* board = create_board();
                
                #pragma omp for schedule(static)
                for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
                {
                     if(inputGroup == INPUT_GROUP_A) 
                    {
                        loadTrainingData(inputStrings[entryNumber], board, &expectedOutputs_A[entryNumber]);
                    }
                    else 
                    {
                        loadTrainingData(inputStrings[entryNumber], board, &expectedOutputs_B[entryNumber]);
                    }

                    uint64_t inputs[20] = {0};

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
                        inputs[0] = mirrorBoard(inputs[0]);
                        inputs[1] = mirrorBoard(inputs[1]);
                        inputs[2] = mirrorBoard(inputs[2]);
                        inputs[3] = mirrorBoard(inputs[3]);
                        inputs[4] = mirrorBoard(inputs[4]);
                        inputs[5] = mirrorBoard(inputs[5]);
                        inputs[6] = mirrorBoard(inputs[6]);
                        inputs[7] = mirrorBoard(inputs[7]);
                        inputs[8] = mirrorBoard(inputs[8]);
                        inputs[9] = mirrorBoard(inputs[9]);
                    }
                    if(getColumn(board->kingSquare_b) > 3)
                    {
                        inputs[10] = mirrorBoard(inputs[10]);
                        inputs[11] = mirrorBoard(inputs[11]);
                        inputs[12] = mirrorBoard(inputs[12]);
                        inputs[13] = mirrorBoard(inputs[13]);
                        inputs[14] = mirrorBoard(inputs[14]);
                        inputs[15] = mirrorBoard(inputs[15]);
                        inputs[16] = mirrorBoard(inputs[16]);
                        inputs[17] = mirrorBoard(inputs[17]);
                        inputs[18] = mirrorBoard(inputs[18]);
                        inputs[19] = mirrorBoard(inputs[19]);
                    }

                    for(int side = 0; side < 2; side++)
                    {
                        int baseIndex = (side == 0) ? kingBuckets[board->kingSquare_w] * 640 : kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * 640;
                        
                        int trackedInputs = 0;
                        for(int piece = 0; piece < 10; piece++)
                        {
                            uint64_t mask = inputs[10 * side + piece];
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

            double sumSquaredError = 0.0;
            enqueueKernels(inputGroup, &sumSquaredError);

            clWaitForEvents(1, &readEvent);

            clReleaseEvent(readEvent);

            totalSumSquaredError+=sumSquaredError;


            printf("\33[2K\r\tAnalyzed block %d/%d; MSE = %e", blockIndex + 1, FILE_COUNT, sumSquaredError / MINIBATCH_SIZE);
            
            if(saveEveryNBlocks && blockIndex > 0 && blockIndex%saveEveryNBlocks == 0) 
            {
                getWeights(trainingNNUE);
                save_trainingWeights();
            }
        }
        totalIterations++;
        
        printf("\33[2K\rIteration %d MSE = %e\n", totalIterations, totalSumSquaredError/(MINIBATCH_SIZE * FILE_COUNT));

        getWeights(trainingNNUE);
        save_trainingWeights();
    } while((totalSumSquaredError/(MINIBATCH_SIZE * FILE_COUNT)) > maxAllowedError && totalIterations < maxIterations);

    free(blockNumbers);
    free(activeInputs_A);
    free(expectedOutputs_A);
    free(activeInputs_B);
    free(expectedOutputs_B);

    freeOpenCL();
}