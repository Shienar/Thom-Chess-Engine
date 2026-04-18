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
float learningRate = STARTING_LR;
float num_consecutive_increases = 0; //increases in loss / MSE are bad
float num_consecutive_decreases = 0;

void iterateTrainingWeights(void (*func)(float*, double*), network_weights_training* trainingWeights, double* context) 
{
    assert(func && trainingWeights);

    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            func(&trainingWeights->weights1[j][i], context);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) func(&trainingWeights->weights1_bias[i], context);

    for(int i = 0; i < ACCUMULATOR_NODES; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            func(&trainingWeights->weights2[j][i], context);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) func(&trainingWeights->weights2_bias[i], context);

    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            func(&trainingWeights->weights3[j][i], context);
        }
    }
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) func(&trainingWeights->weights3_bias[i], context);

    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        func(&trainingWeights->weights4[i], context);
    }
    func(&trainingWeights->weights4_bias, context);
}

/**
 * Box-Muller transform.
 */
void sampleNormalDistribution(float* dest, double standardDeviation) 
{
    double u1; 
    do { u1 = (double)rand() / (double) RAND_MAX; } while(u1 == 0);
    *dest = standardDeviation * sqrt(-2.0 * log(u1)) * cos(2 * PI *  (double)rand()/(double)RAND_MAX);
}

void load_trainingWeights()
{
    if(!trainingNNUE) trainingNNUE = CALLOC(1, sizeof(network_weights_training));

    FILE* input = fopen("import/NNUE_Training.bin", "rb");
    if(input)
    {
        fread(trainingNNUE, sizeof(network_weights_training), 1, input);
        fclose(input);
    }
    else
    {
        DEBUG("Failed to load neural network from file.\n");
        double standardDeviation = 0.1; // High deviation for sparse input layer.
        
        for(int i = 0; i < HALF_INPUT_BITS; i++)
        {
            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            {
                sampleNormalDistribution(&trainingNNUE->weights1[j][i], standardDeviation);
            }
        }
        for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) sampleNormalDistribution(&trainingNNUE->weights1_bias[i], standardDeviation);

        standardDeviation = sqrt(2.0 / 512.0); //He initialization for the rest of the weights.
        for(int i = 0; i < ACCUMULATOR_NODES; i++)
        {
            for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
            {
                sampleNormalDistribution(&trainingNNUE->weights2[j][i], standardDeviation);
            }
        }
        for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) sampleNormalDistribution(&trainingNNUE->weights2_bias[i], standardDeviation);

        standardDeviation = sqrt(2.0 / 32.0);
        for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
        {
            for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
            {
                sampleNormalDistribution(&trainingNNUE->weights3[j][i], standardDeviation);
            }
        }
        for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) sampleNormalDistribution(&trainingNNUE->weights3_bias[i], standardDeviation);

        for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
        {
            sampleNormalDistribution(&trainingNNUE->weights4[i], standardDeviation);
        }
        sampleNormalDistribution(&trainingNNUE->weights4_bias, standardDeviation);
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

void findAbsMax(float* comparedValue, double* max)
{
    if(fabsf(*comparedValue) > *max) *max = fabsf(*comparedValue);
}

void findSum(float* comparedValue, double* sum)
{
    *sum+=*comparedValue;
}

void quantizeWeights(network_weights_training* inputFloats, network_weights_playing* outputBytes)
{
    assert(inputFloats);
    assert(outputBytes);

    double maxValue = -DBL_MAX;
    double meanValue = 0.0;
    iterateTrainingWeights(findAbsMax, inputFloats, &maxValue);
    iterateTrainingWeights(findSum, inputFloats, &meanValue);
    meanValue = meanValue / (HALF_INPUT_BITS * ACCUMULATOR_NODES_PER_SIDE + 
                            ACCUMULATOR_NODES_PER_SIDE +
                            ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES +
                            SECOND_HIDDEN_LAYER_NODES +
                            SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES +
                            2 * THIRD_HIDDEN_LAYER_NODES + 1);
    
    float scalingFactor = maxValue / 127;
    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            outputBytes->weights1[j][i] = (uint8_t) roundf(inputFloats->weights1[j][i] / scalingFactor);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) outputBytes->weights1_bias[i] = (uint8_t) roundf(inputFloats->weights1_bias[i] / scalingFactor);

    for(int i = 0; i < ACCUMULATOR_NODES; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights2[j][i] = (uint8_t) roundf(inputFloats->weights2[j][i] / scalingFactor);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) outputBytes->weights2_bias[i] = (uint8_t) roundf(inputFloats->weights2_bias[i] / scalingFactor);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights3[j][i] = (uint8_t) roundf(inputFloats->weights3[j][i] / scalingFactor);
        }
    }
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) outputBytes->weights3_bias[i] = (uint8_t) roundf(inputFloats->weights3_bias[i] / scalingFactor);
    
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        outputBytes->weights4[i] = (uint8_t) roundf(inputFloats->weights4[i] / scalingFactor);
    }
    outputBytes->weights4_bias = (uint8_t) roundf(inputFloats->weights4_bias / scalingFactor);

    printf("Quantized Weights:\n");
    printf("\tScaling Factor: %f\n", scalingFactor);
    printf("\tMean: %f\n", meanValue);
    printf("\tMax: %f\n", maxValue);
}

static inline float horizontalSIMDSum_Float(__m256 vector)
{
    __m128 output_128 = _mm_add_ps(_mm256_extractf128_ps(vector, 1), _mm256_castps256_ps128(vector));
    output_128 = _mm_add_ps(output_128, _mm_movehl_ps(output_128, output_128));
    output_128 = _mm_add_ss(output_128, _mm_shuffle_ps(output_128, output_128, _MM_SHUFFLE(0, 0, 0, 1)));
    return _mm_cvtss_f32(output_128);
}

//__m256 stored 8 32-bit floats (ps = packed single-precision)
void calculateLayer_Floats(float* inputValues, float* outputValues, int numInputs, int numOutputs, float weights[numOutputs][numInputs], int inputOffset, float* biasWeights,  int applyCReLU)
{
    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex+=8)
    {
        __m256 v_output1 = _mm256_setzero_ps();
        __m256 v_output2 = _mm256_setzero_ps();
        __m256 v_output3 = _mm256_setzero_ps();
        __m256 v_output4 = _mm256_setzero_ps();
        __m256 v_output5 = _mm256_setzero_ps();
        __m256 v_output6 = _mm256_setzero_ps();
        __m256 v_output7 = _mm256_setzero_ps();
        __m256 v_output8 = _mm256_setzero_ps();

        for(int inputIndex = 0; inputIndex < numInputs; inputIndex+=8)
        {
            __m256 v_inputs = _mm256_loadu_ps(&inputValues[inputIndex]);

            //Weights are an array of float w[OUTPUT NODES][INPUT NODES] 
            //Passed as a single pointer to array head. inputIndex = row; outputIndex = column;
            __m256 v_weights1 = _mm256_loadu_ps(&weights[outputIndex + 0][inputIndex + inputOffset]);
            __m256 v_weights2 = _mm256_loadu_ps(&weights[outputIndex + 1][inputIndex + inputOffset]);
            __m256 v_weights3 = _mm256_loadu_ps(&weights[outputIndex + 2][inputIndex + inputOffset]);
            __m256 v_weights4 = _mm256_loadu_ps(&weights[outputIndex + 3][inputIndex + inputOffset]);
            __m256 v_weights5 = _mm256_loadu_ps(&weights[outputIndex + 4][inputIndex + inputOffset]);
            __m256 v_weights6 = _mm256_loadu_ps(&weights[outputIndex + 5][inputIndex + inputOffset]);
            __m256 v_weights7 = _mm256_loadu_ps(&weights[outputIndex + 6][inputIndex + inputOffset]);
            __m256 v_weights8 = _mm256_loadu_ps(&weights[outputIndex + 7][inputIndex + inputOffset]);

            //Multiply inputs by weights and add to intermediate.
            v_output1 = _mm256_fmadd_ps(v_inputs, v_weights1, v_output1);
            v_output2 = _mm256_fmadd_ps(v_inputs, v_weights2, v_output2);
            v_output3 = _mm256_fmadd_ps(v_inputs, v_weights3, v_output3);
            v_output4 = _mm256_fmadd_ps(v_inputs, v_weights4, v_output4);
            v_output5 = _mm256_fmadd_ps(v_inputs, v_weights5, v_output5);
            v_output6 = _mm256_fmadd_ps(v_inputs, v_weights6, v_output6);
            v_output7 = _mm256_fmadd_ps(v_inputs, v_weights7, v_output7);
            v_output8 = _mm256_fmadd_ps(v_inputs, v_weights8, v_output8);
        }

        outputValues[outputIndex + 0] = horizontalSIMDSum_Float(v_output1);
        outputValues[outputIndex + 1] = horizontalSIMDSum_Float(v_output2);
        outputValues[outputIndex + 2] = horizontalSIMDSum_Float(v_output3);
        outputValues[outputIndex + 3] = horizontalSIMDSum_Float(v_output4);
        outputValues[outputIndex + 4] = horizontalSIMDSum_Float(v_output5);
        outputValues[outputIndex + 5] = horizontalSIMDSum_Float(v_output6);
        outputValues[outputIndex + 6] = horizontalSIMDSum_Float(v_output7);
        outputValues[outputIndex + 7] = horizontalSIMDSum_Float(v_output8);
        if(biasWeights)
        {
            outputValues[outputIndex + 0]+= biasWeights[outputIndex + 0];
            outputValues[outputIndex + 1]+= biasWeights[outputIndex + 1];
            outputValues[outputIndex + 2]+= biasWeights[outputIndex + 2];
            outputValues[outputIndex + 3]+= biasWeights[outputIndex + 3];
            outputValues[outputIndex + 4]+= biasWeights[outputIndex + 4];
            outputValues[outputIndex + 5]+= biasWeights[outputIndex + 5];
            outputValues[outputIndex + 6]+= biasWeights[outputIndex + 6];
            outputValues[outputIndex + 7]+= biasWeights[outputIndex + 7];
        }
    }

    if(applyCReLU)
    {
        __m256 v_min = _mm256_setzero_ps();
        __m256 v_max = _mm256_set1_ps(1.0);
        __m256 v_grad = _mm256_set1_ps(LEAK_FACTOR);
        for(int i = 0; i < numOutputs; i+=8)
        {
            SIMD_SCReLU_Float(&outputValues[i], v_min, v_max, v_grad);
        }
    }

}

float forwardPropagate_Float(int turn, accumulator_training* floatAccumulator, int centerAndScaleAtZero)
{
    //Assume accumulator has already been updated.

    //accumulator[0] = white
    //accumulator[1] = black
    //trainingNNUE->weights2[0 to ACCUMULATOR_NODES_PER_SIDE] = current side to move
    //trainingNNUE->weights2[ACCUMULATOR_NODES_PER_SIDE to 2* ACCUMULATOR_NODES_PER_SIDE -1] = opponent's side
    float tempH2[2][SECOND_HIDDEN_LAYER_NODES];
    if(ISWHITE(turn))
    {
        calculateLayer_Floats(floatAccumulator->accumulator[0], tempH2[0], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, trainingNNUE->weights2, 0, trainingNNUE->weights2_bias, 0);
        calculateLayer_Floats(floatAccumulator->accumulator[1], tempH2[1], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, trainingNNUE->weights2, ACCUMULATOR_NODES_PER_SIDE, NULL, 0);
    }
    else
    {
        calculateLayer_Floats(floatAccumulator->accumulator[0], tempH2[0], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, trainingNNUE->weights2, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights2_bias, 0);
        calculateLayer_Floats(floatAccumulator->accumulator[1], tempH2[1], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, trainingNNUE->weights2, 0, NULL, 0);
    }
    
    __m256 v_min = _mm256_setzero_ps();
    __m256 v_max = _mm256_set1_ps(1.0);
    __m256 v_grad = _mm256_set1_ps(LEAK_FACTOR);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i+=8)
    {
        //floatAccumulator->rawH2[i] = tempH2[0][i] + tempH2[1][i] - trainingNNUE->weights2_bias[i];
        //floatAccumulator->h2[i] = SCReLU_Leaky(floatAccumulator->rawH2[i], 0, 1);
        _mm256_storeu_ps(&floatAccumulator->rawH2[i], _mm256_add_ps(_mm256_loadu_ps(&tempH2[0][i]), _mm256_loadu_ps(&tempH2[0][i])));
        memcpy(&floatAccumulator->h2[i], &floatAccumulator->rawH2[i], 256);
        SIMD_SCReLU_Float(&floatAccumulator->h2[i], v_min, v_max, v_grad);
    }

    calculateLayer_Floats(floatAccumulator->h2, floatAccumulator->rawH3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, trainingNNUE->weights3, 0, trainingNNUE->weights3_bias, 0);
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i+=8) 
    {
        //floatAccumulator->h3[i] = SCReLU_Leaky(floatAccumulator->rawH3[i], 0, 1);
        memcpy(&floatAccumulator->h3[i], &floatAccumulator->rawH3[i], 256);
        SIMD_SCReLU_Float(&floatAccumulator->h3[i], v_min, v_max, v_grad);
    }

    calculateLayer_Floats(floatAccumulator->h3, &floatAccumulator->outputNode, THIRD_HIDDEN_LAYER_NODES, OUTPUT_LAYER_NODES, &trainingNNUE->weights4, 0, &trainingNNUE->weights4_bias, 0);

    if(centerAndScaleAtZero) return (floatAccumulator->outputNode - 0.5) * 127;
    else return floatAccumulator->outputNode;
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

void loadTrainingData(char* inputLine, bitboard* board, float* eval)
{
    char* FEN_String = strtok(inputLine, "|");
    char* Eval_String = strtok(NULL, "|");

    sscanf(Eval_String, "%f\n", eval);
    load_fen_string_to_board(board, FEN_String);
}

void train(int saveEveryNBlocks, int maxIterations, float maxAllowedError, accumulator_training* floatAccumulator)
{
    assert(floatAccumulator);
        
    initOpenCL(trainingNNUE);

    int* blockNumbers = CALLOC(FILE_COUNT, sizeof(int));
    for(int i = 0; i < FILE_COUNT; i++)  blockNumbers[i] = i + 1;

    short* activeInputs = CALLOC(64 * POSITIONS_PER_FILE, sizeof(short)); 
    char* activeCount = CALLOC(POSITIONS_PER_FILE, sizeof(char));
    float* expectedOutputs = CALLOC(POSITIONS_PER_FILE, sizeof(float));

    int totalIterations = 0;

    double totalSumSquaredError = 0.0; //accumulated value per iteration

    //similar to circular array queue, but we don't care about dequeueing and we just replace oldest with newest
    double averageErrorWindow[WINDOW_SIZE] = {0.0};
    double averageErrorSum = 0.0;
    double previousAverageWindowError = 0.0;
    int insertionIndex = 0; 

    int warmupPeriod = WINDOW_SIZE;
    int cooldown = WINDOW_SIZE;

    char fileName[40] = {'\0'};
    char inputString[120] = {'\0'};

    do{
        shuffle(blockNumbers, FILE_COUNT);
        totalSumSquaredError = 0.0;

        for(int blockIndex = 0; blockIndex < FILE_COUNT; blockIndex++)
        {
            sprintf(fileName, "./training/trainingData_%d.txt", blockNumbers[blockIndex]);
            FILE* trainingData = fopen(fileName, "r");
            if(!trainingData)
            {
                DEBUG("\nFailed to open file: %s\n", fileName);
                continue;
            }
            memset(activeInputs, 0, 64 * POSITIONS_PER_FILE * sizeof(short));
            memset(activeCount, 0, POSITIONS_PER_FILE * sizeof(char));

            int trackedEntries = 0;
            bitboard* board = create_board(); 
            while(trackedEntries < POSITIONS_PER_FILE && fgets(inputString, 120, trainingData))
            {
                loadTrainingData(inputString, board, &expectedOutputs[trackedEntries]);
                trackedEntries++;

                activeCount[trackedEntries] = __builtin_popcountll(board->pieces_all&(~(board->king_b|board->king_w)));

                uint64_t inputs[20] = {0};

                inputs[0] = board->pawn_w;
                inputs[1] = board->knight_w;
                inputs[2] = board->bishop_w;
                inputs[3] = board->rook_w;
                inputs[4] = board->queen_w;
                inputs[5] = board->pawn_b;
                inputs[6] = board->knight_b;
                inputs[7] = board->bishop_b;
                inputs[8] = board->rook_b;
                inputs[9] = board->queen_b;

                inputs[10] = FLIP_MASK(board->pawn_b);
                inputs[11] = FLIP_MASK(board->knight_b);
                inputs[12] = FLIP_MASK(board->bishop_b);
                inputs[13] = FLIP_MASK(board->rook_b);
                inputs[14] = FLIP_MASK(board->queen_b);
                inputs[15] = FLIP_MASK(board->pawn_w);
                inputs[16] = FLIP_MASK(board->knight_w);
                inputs[17] = FLIP_MASK(board->bishop_w);
                inputs[18] = FLIP_MASK(board->rook_w);
                inputs[19] = FLIP_MASK(board->queen_w);

                if(getColumn(board->kingSquare_w) > 4)
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
                if(getColumn(board->kingSquare_b) > 4)
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
                            activeInputs[POSITIONS_PER_FILE * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                            trackedInputs++;
                            mask&=(mask - 1);
                        }
                    }
                }
                
            }
            destroy_board(board);

            fclose(trainingData);

            
            double sumSquaredError = 0.0;
            enqueueKernels(activeInputs, activeCount, expectedOutputs, learningRate, &sumSquaredError);
            clWaitForEvents(1, &readEvent);

            sumSquaredError /= POSITIONS_PER_FILE;
            averageErrorSum += sumSquaredError - averageErrorWindow[insertionIndex];
            averageErrorWindow[insertionIndex] = sumSquaredError;
            double averageWindowError = averageErrorSum / WINDOW_SIZE;
            insertionIndex = (insertionIndex + 1)%WINDOW_SIZE;

            //GreedyLR
            if(averageWindowError < (previousAverageWindowError * (1 - LR_THRESHOLD)))
            {
                num_consecutive_decreases++;
                num_consecutive_increases = 0;
            }
            else 
            {
                num_consecutive_decreases = 0;
                num_consecutive_increases++;
            }
            if(cooldown)
            {
                cooldown--;
                num_consecutive_increases = 0;
            }
            if(warmupPeriod)
            {
                warmupPeriod--;
                num_consecutive_decreases = 0;
            }
            if(num_consecutive_decreases > PATIENCE)
            {
                learningRate/=LR_FACTOR;
                learningRate = min(MAX_LR, max(learningRate, MIN_LR));
                warmupPeriod = WARMUP_PERIOD;
            }
            else if(num_consecutive_increases > PATIENCE)
            {
                learningRate*=LR_FACTOR;
                learningRate = min(MAX_LR, max(learningRate, MIN_LR));
                cooldown = COOLDOWN_PERIOD;
            }

            previousAverageWindowError = averageWindowError;

            printf("\33[2K\r\tAnalyzed block %d/%d; Block MSE = %e; Window MSE = %e", blockIndex + 1, FILE_COUNT, sumSquaredError, averageWindowError);
            
            if(saveEveryNBlocks && blockIndex > 0 && blockIndex%saveEveryNBlocks == 0) 
            {
                getWeights(trainingNNUE);
                save_trainingWeights();
            }
        }
        totalIterations++;
        
        printf("\33[2K\rIteration %d MSE = %e\n", totalIterations, totalSumSquaredError/(POSITIONS_PER_FILE * FILE_COUNT));

        getWeights(trainingNNUE);
        save_trainingWeights();
    } while((totalSumSquaredError/(POSITIONS_PER_FILE * FILE_COUNT)) > maxAllowedError && totalIterations < maxIterations);

    FREE(blockNumbers);
    FREE(activeInputs);
    FREE(activeCount);
    FREE(expectedOutputs);

    freeOpenCL();
}

void generateTrainingData(int depth, int maxTime, accumulator_training* floatAccumulator)
{
    int fileNumber = 1;
    char fileName[40] = {'\0'};
    FILE* output;
    while(1)
    {
        sprintf(fileName, "./training/trainingData_%d.txt", fileNumber);
        output = fopen(fileName, "r");
        if(!output) break;
        else
        {
            fclose(output);
            fileNumber++;
        }

    }

    int entryCount = 0;
    char FEN[100] = {'\0'};
    if(fileNumber > 1)
    {
        fileNumber--;
        sprintf(fileName, "./training/trainingData_%d.txt", fileNumber);
        output = fopen(fileName, "r");
        while(fgets(FEN, 100, output)) entryCount++;
        fclose(output);
    }

    while(fileNumber <= FILE_COUNT)
    {
        sprintf(fileName, "./training/trainingData_%d.txt", fileNumber);
        output = fopen(fileName, "a");


        while(entryCount < POSITIONS_PER_FILE)
        {
            bitboard* board = create_board();
            loadInputAccumulator(board, floatAccumulator, TRAINING, BLACK|WHITE);

            transpositionTable = create_hashTable_tt();

            //Avoid boring games.
            int movesSinceLastInterestingMove = 0;

            while(1)
            {
                move* bestMove = calculateBestMove(board, depth, maxTime, TRAINING);
                
                //No one is in check and the best move isn't a capture.
                if(!bestMove->capturedPiece && !(board->flags&0x30) && transposition_table_get(board, transpositionTable))
                {
                    movesSinceLastInterestingMove = 0;

                    export_fen_from_board(board, FEN);
                    float evaluation = (float) transposition_table_get(board, transpositionTable)->evaluation;
                    evaluation = (evaluation / 127) + 0.5;
                    fprintf(output, "%s|%f\n", FEN, evaluation);
                    
                    entryCount++;
                    printf("\rFile %d/%d, Entries: %d/%d", fileNumber, FILE_COUNT, entryCount, POSITIONS_PER_FILE);

                }
                else movesSinceLastInterestingMove++;

                if(board->victor || movesSinceLastInterestingMove > 10 || entryCount >= POSITIONS_PER_FILE) break;
                if(moveFromStruct(board, bestMove)) break;
            }
            destroy_hashTable_tt(transpositionTable);
            transpositionTable = NULL;
            destroy_board(board);
        }

        fileNumber++;
        entryCount = 0;

        fclose(output);
    }
    
    FREE(trainingNNUE);
}