#include "../include/neuralnet.h"
#include "../include/debug.h"
#include "../include/bitboard.h"
#include "../include/moves.h"
#include "../include/engine.h"
#include <math.h>
#include <float.h>
#include <immintrin.h>
#include <windows.h>

network_weights_training* trainingNNUE = NULL;

void iterateTrainingWeights(void (*func)(float*, float*), network_weights_training* trainingWeights, float* context) 
{
    if(!func || !trainingWeights)
    {
        DEBUG("Passed null arguments to iterator.")
        return;
    }
    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            func(&trainingWeights->weights1[i][j], context);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) func(&trainingWeights->weights1_bias[i], context);

    for(int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            func(&trainingWeights->weights2[i][j], context);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) func(&trainingWeights->weights2_bias[i], context);
    
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            func(&trainingWeights->weights3[i][j], context);
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
void sampleNormalDistribution(float* dest, float* standardDeviation) 
{
    double u1; 
    do { u1 = (double)rand() / (double) RAND_MAX; } while(u1 == 0);
    *dest = *standardDeviation * sqrt(-2.0 * log(u1)) * cos(2 * 6.283185307179586 *  (double)rand()/(double)RAND_MAX);
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

        float standardDeviation = 0.01;
        iterateTrainingWeights(sampleNormalDistribution, trainingNNUE, &standardDeviation);
        memset(&trainingNNUE->inputNodes, 0, sizeof(trainingNNUE->inputNodes));
        memset(&trainingNNUE->accumulator, 0,  sizeof(trainingNNUE->accumulator));
        memset(&trainingNNUE->h2, 0,  sizeof(trainingNNUE->h2));
        memset(&trainingNNUE->h3, 0,  sizeof(trainingNNUE->h3));
        memset(&trainingNNUE->outputNode, 0,  sizeof(trainingNNUE->outputNode));
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
        DEBUG("Failed to write neural network to file.")
    }
    fclose(output);
}

void findAbsMax(float* comparedValue, float* max)
{
    if(fabsf(*comparedValue) > *max) *max = fabsf(*comparedValue);
}

void findSum(float* comparedValue, float* sum)
{
    *sum+=*comparedValue;
}

void quantizeWeights(network_weights_training* inputFloats, network_weights_playing* outputBytes)
{
    if(!inputFloats) 
    {
        DEBUG("Cannot quantize null input.")
        return;
    }
    else if(!outputBytes)
    {
        DEBUG("Cannot quantize to null output.")
        return;
    }

    float maxValue = -FLT_MAX;
    float meanValue = 0.0;
    iterateTrainingWeights(findAbsMax, inputFloats, &maxValue);
    iterateTrainingWeights(findSum, inputFloats, &meanValue);
    meanValue = meanValue / (HALF_INPUT_BITS * ACCUMULATOR_NODES_PER_SIDE + 
                            ACCUMULATOR_NODES_PER_SIDE +
                            2 * ACCUMULATOR_NODES_PER_SIDE * SECOND_HIDDEN_LAYER_NODES +
                            SECOND_HIDDEN_LAYER_NODES +
                            SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES +
                            2 * THIRD_HIDDEN_LAYER_NODES + 1);
    
    inputFloats->scalingFactor = maxValue / 127;
    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            outputBytes->weights1[i][j] = (uint8_t) roundf(inputFloats->weights1[i][j] / inputFloats->scalingFactor);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) outputBytes->weights1_bias[i] = (uint8_t) roundf(inputFloats->weights1_bias[i] / inputFloats->scalingFactor);

    for(int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights2[i][j] = (uint8_t) roundf(inputFloats->weights2[i][j] / inputFloats->scalingFactor);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) outputBytes->weights2_bias[i] = (uint8_t) roundf(inputFloats->weights2_bias[i] / inputFloats->scalingFactor);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights3[i][j] = (uint8_t) roundf(inputFloats->weights3[i][j] / inputFloats->scalingFactor);
        }
    }
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) outputBytes->weights3_bias[i] = (uint8_t) roundf(inputFloats->weights3_bias[i] / inputFloats->scalingFactor);
    
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        outputBytes->weights4[i] = (uint8_t) roundf(inputFloats->weights4[i] / inputFloats->scalingFactor);
    }
    outputBytes->weights4_bias = (uint8_t) roundf(inputFloats->weights4_bias / inputFloats->scalingFactor);

    printf("Quantized Weights:\n");
    printf("\tScaling Factor: %f\n", inputFloats->scalingFactor);
    printf("\tMean: %f\n", meanValue);
    printf("\tMax: %f\n", maxValue);
    memset(&outputBytes->inputNodes, 0, sizeof(outputBytes->inputNodes));
    memset(&outputBytes->accumulator, 0, sizeof(outputBytes->accumulator));
}

float SCReLU_Float(float val, float min, float max)
{
    if(val <= min) return min*min;
    if(val >= max) return max*max;
    return val*val;
}
float SCReLU_derivative(float val, float min, float max)
{
    return (val <= min || val >= max)?(0.0):(2*val);
}

//__m256 stored 8 32-bit floats (ps = packed single-precision)
void calculateLayer_Floats(float* inputValues, float* outputValues, int numInputs, int numOutputs, float weights[numInputs][numOutputs], float* biasWeights,  int applyCReLU)
{
    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex++)
    {
       if(biasWeights) outputValues[outputIndex] = biasWeights[outputIndex];
       else outputValues[outputIndex] = 0.0;

        __m256 intermediate1 = _mm256_setzero_ps();
        __m256 intermediate2 = _mm256_setzero_ps();
        __m256 intermediate3 = _mm256_setzero_ps();
        __m256 intermediate4 = _mm256_setzero_ps();

        //All layer lengths are divisible by 32 so no overflow.
        for(int inputIndex = 0; inputIndex < numInputs; inputIndex+=32)
        {
            __m256 inputBatch1 = _mm256_loadu_ps(&inputValues[inputIndex]);
            __m256 inputBatch2 = _mm256_loadu_ps(&inputValues[inputIndex + 8]);
            __m256 inputBatch3 = _mm256_loadu_ps(&inputValues[inputIndex + 16]);
            __m256 inputBatch4 = _mm256_loadu_ps(&inputValues[inputIndex + 24]);

            //Weights are an array of float w[INPUT NODES][OUTPUTS NODES]
            //Passed as a single pointer to array head. inputIndex = row; outputIndex = column;
            __m256 weightsBatch1 = _mm256_loadu_ps(&weights[inputIndex][outputIndex]);
            __m256 weightsBatch2 = _mm256_loadu_ps(&weights[inputIndex + 8][outputIndex]);
            __m256 weightsBatch3 = _mm256_loadu_ps(&weights[inputIndex + 16][outputIndex]);
            __m256 weightsBatch4 = _mm256_loadu_ps(&weights[inputIndex + 24][outputIndex]);

            //Multiply inputs by weights and add to intermediate.
            intermediate1 = _mm256_fmadd_ps(inputBatch1, weightsBatch1, intermediate1);
            intermediate2 = _mm256_fmadd_ps(inputBatch2, weightsBatch2, intermediate2);
            intermediate3 = _mm256_fmadd_ps(inputBatch3, weightsBatch3, intermediate3);
            intermediate4 = _mm256_fmadd_ps(inputBatch4, weightsBatch4, intermediate4);
        }

        //Add the four registers together. Sum stored in intermediate1.
        intermediate1 = _mm256_add_ps(intermediate1, intermediate2);
        intermediate3 = _mm256_add_ps(intermediate3, intermediate4);
        intermediate1 = _mm256_add_ps(intermediate1, intermediate3);

        /**
         * Simplify the 256-bit register into a 32-bit sum.
         * intermediate1 = [f0, f1, f2, f3| f4, f5, f6, f7]
         * 
         * Instruction 1: Horizontally add intermediate1 with itself.
         *      sum256 = [(f0+f1), (f2+f3), (f0+f1), (f2+f3) | (f4+f5), (f6+f7), (f4+f5), (f6+f7)]
         * Instruction 2: Horizontally add intermediate1 with itself again.
         *      sum256 = [(f0+f1+f2+f3), (f0+f1+f2+f3), (f0+f1+f2+f3), (f0+f1+f2+f3) | (f4+f5+f6+f7), (f4+f5+f6+f7), (f4+f5+f6+f7), (f4+f5+f6+f7)]
         *      - The lower 128 bits contain duplicate sums of the first 128 bits
         *      - The upper 128 bits contain duplicate sums of the last 128 bits
         * Instruction 3: Extract the upper 128 bits.
         *      - _mm256_extractf128_ps(intermediate1, 1);
         * Instruction 4: Cast the lower 128 bits into a 128 bit register.
         *      - _mm256_castps256_ps128(intermediate1)
         * Instrction 5: Add the first 32-bit floats of the two 128-bit registers together.
         *      - _mm_add_ss()
         * Instruction 6: Cast to a regular float datatype.
         *      - _m_cvtss_f32()
         */
        intermediate1 = _mm256_hadd_ps(intermediate1, intermediate1);
        intermediate1 = _mm256_hadd_ps(intermediate1, intermediate1);
        outputValues[outputIndex] += _mm_cvtss_f32(_mm_add_ss(_mm256_extractf128_ps(intermediate1, 1), _mm256_castps256_ps128(intermediate1)));

        if(applyCReLU) outputValues[outputIndex] = SCReLU_Float(outputValues[outputIndex], 0, 1);
    }
}

float forwardPropagate_Float(int turn)
{
    //Assume accumulator has already been updated.

    //accumulator[0] = white
    //accumulator[1] = black
    //trainingNNUE->weights2[0 to ACCUMULATOR_NODES_PER_SIDE] = current side to move
    //trainingNNUE->weights2[ACCUMULATOR_NODES_PER_SIDE to 2* ACCUMULATOR_NODES_PER_SIDE -1] = opponent's side
    float tempH2[2][SECOND_HIDDEN_LAYER_NODES];
    if(ISWHITE(turn))
    {
        calculateLayer_Floats(trainingNNUE->accumulator[0], tempH2[0], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, trainingNNUE->weights2, trainingNNUE->weights2_bias, 0);
        calculateLayer_Floats(trainingNNUE->accumulator[1], tempH2[1], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, &trainingNNUE->weights2[ACCUMULATOR_NODES_PER_SIDE], NULL, 0);
    }
    else
    {
        calculateLayer_Floats(trainingNNUE->accumulator[0], tempH2[0], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, &trainingNNUE->weights2[ACCUMULATOR_NODES_PER_SIDE], trainingNNUE->weights2_bias, 0);
        calculateLayer_Floats(trainingNNUE->accumulator[1], tempH2[1], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, trainingNNUE->weights2, NULL, 0);
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        //The above step added the biases twice.
        trainingNNUE->h2[i] = SCReLU_Float(tempH2[0][i] + tempH2[1][i] - trainingNNUE->weights2_bias[i], 0, 1);
    }

    calculateLayer_Floats(trainingNNUE->h2, trainingNNUE->h3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, trainingNNUE->weights3, trainingNNUE->weights3_bias, 1);
    calculateLayer_Floats(trainingNNUE->h3, &trainingNNUE->outputNode, THIRD_HIDDEN_LAYER_NODES, OUTPUT_LAYER_NODES, &trainingNNUE->weights4, &trainingNNUE->weights4_bias, 0);

    return trainingNNUE->outputNode;
}

void shuffle(long* arr, int count)
{
    for(int i = count-1; i > 0; i--)
    {
        int j = rand()%i;
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

void backpropagate(int saveEveryNIterations, int maxIterations, float maxAllowedError)
{
    FILE* trainingData = fopen("./import/trainingData.bin", "rb");
    
    fseek(trainingData, 0, SEEK_END);
    int entryCount = ftell(trainingData)/sizeof(network_training_data);

    rewind(trainingData);

    long blockNumbers[NUMBER_OF_BLOCKS] = {0};
    int entriesPerBlock = min(entryCount, (int) (entryCount / NUMBER_OF_BLOCKS));
    for(int i = 0; i < NUMBER_OF_BLOCKS; i++)
    {
        blockNumbers[i] = (long)entriesPerBlock*i*sizeof(network_training_data);
    }

    int totalIterations = 0;

    double prevSumSquaredError = 0.0;
    double sumSquaredError = 0.0;
    int sameErrorXTimesInARow = 0;
    float expectedOutput = 0.0;

    network_training_data data = {0};
    do{
        sumSquaredError = 0.0;
        shuffle(blockNumbers, NUMBER_OF_BLOCKS);

        for(int blockIndex = 0; blockIndex < NUMBER_OF_BLOCKS; blockIndex++)
        {
            fseek(trainingData, blockNumbers[blockIndex], SEEK_SET);

            for(int blockOffset = 0; blockOffset < entriesPerBlock; blockOffset++)
            {
                fread(&data, sizeof(network_training_data), 1, trainingData);
                loadInputAccumulator(&data.board, TRAINING_NNUE);
                forwardPropagate_Float(data.board.turn);
                expectedOutput = data.evaluation;

                sumSquaredError+= pow((double) (trainingNNUE->outputNode - expectedOutput), 2.0);

                //Calculate Edge Weight Deltas
                float delta4 = (expectedOutput - trainingNNUE->outputNode);

                float delta3[THIRD_HIDDEN_LAYER_NODES] = {0};
                for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) 
                {
                    delta3[i] = delta4 * trainingNNUE->weights4[i] * SCReLU_derivative(trainingNNUE->h3[i], 0, 1);
                }
                
                float delta2[SECOND_HIDDEN_LAYER_NODES] = {0};
                for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
                {
                    float sum = 0.0f;
                    for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) sum+= delta3[j] * trainingNNUE->weights3[i][j];
                    
                    delta2[i] = sum * SCReLU_derivative(trainingNNUE->h2[i], 0, 1);
                }

                float delta1[2][ACCUMULATOR_NODES_PER_SIDE] = {0};
                for (int side = 0; side < 2; side++) 
                {
                    for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) 
                    {
                        float sum = 0.0f;
                        int offset = side * ACCUMULATOR_NODES_PER_SIDE;
                        for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) sum += delta2[j] * trainingNNUE->weights2[offset + i][j];
                        
                        delta1[side][i] = sum * SCReLU_derivative(trainingNNUE->accumulator[side][i], 0, 1);
                    }
                }

                //Apply Edge Weight Deltas
                for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) 
                {
                    trainingNNUE->weights4[i]+= LEARNING_RATE * delta4 * trainingNNUE->h3[i];
                }
                trainingNNUE->weights4_bias+= LEARNING_RATE * delta4;

                for (int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
                {
                    for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) trainingNNUE->weights3[i][j]+= LEARNING_RATE * delta3[j] * trainingNNUE->h2[i];
                }
                for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) trainingNNUE->weights3_bias[j]+= LEARNING_RATE * delta3[j];
                
                for (int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++) 
                {
                    for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) 
                    {
                        trainingNNUE->weights2[i][j] += LEARNING_RATE * delta2[j] * trainingNNUE->accumulator[(int) (i / ACCUMULATOR_NODES_PER_SIDE)][i % ACCUMULATOR_NODES_PER_SIDE];
                    }
                }
                for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) trainingNNUE->weights2_bias[j] += LEARNING_RATE * delta2[j];
                
                for (int i = 0; i < 640; i++) 
                {
                    uint64_t inputBitboard_White = trainingNNUE->inputNodes[i];
                    uint64_t inputBitboard_Black = trainingNNUE->inputNodes[640 + i];
                    if(inputBitboard_White != 0) //Don't update the edge weights if they didn't contribute.
                    {
                        for(int square = 0; square < 64; square++)
                        {
                            if(inputBitboard_White&(1ull<<square))
                            {
                                for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
                                {
                                    trainingNNUE->weights1[(64 * i) + square][j]+= delta1[0][j];
                                }

                            }
                        }
                    }
                    if(inputBitboard_Black != 0) //Don't update the edge weights if they didn't contribute.
                    {
                        for(int square = 0; square < 64; square++)
                        {
                            if(inputBitboard_Black&(1ull<<square))
                            {
                                for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
                                {
                                    trainingNNUE->weights1[(64 * i) + square][j]+= delta1[1][j];
                                }

                            }
                        }
                    }

                }
                for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++) trainingNNUE->weights1_bias[j] += LEARNING_RATE * (delta1[0][j] + delta1[1][j]);
            }
        }

        totalIterations++;
        if(saveEveryNIterations && totalIterations%saveEveryNIterations == 0) save_trainingWeights();
        
        printf("\rIteration %d error = %e", totalIterations, sumSquaredError);

        if(sumSquaredError == prevSumSquaredError) sameErrorXTimesInARow++;
        else sameErrorXTimesInARow = 0;

        if(sameErrorXTimesInARow > 5) break;
        
        prevSumSquaredError = sumSquaredError;

    } while(sumSquaredError > maxAllowedError && totalIterations < maxIterations);

    save_trainingWeights();
    printf("\n");
}

void generateTrainingData(int depth, int maxTime, int maxPositions)
{
    FILE* output = fopen("./import/trainingData.bin", "ab+");

    fseek(output, 0, SEEK_END);
    int entryCount = ftell(output);
    entryCount/=sizeof(network_training_data);

    //Load scaling factor for dequantization.
    load_trainingWeights();
    float scalingFactor = trainingNNUE->scalingFactor;
    FREE(trainingNNUE);
    trainingNNUE = NULL;
    

    int error = 0;
    while(entryCount < maxPositions)
    {
        bitboard* board = create_board();
        load_playingWeights();
        loadInputAccumulator(board, PLAYER_NNUE);

        transpositionTable = create_hashTable_tt();
        while(1)
        {
            do
            {
                move* bestMove = calculateBestMove(board, depth, maxTime);
                
                //No one is in check and the best move isn't a capture.
                if(bestMove->capturedPiece == 0 && (board->flags&0x30) == 0)
                {
                    network_training_data newData = {0};
                    newData.board.pawn_w = board->pawn_w;
                    newData.board.pawn_b = board->pawn_b;
                    newData.board.knight_w = board->knight_w;
                    newData.board.knight_b = board->knight_b;
                    newData.board.bishop_w = board->bishop_w;
                    newData.board.bishop_b = board->bishop_b;
                    newData.board.rook_w = board->rook_w;
                    newData.board.rook_b = board->rook_b;
                    newData.board.queen_w = board->queen_w;
                    newData.board.queen_b = board->queen_b;
                    newData.board.king_w = board->king_w;
                    newData.board.king_b = board->king_b;

                    newData.board.pieces_all = board->pieces_all;
                    newData.board.pieces_w = board->pieces_w;
                    newData.board.pieces_b = board->pieces_b;
                    
                    newData.board.kingSquare_w = board->kingSquare_w;
                    newData.board.kingSquare_b = board->kingSquare_b;

                    newData.board.turn = board->turn;

                    newData.board.enPassantSquare = board->enPassantSquare;

                    newData.board.flags = board->flags;

                    newData.board.ht = NULL;
                    newData.board.moveStackTop = NULL;
                    newData.board.halfMoveCount = board->halfMoveCount;

                    newData.evaluation = (float) transposition_table_get(board, transpositionTable)->evaluation * scalingFactor;
                    fwrite(&newData, sizeof(network_training_data), 1, output);
                    entryCount++;
                    printf("\rTraining Data entries: %d", entryCount);
                }

                error = moveFromStruct(board, bestMove);
            }while(error != 0);
            
            if(board->victor || entryCount >= maxPositions) break;
        }
        destroy_hashTable_tt(transpositionTable);
        transpositionTable = NULL;
        destroy_board(board);
    }

    FREE(playerNNUE);
    fclose(output);
}

void updateTrainingData(int depth, int maxTime)
{
    FILE* input = fopen("./import/trainingData.bin", "rb+");
    
    fseek(input, 0, SEEK_END);
    int entryCount = ftell(input)/sizeof(network_training_data);
    rewind(input);
    
    network_training_data data = {0};

    for(int i = 0; i < entryCount; i++)
    {
        fread(&data, sizeof(network_training_data), 1, input);
        data.evaluation = principalVariationSearch(&data.board, -DBL_MAX, DBL_MAX, depth, depth, NULL, 0, NULL);
        fwrite(&data, sizeof(network_training_data), 1, input);
    }
    fclose(input);
}