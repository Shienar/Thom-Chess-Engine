#include "neuralnet.h"
#include "../debug.h"
#include "../board/bitboard.h"
#include "../board/moves.h"
#include "engine.h"
#include <math.h>
#include <float.h>
#include <immintrin.h>
#include <windows.h>

network_weights_training* trainingNNUE = NULL;

void iterateTrainingWeights(void (*func)(float*, float*), network_weights_training* trainingWeights, float* context) 
{
    assert(func && trainingWeights);

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
    assert(inputFloats);
    assert(outputBytes);

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
}

float SCReLU_Float(float val, float min, float max)
{
    if(val <= min) return min*min;
    if(val >= max) return max*max;
    return val*val;
}
float SCReLU_derivative(float val, float min, float max)
{
    return (val <= min || val >= max) ? (0.0) : (2*val);
}

//__m256 stored 8 32-bit floats (ps = packed single-precision)
void calculateLayer_Floats(float* inputValues, float* outputValues, int numInputs, int numOutputs, float weights[numInputs][numOutputs], float* biasWeights,  int applyCReLU)
{
    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex++)
    {
       if(biasWeights) outputValues[outputIndex] = biasWeights[outputIndex];
       else outputValues[outputIndex] = 0.0;

        __m256 output = _mm256_setzero_ps();

        for(int inputIndex = 0; inputIndex < numInputs; inputIndex+=8)
        {
            __m256 inputBatch = _mm256_loadu_ps(&inputValues[inputIndex]);

            //Weights are an array of float w[INPUT NODES][OUTPUTS NODES]
            //Passed as a single pointer to array head. inputIndex = row; outputIndex = column;
            __m256 weightsBatch = _mm256_loadu_ps(&weights[inputIndex][outputIndex]);

            //Multiply inputs by weights and add to intermediate.
            output = _mm256_fmadd_ps(inputBatch, weightsBatch, output);
        }

        //Horizontal addition
        __m128 output_128 = _mm_add_ps(_mm256_extractf128_ps(output, 1), _mm256_castps256_ps128(output));
        output_128 = _mm_add_ps(output_128, _mm_movehl_ps(output_128, output_128));
        output_128 = _mm_add_ss(output_128, _mm_shuffle_ps(output_128, output_128, _MM_SHUFFLE(0, 0, 0, 1)));
        outputValues[outputIndex] += _mm_cvtss_f32(output_128);

        if(applyCReLU) outputValues[outputIndex] = SCReLU_Float(outputValues[outputIndex], 0, 1);

    }
}

float forwardPropagate_Float(int turn, accumulator_training* floatAccumulator)
{
    //Assume accumulator has already been updated.

    //accumulator[0] = white
    //accumulator[1] = black
    //trainingNNUE->weights2[0 to ACCUMULATOR_NODES_PER_SIDE] = current side to move
    //trainingNNUE->weights2[ACCUMULATOR_NODES_PER_SIDE to 2* ACCUMULATOR_NODES_PER_SIDE -1] = opponent's side
    float tempH2[2][SECOND_HIDDEN_LAYER_NODES];
    if(ISWHITE(turn))
    {
        calculateLayer_Floats(floatAccumulator->accumulator[0], tempH2[0], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, trainingNNUE->weights2, trainingNNUE->weights2_bias, 0);
        calculateLayer_Floats(floatAccumulator->accumulator[1], tempH2[1], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, &trainingNNUE->weights2[ACCUMULATOR_NODES_PER_SIDE], NULL, 0);
    }
    else
    {
        calculateLayer_Floats(floatAccumulator->accumulator[0], tempH2[0], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, &trainingNNUE->weights2[ACCUMULATOR_NODES_PER_SIDE], trainingNNUE->weights2_bias, 0);
        calculateLayer_Floats(floatAccumulator->accumulator[1], tempH2[1], ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, trainingNNUE->weights2, NULL, 0);
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        //The above step added the biases twice.
        floatAccumulator->h2[i] = SCReLU_Float(tempH2[0][i] + tempH2[1][i] - trainingNNUE->weights2_bias[i], 0, 1);
    }

    calculateLayer_Floats(floatAccumulator->h2, floatAccumulator->h3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, trainingNNUE->weights3, trainingNNUE->weights3_bias, 1);
    calculateLayer_Floats(floatAccumulator->h3, &floatAccumulator->outputNode, THIRD_HIDDEN_LAYER_NODES, OUTPUT_LAYER_NODES, &trainingNNUE->weights4, &trainingNNUE->weights4_bias, 0);

    return floatAccumulator->outputNode;
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

typedef struct rpropThreadData {
    network_weights_training* curBatch_gradientSums;
    network_weights_training* prevBatch_gradientSums;
    long seekByteOffset;
    char* fileName;
    int entriesToTrack;
    double* sumSquaredError;
} rpropThreadData;

void loadTrainingData(char* inputLine, bitboard* board, float* eval)
{
    char* FEN_String = strtok(inputLine, "|");
    char* Eval_String = strtok(NULL, "|");

    sscanf(Eval_String, "%f\n", eval);
    load_fen_string_to_board(board, FEN_String);
}

DWORD WINAPI rpropThreadFunc(LPVOID lpParam)
{
    rpropThreadData* data = (rpropThreadData*)lpParam;
    
    char inputString[120] = {'\0'};

    FILE* trainingData = fopen(data->fileName, "r");

    fseek(trainingData, data->seekByteOffset, SEEK_SET);

    //File pointer is randomly located on a line. Skip it
    if(data->seekByteOffset > 0) fgets(inputString, 120, trainingData);
    
    int trackedEntries = 0;
    float expectedOutput = 0.0;
    bitboard* board = create_board();
    accumulator_training floatAccumulator = {0};
    while(trackedEntries < data->entriesToTrack && fgets(inputString, 120, trainingData))
    {
        loadTrainingData(inputString, board, &expectedOutput);

        loadInputAccumulator(board, &floatAccumulator, TRAINING, BLACK|WHITE);
        forwardPropagate_Float(board->turn, &floatAccumulator);

        float error = floatAccumulator.outputNode - expectedOutput;
        *data->sumSquaredError+= (double) error * error;

        //Calculate Edge Weight Deltas
        float delta4 = (expectedOutput - floatAccumulator.outputNode);

        float delta3[THIRD_HIDDEN_LAYER_NODES] = {0};
        for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) 
        {
            delta3[i] = delta4 * trainingNNUE->weights4[i] * SCReLU_derivative(floatAccumulator.h3[i], 0, 1);
        }
        
        float delta2[SECOND_HIDDEN_LAYER_NODES] = {0};
        for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
        {
            __m256 v_sum = _mm256_setzero_ps();
            for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j+= 8) 
            {
                //sum+= delta3[j] * trainingNNUE->weights3[i][j];
                __m256 v_delta3 = _mm256_loadu_ps(&delta3[j]);
                __m256 v_weights3 = _mm256_loadu_ps(&trainingNNUE->weights3[i][j]);
                v_sum = _mm256_fmadd_ps(v_delta3, v_weights3, v_sum);
                
            }

            //Horizontal add of sum.
            __m128 sum_128 = _mm_add_ps(_mm256_extractf128_ps(v_sum, 1), _mm256_castps256_ps128(v_sum));
            sum_128 = _mm_add_ps(sum_128, _mm_movehl_ps(sum_128, sum_128));
            sum_128 = _mm_add_ss(sum_128, _mm_shuffle_ps(sum_128, sum_128, _MM_SHUFFLE(0, 0, 0, 1)));
            
            delta2[i] = _mm_cvtss_f32(sum_128) * SCReLU_derivative(floatAccumulator.h2[i], 0, 1);
        }

        float delta1[2][ACCUMULATOR_NODES_PER_SIDE] = {0};
        for (int side = 0; side < 2; side++) 
        {
            for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=8) 
            {
                int offset = side * ACCUMULATOR_NODES_PER_SIDE;

                __m256 v_sum = _mm256_setzero_ps();
                for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j+= 8) 
                {
                    //sum += delta2[j] * trainingNNUE->weights2[offset + i][j];
                    __m256 v_delta2 = _mm256_loadu_ps(&delta2[j]);
                    __m256 v_weights2 = _mm256_loadu_ps(&trainingNNUE->weights2[offset + i][j]);
                    v_sum = _mm256_fmadd_ps(v_delta2, v_weights2, v_sum);
                    
                }

                //Horizontal add of sum.
                __m128 sum_128 = _mm_add_ps(_mm256_extractf128_ps(v_sum, 1), _mm256_castps256_ps128(v_sum));
                sum_128 = _mm_add_ps(sum_128, _mm_movehl_ps(sum_128, sum_128));
                sum_128 = _mm_add_ss(sum_128, _mm_shuffle_ps(sum_128, sum_128, _MM_SHUFFLE(0, 0, 0, 1)));
                
                delta1[side][i]= _mm_cvtss_f32(sum_128) * SCReLU_derivative(floatAccumulator.accumulator[side][i], 0, 1);
            }
        }

        //Accumulate edge weight deltas into the gradient sum
        __m256 v_deltaMult = _mm256_set1_ps(delta4);
        for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i+=8) 
        {
            //curBatch_gradientSums->weights4[i]+= delta4 * floatAccumulator->h3[i];
            __m256 v_weights = _mm256_loadu_ps(&data->curBatch_gradientSums->weights4[i]);
            __m256 v_h3 = _mm256_loadu_ps(&floatAccumulator.h3[i]);
            _mm256_storeu_ps(&data->curBatch_gradientSums->weights4[i], _mm256_fmadd_ps(v_deltaMult, v_h3, v_weights));

        }
        data->curBatch_gradientSums->weights4_bias+= delta4;

        for (int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
        {
            v_deltaMult = _mm256_set1_ps(floatAccumulator.h2[i]);
            for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j+=8) 
            {
                //curBatch_gradientSums->weights3[i][j]+= delta3[j] * floatAccumulator->h2[i];
                __m256 v_weights = _mm256_loadu_ps(&data->curBatch_gradientSums->weights3[i][j]);
                __m256 v_delta3 = _mm256_loadu_ps(&delta3[j]);
                _mm256_storeu_ps(&data->curBatch_gradientSums->weights3[i][j], _mm256_fmadd_ps(v_deltaMult, v_delta3, v_weights));
            }
        }

        for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j+=8) 
        {
            //curBatch_gradientSums->weights3_bias[j]+= delta3[j];
            __m256 v_weights = _mm256_loadu_ps(&data->curBatch_gradientSums->weights3_bias[j]);
            __m256 v_delta3 = _mm256_loadu_ps(&delta3[j]);
            _mm256_storeu_ps(&data->curBatch_gradientSums->weights3_bias[j], _mm256_add_ps(v_delta3, v_weights));
        }
        
        for (int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++) 
        {
            v_deltaMult = _mm256_set1_ps(floatAccumulator.accumulator[(int) i / ACCUMULATOR_NODES_PER_SIDE][i % ACCUMULATOR_NODES_PER_SIDE]);
            for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j+=8) 
            {
                //curBatch_gradientSums->weights2[i][j] += delta2[j] * floatAccumulator->accumulator[(int) (i / ACCUMULATOR_NODES_PER_SIDE)][i % ACCUMULATOR_NODES_PER_SIDE];
                __m256 v_weights = _mm256_loadu_ps(&data->curBatch_gradientSums->weights2[i][j]);
                __m256 v_delta2 = _mm256_loadu_ps(&delta2[j]);
                _mm256_storeu_ps(&data->curBatch_gradientSums->weights2[i][j], _mm256_fmadd_ps(v_deltaMult, v_delta2, v_weights));
            }
        }
        for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j+=8) 
        {
            //curBatch_gradientSums->weights2_bias[j] += delta2[j];
            __m256 v_weights = _mm256_loadu_ps(&data->curBatch_gradientSums->weights2_bias[j]);
            __m256 v_delta2 = _mm256_loadu_ps(&delta2[j]);
            _mm256_storeu_ps(&data->curBatch_gradientSums->weights2_bias[j], _mm256_add_ps(v_delta2, v_weights));
        }
        
        for (int i = 0; i < 640; i++) 
        {
            uint64_t inputBitboard_White = floatAccumulator.inputNodes[i];
            uint64_t inputBitboard_Black = floatAccumulator.inputNodes[640 + i];

            while (inputBitboard_White) {

                int square = __builtin_ctzll(inputBitboard_White);
                int idx = 64 * i + square;

                for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+= 8) 
                {
                    //curBatch_gradientSums->weights1[(64 * i) + square][j] += delta1[0][j];
                    __m256 v_weights = _mm256_loadu_ps(&data->curBatch_gradientSums->weights1[idx][j]);
                    __m256 v_delta1 = _mm256_loadu_ps(&delta1[0][j]);
                    _mm256_storeu_ps(&data->curBatch_gradientSums->weights1[idx][j], _mm256_add_ps(v_delta1, v_weights));
                }

                inputBitboard_White &= (inputBitboard_White - 1);
            }

            while (inputBitboard_Black) {
                int square = __builtin_ctzll(inputBitboard_Black);
                int idx = 64 * i + square;

                for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=8) 
                {
                    //curBatch_gradientSums->weights1[(64 * i) + square][j] += delta1[1][j];
                    __m256 v_weights = _mm256_loadu_ps(&data->curBatch_gradientSums->weights1[idx][j]);
                    __m256 v_delta1 = _mm256_loadu_ps(&delta1[1][j]);
                    _mm256_storeu_ps(&data->curBatch_gradientSums->weights1[idx][j], _mm256_add_ps(v_delta1, v_weights));
                }
                
                inputBitboard_Black &= (inputBitboard_Black - 1);
            }
        }
        for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=8) 
        {
            //curBatch_gradientSums->weights1_bias[j] += (delta1[0][j] + delta1[1][j]);
            __m256 v_weights = _mm256_loadu_ps(&data->curBatch_gradientSums->weights1_bias[j]);
            __m256 v_delta1_0 = _mm256_loadu_ps(&delta1[0][j]);
            __m256 v_delta1_1 = _mm256_loadu_ps(&delta1[1][j]);
            __m256 v_delta1 = _mm256_add_ps(v_delta1_0, v_delta1_1);
            _mm256_storeu_ps(&data->curBatch_gradientSums->weights2_bias[j], _mm256_add_ps(v_delta1, v_weights));
        }
    }

    destroy_board(board);
    fclose(trainingData);
    return 0;
}

void resilient_update(float* weight, float* update_value, float* current_grad, float* prev_grad)
{
    float change = (*current_grad * *prev_grad);
    if(change > 0) change = 1;
    else if(change < 0) change = -1;

    if (change > 0) 
    {
        // Gradient direction is the same: speed up
        *update_value = min(*update_value * RESILIENT_INCREASE_FACTOR, MIN_UPDATE_VALUE);
        *weight -= copysignf(1.0f, *current_grad) * *update_value;
        *prev_grad = *current_grad;
    } 
    else if (change < 0) {
        // Overstepped the minimum: slow down and backtrack
        *update_value = max(*update_value * RESILIENT_DECREASE_FACTOR, MAX_UPDATE_VALUE);
        *prev_grad = 0;       
    } 
    else {
        *weight -= copysignf(1.0f, *current_grad) * *update_value;
        *prev_grad = *current_grad;
    }
}

void resilient_updateAll(network_weights_training* weightUpdateValues, network_weights_training* curBatch_gradientSums, network_weights_training* prevBatch_gradientSums)
{
    //Update weights based on gradient sums.                  
    //weights4
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        resilient_update(&trainingNNUE->weights4[i], &weightUpdateValues->weights4[i], &curBatch_gradientSums->weights4[i], &prevBatch_gradientSums->weights4[i]);
    }
    resilient_update(&trainingNNUE->weights4_bias, &weightUpdateValues->weights4_bias, &curBatch_gradientSums->weights4_bias, &prevBatch_gradientSums->weights4_bias);
    
    //weights3
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            resilient_update(&trainingNNUE->weights3[i][j], &weightUpdateValues->weights3[i][j], &curBatch_gradientSums->weights3[i][j], &prevBatch_gradientSums->weights3[i][j]);
        }
    }
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        resilient_update(&trainingNNUE->weights3_bias[i], &weightUpdateValues->weights3_bias[i], &curBatch_gradientSums->weights3_bias[i], &prevBatch_gradientSums->weights3_bias[i]);
    }

    //weights2
    for(int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            resilient_update(&trainingNNUE->weights2[i][j], &weightUpdateValues->weights2[i][j], &curBatch_gradientSums->weights2[i][j], &prevBatch_gradientSums->weights2[i][j]);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        resilient_update(&trainingNNUE->weights2_bias[i], &weightUpdateValues->weights2_bias[i], &curBatch_gradientSums->weights2_bias[i], &prevBatch_gradientSums->weights2_bias[i]);
    }

    //weights1
    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            resilient_update(&trainingNNUE->weights1[i][j], &weightUpdateValues->weights1[i][j], &curBatch_gradientSums->weights1[i][j], &prevBatch_gradientSums->weights1[i][j]);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
    {
        resilient_update(&trainingNNUE->weights1_bias[i], &weightUpdateValues->weights1_bias[i], &curBatch_gradientSums->weights1_bias[i], &prevBatch_gradientSums->weights1_bias[i]);
    }
    memset(curBatch_gradientSums, 0.0, sizeof(network_weights_training));
}

void resilient_propagation(int saveEveryNBlocks, int maxIterations, float maxAllowedError, accumulator_training* floatAccumulator)
{
    assert(floatAccumulator);

    int* blockNumbers = CALLOC(FILE_COUNT, sizeof(int));
    for(int i = 0; i < FILE_COUNT; i++)
    {
        blockNumbers[i] = i;
    }

    int totalIterations = 0;

    
    HANDLE helperThreads[HELPER_THREAD_COUNT] = {0};
    DWORD helperThreadID[HELPER_THREAD_COUNT] = {0};
    rpropThreadData threadData[HELPER_THREAD_COUNT] = {0};

    network_weights_training* curBatch_gradientSums = CALLOC(HELPER_THREAD_COUNT + 1, sizeof(network_weights_training)); //Final index is the accumlated sum
    network_weights_training* prevBatch_gradientSums = CALLOC(HELPER_THREAD_COUNT + 1, sizeof(network_weights_training)); //Final index is the accumlated sum
    network_weights_training* weightUpdateValues = CALLOC(1, sizeof(network_weights_training));
    memset(weightUpdateValues, INITIAL_UPDATE_VALUE, sizeof(network_weights_training));

    double* sumSquaredErrors = CALLOC(HELPER_THREAD_COUNT, sizeof(double));
    double totalSumSquaredError = 0.0; //accumulated value.

    char fileName[40] = {'\0'};
    for(int i = 0; i < HELPER_THREAD_COUNT; i++)
    {
        threadData[i].curBatch_gradientSums = &curBatch_gradientSums[i];
        threadData[i].prevBatch_gradientSums = &prevBatch_gradientSums[i];
        threadData[i].entriesToTrack = (int) floor(POSITIONS_PER_FILE / HELPER_THREAD_COUNT);
        threadData[i].fileName = fileName;
        threadData[i].sumSquaredError = &sumSquaredErrors[i];
    }

    do{
        shuffle(blockNumbers, FILE_COUNT);

        for(int blockIndex = 0; blockIndex < FILE_COUNT; blockIndex++)
        {
            sprintf(fileName, "./training/trainingData_%d.txt", blockNumbers[blockIndex]);
            FILE* trainingData = fopen(fileName, "r");
            if(!trainingData)
            {
                DEBUG("\nFailed to open file: %s\n", fileName);
                continue;
            }
            fseek(trainingData, 0, SEEK_END);
            long fileBytes = ftell(trainingData);
            fclose(trainingData);
            for(int i = 0; i < HELPER_THREAD_COUNT; i++)
            {
                *threadData[i].sumSquaredError = 0;
                threadData[i].seekByteOffset = i * (long) floor(fileBytes / HELPER_THREAD_COUNT);
                helperThreads[i] = CreateThread(NULL, 0, rpropThreadFunc, &threadData[i], 0, &helperThreadID[i]);
            }

            for(int i = 0; i < HELPER_THREAD_COUNT; i++) 
            {
                WaitForSingleObject(helperThreads[i], INFINITE);
                CloseHandle(helperThreads[i]);
            }

            //Accumulate the values.
            memset(&curBatch_gradientSums[HELPER_THREAD_COUNT], 0.0, sizeof(network_weights_training));
            double sumSquaredError = 0.0;
            for(int i = 0; i < HELPER_THREAD_COUNT; i++)
            {
                curBatch_gradientSums[HELPER_THREAD_COUNT].weights4_bias +=  curBatch_gradientSums[i].weights4_bias;
                for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) curBatch_gradientSums[HELPER_THREAD_COUNT].weights4[j] +=  curBatch_gradientSums[i].weights4[j];
                for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) curBatch_gradientSums[HELPER_THREAD_COUNT].weights3_bias[j] +=  curBatch_gradientSums[i].weights3_bias[j];
                for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) for(int k = 0; k < THIRD_HIDDEN_LAYER_NODES; k++) curBatch_gradientSums[HELPER_THREAD_COUNT].weights3[j][k] +=  curBatch_gradientSums[i].weights3[j][k];
                for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) curBatch_gradientSums[HELPER_THREAD_COUNT].weights2_bias[j] +=  curBatch_gradientSums[i].weights2_bias[j];
                for(int j = 0; j < 2 * ACCUMULATOR_NODES_PER_SIDE; j++) for(int k = 0; k < SECOND_HIDDEN_LAYER_NODES; k++) curBatch_gradientSums[HELPER_THREAD_COUNT].weights2[j][k] +=  curBatch_gradientSums[i].weights2[j][k];
                for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++) curBatch_gradientSums[HELPER_THREAD_COUNT].weights1_bias[j] +=  curBatch_gradientSums[i].weights1_bias[j];
                for(int j = 0; j < HALF_INPUT_BITS; j++) for(int k = 0; k < ACCUMULATOR_NODES_PER_SIDE; k++) curBatch_gradientSums[HELPER_THREAD_COUNT].weights1[j][k] +=  curBatch_gradientSums[i].weights1[j][k];
                sumSquaredError+= sumSquaredErrors[i];
            }
            
            totalSumSquaredError+= sumSquaredError;
            printf("\r\tAnalyzed block %d/%d; mean squared error = %e", blockIndex, FILE_COUNT, sumSquaredError/POSITIONS_PER_FILE);
            resilient_updateAll(weightUpdateValues, &curBatch_gradientSums[HELPER_THREAD_COUNT], &prevBatch_gradientSums[HELPER_THREAD_COUNT]);
            
            if(saveEveryNBlocks && blockIndex > 0 && blockIndex%saveEveryNBlocks == 0) save_trainingWeights();
        }
        totalIterations++;
        
        printf("\rIteration %d error = %e\n", totalIterations, totalSumSquaredError/(POSITIONS_PER_FILE * FILE_COUNT));


        //Mean squared error.
    } while((totalSumSquaredError/(POSITIONS_PER_FILE * FILE_COUNT)) > maxAllowedError && totalIterations < maxIterations);

    FREE(blockNumbers);
    save_trainingWeights();
    printf("\n");
}

void generateTrainingData(int depth, int maxTime, accumulator_training* floatAccumulator)
{
    int fileNumber = 1;
    char fileName[40] = {'\0'};
    FILE* output;
    while(1)
    {
        sprintf(fileName, "./training/trainingData_%d.txt", fileNumber);
        output = fopen(fileName, "w");
        if(!output) break;
        else
        {
            fclose(output);
            fileNumber++;
        }

    }

    int entryCount = 0;

    while(entryCount < POSITIONS_PER_FILE)
    {
        bitboard* board = create_board();
        loadInputAccumulator(board, floatAccumulator, TRAINING, BLACK|WHITE);

        transpositionTable = create_hashTable_tt();

        //Avoid boring games.
        int movesSinceLastInterestingMove = 0;

        while(1)
        {
            move* bestMove = calculateBestMove(board, depth, maxTime);
            
            //No one is in check and the best move isn't a capture.
            if(!bestMove->capturedPiece && !(board->flags&0x30) && transposition_table_get(board, transpositionTable))
            {
                movesSinceLastInterestingMove = 0;

                char FEN[100] = {'\0'};
                export_fen_from_board(board, FEN);
                float evaluation = (float) transposition_table_get(board, transpositionTable)->evaluation;
                fprintf(output, "%s|%f\n", FEN, evaluation);

            }
            else movesSinceLastInterestingMove++;

            if(board->victor || movesSinceLastInterestingMove > 10 || entryCount >= POSITIONS_PER_FILE) break;
            if(moveFromStruct(board, bestMove)) break;
        }
        destroy_hashTable_tt(transpositionTable);
        transpositionTable = NULL;
        destroy_board(board);
    }

    FREE(trainingNNUE);
    fclose(output);
}