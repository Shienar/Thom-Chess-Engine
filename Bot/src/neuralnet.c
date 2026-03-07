#include "../include/neuralnet.h"
#include "../include/debug.h"
#include "../include/bitboard.h"
#include <math.h>
#include <float.h>
#include <immintrin.h>

network_weights_training* trainingNNUE = NULL;
network_weights_playing* playerNNUE = NULL;

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

        float standardDeviation = sqrt(2/HALF_INPUT_BITS);
        iterateTrainingWeights(sampleNormalDistribution, trainingNNUE, &standardDeviation);
        memset(&trainingNNUE->inputNodes, 0, sizeof(trainingNNUE->inputNodes));
        memset(&trainingNNUE->accumulator, 0,  sizeof(trainingNNUE->accumulator));
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

void load_playingWeights()
{
    if(!playerNNUE) playerNNUE = CALLOC(1, sizeof(network_weights_training));

    FILE* input = fopen("import/NNUE_Player.bin", "rb");
    if(input)
    {
        fread(playerNNUE, sizeof(network_weights_training), 1, input);
        fclose(input);
    }
    else
    {
        load_trainingWeights();
        quantizeWeights(trainingNNUE, playerNNUE);
        memset(&playerNNUE->inputNodes, 0, sizeof(playerNNUE->inputNodes));
        memset(&playerNNUE->accumulator, 0,  sizeof(playerNNUE->accumulator));

        FREE(trainingNNUE);
        trainingNNUE = NULL;
    }
}

void save_playingWeights()
{
    FILE* output = fopen("import/NNUE_Player.bin", "wb");
    if(output)
    {
        fwrite(playerNNUE, sizeof(network_weights_playing), 1, output);
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

void quantizeWeights(network_weights_training* inputFloats, network_weights_playing* outputBytes)
{
    if(!inputFloats || !outputBytes) 
    {
        DEBUG("Cannot quantize with null pointers.")
        return;
    }

    float maxValue = -FLT_MAX;
    iterateTrainingWeights(findAbsMax, inputFloats, &maxValue);
    
    float scalingFactor = maxValue / 127;
    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            outputBytes->weights1[i][j] = (uint8_t) roundf(inputFloats->weights1[i][j] / scalingFactor);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) outputBytes->weights1_bias[i] = (uint8_t) roundf(inputFloats->weights1_bias[i] / scalingFactor);

    for(int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights2[i][j] = (uint8_t) roundf(inputFloats->weights2[i][j] / scalingFactor);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) outputBytes->weights2_bias[i] = (uint8_t) roundf(inputFloats->weights2_bias[i] / scalingFactor);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights3[i][j] = (uint8_t) roundf(inputFloats->weights3[i][j] / scalingFactor);
        }
    }
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) outputBytes->weights3_bias[i] = (uint8_t) roundf(inputFloats->weights3_bias[i] / scalingFactor);
    
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        outputBytes->weights4[i] = (uint8_t) roundf(inputFloats->weights4[i] / scalingFactor);
    }
    outputBytes->weights4_bias = (uint8_t) roundf(inputFloats->weights4_bias / scalingFactor);
}

float SCReLU_Float(float val, float min, float max)
{
    if(val <= min) return min*min;
    if(val >= max) return max*max;
    return val*val;
}
int8_t SCReLU_Int(int8_t val, int8_t min, int8_t max)
{
    if(val <= min) return min*min;
    if(val >= max) return max*max;
    return val*val;
}

//__m256 stored 8 32-bit floats (ps = packed single-precision)
void calculateLayer_Floats(float* inputValues, float* outputValues, int numInputs, int numOutputs, float** weights, float* biasWeights,  int applyCReLU)
{
    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex++)
    {
        outputValues[outputIndex] = biasWeights[outputIndex];
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

//__m256i stores 32 8-bit ints (epi8 = extended packed 8-bit integer (signed))
void calculateLayer_IntBytes(uint8_t* inputValues, uint8_t* outputValues, int numInputs, int numOutputs, uint8_t** weights, uint8_t* biasWeights,  int applyCReLU)
{
    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex++)
    {
        __m256i intermediate1 = _mm256_setzero_si256(); //si256 = 256-bit signed integer.
        __m256i intermediate2 = _mm256_setzero_si256();
        __m256i intermediate3 = _mm256_setzero_si256();
        __m256i intermediate4 = _mm256_setzero_si256();

        //All layer lengths are divisible by 32 so no overflow.
        for(int inputIndex = 0; inputIndex < numInputs; inputIndex+=32)
        {
            __m256i inputBatch1 = _mm256_loadu_epi8(&inputValues[inputIndex]);
            __m256i inputBatch2 = _mm256_loadu_epi8(&inputValues[inputIndex + 8]);
            __m256i inputBatch3 = _mm256_loadu_epi8(&inputValues[inputIndex + 16]);
            __m256i inputBatch4 = _mm256_loadu_epi8(&inputValues[inputIndex + 24]);

            //Weights are an array of float w[INPUT NODES][OUTPUTS NODES]
            __m256i weightsBatch1 = _mm256_loadu_epi8(&weights[inputIndex][outputIndex]);
            __m256i weightsBatch2 = _mm256_loadu_epi8(&weights[inputIndex + 8][outputIndex]);
            __m256i weightsBatch3 = _mm256_loadu_epi8(&weights[inputIndex + 16][outputIndex]);
            __m256i weightsBatch4 = _mm256_loadu_epi8(&weights[inputIndex + 24][outputIndex]);

            //Multiply inputs by weights.
            //Outputs 16-bit results. Neighboring values get summed.
            __m256i tempProduct1 = _mm256_maddubs_epi16 (inputBatch1, weightsBatch1);
            __m256i tempProduct2 = _mm256_maddubs_epi16 (inputBatch2, weightsBatch2);
            __m256i tempProduct3 = _mm256_maddubs_epi16 (inputBatch3, weightsBatch3);
            __m256i tempProduct4 = _mm256_maddubs_epi16 (inputBatch4, weightsBatch4);
            
            //Add temp products to intermediate registers.
            intermediate1 = _mm256_add_epi16(intermediate1, tempProduct1);
            intermediate2 = _mm256_add_epi16(intermediate2, tempProduct2);
            intermediate3 = _mm256_add_epi16(intermediate3, tempProduct3);
            intermediate4 = _mm256_add_epi16(intermediate4, tempProduct4);
        }

        //Add the four registers together. Sum stored in intermediate1.
        intermediate1 = _mm256_add_epi16(intermediate1, intermediate2);
        intermediate3 = _mm256_add_epi16(intermediate3, intermediate4);
        intermediate1 = _mm256_add_epi16(intermediate1, intermediate3);

        /**
         * Simplify the 256-bit register into an 8-bit sum.
         * It currently holds 16 16-bit values.
         * intermediate1 = [s15, s14, s13, s12, s11, s10, s9, s8,| s7, s6, s5, s4, s3, s2, s1, s0]
         * 
         * Instruction 1: Sum the upper and lower halves to create a merged 128 bit register of 8 16-bit values.
         *      sum128 = [(s15 +s7), (s14 + s6), (s13 + s5), (s12 + s4), | (s11 +s3), (s10 + s2), (s9 + s1), (s8 + s0)]
         *      - Concisely described as [s7, s6, s5, s4, | s3, s2, s1, s0] below
         * Instruction 2: Add it to itself, shifted 8 bytes to the right
         *      - Operand 1: [s7, s6, s5, s4, | s3, s2, s1, s0]
         *      - Operand 2: [0, 0, 0, 0, | s7, s6, s5, s4]
         *      - Sum: [s7, s6, s5, s4, | (s3 + s7), (s2 + s6), (s1 + s5), (s0 + s4)]
         * Instruction 3: Add it to itself again, shifted 4 bytes to the right
         *      - Operand 1: [s7, s6, s5, s4, | (s3 + s7), (s2 + s6), (s1 + s5), (s0 + s4)]
         *      - Operand 2: [0, 0, s7, s6, | s5, s4, (s3 + s7), (s2 + s6)]
         *      - Sum: [s7, s6, (s5 + s7), (s4 + s6), | (s3 + s4 + s7), (s2 + s4 + s6), (s1 + s3 + s5 + s7), (s0 + s2 + s4 + s6)]
         * Instruction 4: Add it to itself again, shifted 2 bytes to the right
         *      - Operand 1: [s7, s6, (s5 + s7), (s4 + s6), | (s3 + s4 + s7), (s2 + s4 + s6), (s1 + s3 + s5 + s7), (s0 + s2 + s4 + s6)]
         *      - Operand 2: [0, s7, s6, (s5 + s7), | (s4 + s6), (s3 + s4 + s7), (s2 + s4 + s6), (s1 + s3 + s5 + s7)]
         *      - Sum: [s7, (s6 + s7), (s5 + s6 + s7), (s4 + s5 + s6 + s7), | (s3 + s4 + s4 + s6 s7), (s2 + s3 + s4 + s4 + s6 + s7), (s1 + s2 + s3 + s4 + s5 + s6 + s7), (s0 + s1 + s2 + s3 + s4 + s5 + s6 + s7)]
         * Instruction 5: Extract the rightmost value (total sum)
         */
        __m128i sum128 = _mm_add_epi16(_mm256_castsi256_si128(intermediate1), _mm256_extracti128_si256(intermediate1, 1));
        sum128 = _mm_add_epi16(sum128, _mm_srli_si128(sum128, 8));
        sum128 = _mm_add_epi16(sum128, _mm_srli_si128(sum128, 4));
        sum128 = _mm_add_epi16(sum128, _mm_srli_si128(sum128, 2));
        int16_t totalSum = _mm_extract_epi16(sum128, 0);
        totalSum+= biasWeights[outputIndex];

        if(totalSum >= INT8_MAX) totalSum = INT8_MAX;
        else if(totalSum <= INT8_MIN) totalSum = INT8_MIN;
        
        outputValues[outputIndex] = totalSum;

        if(applyCReLU) outputValues[outputIndex] = SCReLU_Int(outputValues[outputIndex], 0, 1);
    }
}

void extractInputLayerToArray(uint64_t* inputLayerCompact, float* inputLayerFloats, int8_t* inputLayerBytes)
{
    if(!inputLayerCompact)
    {
        DEBUG("Cannot extract null input layer.")
        return;
    }
    else if(!inputLayerFloats)
    {
        for(int i = 0; i < INPUT_BITS; i++)
        {
            if((inputLayerCompact[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerFloats[i] = 1.0;
            else inputLayerFloats[0] = 0.0;
        }
    }
    else if(!inputLayerBytes)
    {
        for(int i = 0; i < INPUT_BITS; i++)
        {
            if((inputLayerCompact[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerBytes[i] = 1;
            else inputLayerBytes[0] = 0;
        }
    }
    else
    {
        DEBUG("No provided destination for extraction.")
    }
}

void loadInputAccumulator(bitboard* board, int networkType)
{
    if(!board)
    {
        DEBUG("Cannot load null board into accumulator.")
        return;
    }
    else if(networkType == TRAINING_NNUE && !trainingNNUE)
    {
        DEBUG("Cannot load null training weights.")
        return;
    }
    else if(networkType == PLAYER_NNUE && !playerNNUE)
    {
        DEBUG("Cannot load null playing weights.")
        return;
    }

    
    uint64_t* inputs = NULL;
    if(networkType == TRAINING_NNUE) inputs = &trainingNNUE->inputNodes;
    else inputs = &playerNNUE->inputNodes;
    memset(inputs, 0, 1280*sizeof(uint64_t));

    int baseIndex_w = 10*board->kingSquare_w;
    int baseIndex_b = 640 + 10*board->kingSquare_b;

    inputs[baseIndex_w + 0] = board->pawn_w;
    inputs[baseIndex_w + 1] = board->knight_w;
    inputs[baseIndex_w + 2] = board->bishop_w;
    inputs[baseIndex_w + 3] = board->rook_w;
    inputs[baseIndex_w + 4] = board->queen_w;
    inputs[baseIndex_w + 5] = board->pawn_b;
    inputs[baseIndex_w + 6] = board->knight_b;
    inputs[baseIndex_w + 7] = board->bishop_b;
    inputs[baseIndex_w + 8] = board->rook_b;
    inputs[baseIndex_w + 9] = board->queen_b;

    inputs[baseIndex_b + 0] = board->pawn_b;
    inputs[baseIndex_b + 1] = board->knight_b;
    inputs[baseIndex_b + 2] = board->bishop_b;
    inputs[baseIndex_b + 3] = board->rook_b;
    inputs[baseIndex_b + 4] = board->queen_b;
    inputs[baseIndex_b + 5] = board->pawn_w;
    inputs[baseIndex_b + 6] = board->knight_w;
    inputs[baseIndex_b + 7] = board->bishop_w;
    inputs[baseIndex_b + 8] = board->rook_w;
    inputs[baseIndex_b + 9] = board->queen_w;

    if(networkType == TRAINING_NNUE)
    {
        float inputArray[81920];
        extractInputLayerToArray(inputs, inputArray, NULL);

        calculateLayer_Floats(inputArray, trainingNNUE->accumulator[0], HALF_INPUT_BITS, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights1, trainingNNUE->weights1_bias, 0);
        calculateLayer_Floats(&inputArray[40960], trainingNNUE->accumulator[1], HALF_INPUT_BITS, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights1, trainingNNUE->weights1_bias, 0);
    }
    else
    {
        int8_t inputArray[81920];
        extractInputLayerToArray(inputs, NULL, inputArray);

        calculateLayer_IntBytes(inputArray, trainingNNUE->accumulator[0], HALF_INPUT_BITS, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights1, trainingNNUE->weights1_bias, 0);
        calculateLayer_IntBytes(&inputArray[40960], trainingNNUE->accumulator[1], HALF_INPUT_BITS, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights1, trainingNNUE->weights1_bias, 0);
    }
}

void updateMoveAccumulator(bitboard* board, move* lastMove, int networkType, int shouldUndoMove)
{
    if(!lastMove)
    {
        DEBUG("Cannot load null move.")
        return;
    }

    if(ISKING(lastMove->piece))
    {
        loadInputAccumulator(board, networkType);
    }
    else
    {
        //Update input nodes.
        uint64_t xorMask = (1ull<<lastMove->startSquare)|(1ull<<lastMove->endSquare);

        int pieceOffset = lastMove->piece;
        int kingSquare = 0;
        if(ISBLACK(pieceOffset)) 
        {
            pieceOffset = (lastMove->piece&0xF) + 4;
            kingSquare = board->kingSquare_b;
        }
        else 
        {
            pieceOffset = (lastMove->piece&0xF) - 1;
            kingSquare = board->kingSquare_w;
        }

        int inputNodeIndex = (640 * ISBLACK(lastMove->piece)) + (10 * kingSquare) + pieceOffset;

        if(networkType == TRAINING_NNUE) trainingNNUE->inputNodes[inputNodeIndex]^=xorMask;
        else playerNNUE->inputNodes[inputNodeIndex]^=xorMask;

        //Doesn't care about 40,960 offset for black inputs.
        int baseIndex = (640 * kingSquare) + (64 * pieceOffset);
        int fromSquareIndex, toSquareIndex;
        if(shouldUndoMove)
        {
            fromSquareIndex = lastMove->endSquare;
            toSquareIndex = lastMove->startSquare;
        }
        else
        {
            fromSquareIndex = lastMove->startSquare;
            toSquareIndex = lastMove->endSquare;
        }

        if(networkType == TRAINING_NNUE)
        {
            float* accumulator;
            if(ISBLACK(lastMove->piece)) accumulator = trainingNNUE->accumulator[1];
            else accumulator = trainingNNUE->accumulator[0];

            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
            {
                accumulator[i] = accumulator[i] + trainingNNUE->weights1[toSquareIndex][i] -  trainingNNUE->weights1[fromSquareIndex][i];
            }
        }
        else
        {
            float* accumulator;
            if(ISBLACK(lastMove->piece)) accumulator = playerNNUE->accumulator[1];
            else accumulator = playerNNUE->accumulator[0];

            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
            {
                accumulator[i] = accumulator[i] + playerNNUE->weights1[toSquareIndex][i] -  playerNNUE->weights1[fromSquareIndex][i];
            }
        }
    }
}


float forwardPropagate_Float()
{
    //Assume accumulator has already been updated.
    float h2[SECOND_HIDDEN_LAYER_NODES] = {0.0};
    float h3[THIRD_HIDDEN_LAYER_NODES] = {0.0};
    float outputNode = 0.0;

    calculateLayer_Floats(trainingNNUE->accumulator, h2, 2*ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, trainingNNUE->weights2, trainingNNUE->weights2_bias, 1);
    calculateLayer_Floats(h2, h3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, trainingNNUE->weights3, trainingNNUE->weights3_bias, 1);
    calculateLayer_Floats(h3, &outputNode, THIRD_HIDDEN_LAYER_NODES, OUTPUT_LAYER_NODES, trainingNNUE->weights4, &trainingNNUE->weights4_bias, 1);

    return outputNode;
}
float forwardPropagate_Int()
{
    //Assume accumulator has already been updated.
    int8_t h2[SECOND_HIDDEN_LAYER_NODES] = {0};
    int8_t h3[THIRD_HIDDEN_LAYER_NODES] = {0};
    int8_t outputNode = 0;

    calculateLayer_IntBytes(playerNNUE->accumulator, h2, 2*ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, playerNNUE->weights2, playerNNUE->weights2_bias, 1);
    calculateLayer_IntBytes(h2, h3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, playerNNUE->weights3, playerNNUE->weights3_bias, 1);
    calculateLayer_IntBytes(h3, &outputNode, THIRD_HIDDEN_LAYER_NODES, OUTPUT_LAYER_NODES, playerNNUE->weights4, &playerNNUE->weights4_bias, 1);

    return outputNode;
}