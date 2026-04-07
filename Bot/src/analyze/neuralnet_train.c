#include "neuralnet.h"
#include "../debug.h"
#include "../board/bitboard.h"
#include "../board/moves.h"
#include "engine.h"
#include <float.h>
#include <immintrin.h>
#include <windows.h>

network_weights_training* trainingNNUE = NULL;

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

    for(int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++)
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
void sampleNormalDistribution(float* dest, double* standardDeviation) 
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
        double standardDeviation = sqrt(2.0/32.0);
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
                            2 * ACCUMULATOR_NODES_PER_SIDE * SECOND_HIDDEN_LAYER_NODES +
                            SECOND_HIDDEN_LAYER_NODES +
                            SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES +
                            2 * THIRD_HIDDEN_LAYER_NODES + 1);
    
    inputFloats->scalingFactor = maxValue / 127;
    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            outputBytes->weights1[j][i] = (uint8_t) roundf(inputFloats->weights1[j][i] / inputFloats->scalingFactor);
        }
    }
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) outputBytes->weights1_bias[i] = (uint8_t) roundf(inputFloats->weights1_bias[i] / inputFloats->scalingFactor);

    for(int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights2[j][i] = (uint8_t) roundf(inputFloats->weights2[j][i] / inputFloats->scalingFactor);
        }
    }
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) outputBytes->weights2_bias[i] = (uint8_t) roundf(inputFloats->weights2_bias[i] / inputFloats->scalingFactor);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            outputBytes->weights3[j][i] = (uint8_t) roundf(inputFloats->weights3[j][i] / inputFloats->scalingFactor);
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

//__m256 stored 8 32-bit floats (ps = packed single-precision)
void calculateLayer_Floats(float* inputValues, float* outputValues, int numInputs, int numOutputs, float weights[numOutputs][numInputs], int inputOffset, float* biasWeights,  int applyCReLU)
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
            __m256 weightsBatch = _mm256_loadu_ps(&weights[outputIndex][inputIndex + inputOffset]);

            //Multiply inputs by weights and add to intermediate.
            output = _mm256_fmadd_ps(inputBatch, weightsBatch, output);
        }

        //Horizontal addition
        __m128 output_128 = _mm_add_ps(_mm256_extractf128_ps(output, 1), _mm256_castps256_ps128(output));
        output_128 = _mm_add_ps(output_128, _mm_movehl_ps(output_128, output_128));
        output_128 = _mm_add_ss(output_128, _mm_shuffle_ps(output_128, output_128, _MM_SHUFFLE(0, 0, 0, 1)));
        outputValues[outputIndex] += _mm_cvtss_f32(output_128);

        if(applyCReLU) outputValues[outputIndex] = SCReLU(outputValues[outputIndex], 0, 1);

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
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        floatAccumulator->rawH2[i] = tempH2[0][i] + tempH2[1][i] - trainingNNUE->weights2_bias[i];
        floatAccumulator->h2[i] = SCReLU(floatAccumulator->rawH2[i], 0, 1);
    }

    calculateLayer_Floats(floatAccumulator->h2, floatAccumulator->rawH3, SECOND_HIDDEN_LAYER_NODES, THIRD_HIDDEN_LAYER_NODES, trainingNNUE->weights3, 0, trainingNNUE->weights3_bias, 0);
    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) floatAccumulator->h3[i] = SCReLU(floatAccumulator->rawH3[i], 0, 1);

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

typedef struct rpropThreadData {
    network_weights_training* gradientSums;
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

DWORD WINAPI calculateGradients(LPVOID lpParam)
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
        forwardPropagate_Float(board->turn, &floatAccumulator, 0);

        //if((double)rand()/RAND_MAX < 0.0002) printf("Expected %f, received %f\n", expectedOutput, floatAccumulator.outputNode);
        float delta4 = floatAccumulator.outputNode - expectedOutput;

        *data->sumSquaredError+= (double) delta4 * delta4;

        //Calculate Edge Weight Deltas

        float delta3[THIRD_HIDDEN_LAYER_NODES] = {0};
        for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++) 
        {
            delta3[i] = delta4 * trainingNNUE->weights4[i] * SCReLU_Derivative(floatAccumulator.rawH3[i], 0, 1);
        }
        
        float delta2[SECOND_HIDDEN_LAYER_NODES] = {0};
        for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
        {
            for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) 
            {
                delta2[i] += delta3[j] * trainingNNUE->weights3[j][i];
            }
            
            delta2[i] *= SCReLU_Derivative(floatAccumulator.rawH2[i], 0, 1);
        }

        float delta1[2][ACCUMULATOR_NODES_PER_SIDE] = {0};
        for (int side = 0; side < 2; side++) 
        {
            for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++) 
            {
                for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) 
                {
                    delta1[side][i] += delta2[j] * trainingNNUE->weights2[j][side * ACCUMULATOR_NODES_PER_SIDE + i];
                }
                
                delta1[side][i] *= SCReLU_Derivative(floatAccumulator.rawAccumulator[side][i], 0, 1);
            }
        }

        //Accumulate edge weight deltas into the gradient sum
        __m256 v_deltaMult = _mm256_set1_ps(delta4);
        for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i+=8) 
        {
            //gradientSums->weights4[i]+= delta4 * floatAccumulator->h3[i];
            __m256 v_weights = _mm256_loadu_ps(&data->gradientSums->weights4[i]);
            __m256 v_h3 = _mm256_loadu_ps(&floatAccumulator.h3[i]);
            _mm256_storeu_ps(&data->gradientSums->weights4[i], _mm256_fmadd_ps(v_deltaMult, v_h3, v_weights));

        }
        data->gradientSums->weights4_bias+= delta4;

        for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) 
        {
            v_deltaMult = _mm256_set1_ps(delta3[j]);
            for (int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i+=8)
            {
                //gradientSums->weights3[j][i]+= delta3[j] * floatAccumulator->h2[i];
                __m256 v_weights = _mm256_loadu_ps(&data->gradientSums->weights3[j][i]);
                __m256 v_h2 = _mm256_loadu_ps(&floatAccumulator.h2[i]);
                _mm256_storeu_ps(&data->gradientSums->weights3[j][i], _mm256_fmadd_ps(v_deltaMult, v_h2, v_weights));
            }
        }

        for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j+=8) 
        {
            //gradientSums->weights3_bias[j]+= delta3[j];
            __m256 v_weights = _mm256_loadu_ps(&data->gradientSums->weights3_bias[j]);
            __m256 v_delta3 = _mm256_loadu_ps(&delta3[j]);
            _mm256_storeu_ps(&data->gradientSums->weights3_bias[j], _mm256_add_ps(v_delta3, v_weights));
        }
        
        for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) 
        {
            v_deltaMult = _mm256_set1_ps(delta2[j]);
            for (int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i+=8) 
            {
                //gradientSums->weights2[i][j] += delta2[j] * floatAccumulator->accumulator[(int) (i / ACCUMULATOR_NODES_PER_SIDE)][i % ACCUMULATOR_NODES_PER_SIDE];
                __m256 v_weights = _mm256_loadu_ps(&data->gradientSums->weights2[j][i]);
                __m256 v_acc = _mm256_loadu_ps(&floatAccumulator.accumulator[(int) i / ACCUMULATOR_NODES_PER_SIDE][i % ACCUMULATOR_NODES_PER_SIDE]);
                _mm256_storeu_ps(&data->gradientSums->weights2[j][i], _mm256_fmadd_ps(v_deltaMult, v_acc, v_weights));
            }
        }
        for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j+=8) 
        {
            //gradientSums->weights2_bias[j] += delta2[j];
            __m256 v_weights = _mm256_loadu_ps(&data->gradientSums->weights2_bias[j]);
            __m256 v_delta2 = _mm256_loadu_ps(&delta2[j]);
            _mm256_storeu_ps(&data->gradientSums->weights2_bias[j], _mm256_add_ps(v_delta2, v_weights));
        }
        
        for (int i = 0; i < 640; i++) 
        {
            uint64_t inputBitboard_White = floatAccumulator.inputNodes[i];
            uint64_t inputBitboard_Black = floatAccumulator.inputNodes[640 + i];

            while (inputBitboard_White) {

                int square = __builtin_ctzll(inputBitboard_White);
                int idx = 64 * i + square;

                for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++) 
                {
                    data->gradientSums->weights1[j][idx] += delta1[0][j];
                }

                inputBitboard_White &= (inputBitboard_White - 1);
            }

            while (inputBitboard_Black) {
                int square = __builtin_ctzll(inputBitboard_Black);
                int idx = 64 * i + square;

                for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++) 
                {
                    //gradientSums->weights1[(64 * i) + square][j] += delta1[1][j];
                    data->gradientSums->weights1[j][idx] += delta1[1][j];
                }
                
                inputBitboard_Black &= (inputBitboard_Black - 1);
            }
        }
        for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=8) 
        {
            //gradientSums->weights1_bias[j] += (delta1[0][j] + delta1[1][j]);
            __m256 v_weights = _mm256_loadu_ps(&data->gradientSums->weights1_bias[j]);
            __m256 v_delta1_0 = _mm256_loadu_ps(&delta1[0][j]);
            __m256 v_delta1_1 = _mm256_loadu_ps(&delta1[1][j]);
            __m256 v_delta1 = _mm256_add_ps(v_delta1_0, v_delta1_1);
            _mm256_storeu_ps(&data->gradientSums->weights1_bias[j], _mm256_add_ps(v_delta1, v_weights));
        }

        trackedEntries++;
    }

    destroy_board(board);
    fclose(trainingData);
    return 0;
}

long timestamp = 1;
float biasCorrection1 = 0.0;
float biasCorrection2 = 0.0;
void updateWeight(float* weight, float* gradient, float* firstMoment, float* secondMoment)
{
    *firstMoment = ADAM_BETA1 * *firstMoment + (1.0 - ADAM_BETA1) * *gradient; 
    *secondMoment = ADAM_BETA2 * *secondMoment + (1.0 - ADAM_BETA2) * (*gradient * *gradient); 

    float corrected_firstMoment = *firstMoment / (1.0 - biasCorrection1);
    float corrected_secondMoment = *secondMoment / (1.0 - biasCorrection2);

    *weight -= ADAM_LEARNING_RATE * (corrected_firstMoment / (sqrt(corrected_secondMoment) + ADAM_EPSILON));

    *gradient = 0;
}

typedef struct {
    network_weights_training *gradientSums;
    network_weights_training *firstMoment;
    network_weights_training *secondMoment;
} updateContext;

VOID CALLBACK UpdateWeights4(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work) {
    updateContext* c = (updateContext*)Context;

    network_weights_training *gradientSums = c->gradientSums;
    network_weights_training *firstMoment = c->firstMoment;
    network_weights_training *secondMoment = c->secondMoment;

    for(int i = 0; i < HELPER_THREAD_COUNT; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) gradientSums[HELPER_THREAD_COUNT].weights4[j] +=  gradientSums[i].weights4[j];
    }

    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        gradientSums[HELPER_THREAD_COUNT].weights4[i] /= POSITIONS_PER_FILE;
        updateWeight(&trainingNNUE->weights4[i], &gradientSums[HELPER_THREAD_COUNT].weights4[i], &firstMoment->weights4[i], &secondMoment->weights4[i]);
    }

}

VOID CALLBACK UpdateWeights3(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work) {
    updateContext* c = (updateContext*)Context;

    network_weights_training *gradientSums = c->gradientSums;
    network_weights_training *firstMoment = c->firstMoment;
    network_weights_training *secondMoment = c->secondMoment;

    for(int i = 0; i < HELPER_THREAD_COUNT; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) for(int k = 0; k < THIRD_HIDDEN_LAYER_NODES; k++) gradientSums[HELPER_THREAD_COUNT].weights3[k][j] +=  gradientSums[i].weights3[k][j];
    }
    
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            gradientSums[HELPER_THREAD_COUNT].weights3[j][i] /= POSITIONS_PER_FILE;
            updateWeight(&trainingNNUE->weights3[j][i], &gradientSums[HELPER_THREAD_COUNT].weights3[j][i], &firstMoment->weights3[j][i], &secondMoment->weights3[j][i]);
        }
    }
}

VOID CALLBACK UpdateWeights3_bias(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work) {
    updateContext* c = (updateContext*)Context;

    network_weights_training *gradientSums = c->gradientSums;
    network_weights_training *firstMoment = c->firstMoment;
    network_weights_training *secondMoment = c->secondMoment;
    
    for(int i = 0; i < HELPER_THREAD_COUNT; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) gradientSums[HELPER_THREAD_COUNT].weights3_bias[j] +=  gradientSums[i].weights3_bias[j];
    }

    for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
    {
        gradientSums[HELPER_THREAD_COUNT].weights3_bias[i] /= POSITIONS_PER_FILE;
        updateWeight(&trainingNNUE->weights3_bias[i], &gradientSums[HELPER_THREAD_COUNT].weights3_bias[i], &firstMoment->weights3_bias[i], &secondMoment->weights3_bias[i]);
    }
}

VOID CALLBACK UpdateWeights2(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work) {
    updateContext* c = (updateContext*)Context;

    network_weights_training *gradientSums = c->gradientSums;
    network_weights_training *firstMoment = c->firstMoment;
    network_weights_training *secondMoment = c->secondMoment;

    for(int i = 0; i < HELPER_THREAD_COUNT; i++)
    {
        for(int j = 0; j < 2 * ACCUMULATOR_NODES_PER_SIDE; j++) for(int k = 0; k < SECOND_HIDDEN_LAYER_NODES; k++) gradientSums[HELPER_THREAD_COUNT].weights2[k][j] +=  gradientSums[i].weights2[k][j];
    }

    for(int i = 0; i < 2 * ACCUMULATOR_NODES_PER_SIDE; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            gradientSums[HELPER_THREAD_COUNT].weights2[j][i] /= POSITIONS_PER_FILE;
            updateWeight(&trainingNNUE->weights2[j][i], &gradientSums[HELPER_THREAD_COUNT].weights2[j][i], &firstMoment->weights2[j][i], &secondMoment->weights2[j][i]);
        }
    }
}

VOID CALLBACK UpdateWeights2_bias(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work) {
    updateContext* c = (updateContext*)Context;

    network_weights_training *gradientSums = c->gradientSums;
    network_weights_training *firstMoment = c->firstMoment;
    network_weights_training *secondMoment = c->secondMoment;

    for(int i = 0; i < HELPER_THREAD_COUNT; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) gradientSums[HELPER_THREAD_COUNT].weights2_bias[j] +=  gradientSums[i].weights2_bias[j];
    }

    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        gradientSums[HELPER_THREAD_COUNT].weights2_bias[i] /= POSITIONS_PER_FILE;
        updateWeight(&trainingNNUE->weights2_bias[i], &gradientSums[HELPER_THREAD_COUNT].weights2_bias[i], &firstMoment->weights2_bias[i], &secondMoment->weights2_bias[i]);
    }
}

VOID CALLBACK UpdateWeights1(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work) {
    updateContext* c = (updateContext*)Context;

    network_weights_training *gradientSums = c->gradientSums;
    network_weights_training *firstMoment = c->firstMoment;
    network_weights_training *secondMoment = c->secondMoment;

    for(int i = 0; i < HELPER_THREAD_COUNT; i++)
    {
        for(int j = 0; j < HALF_INPUT_BITS; j++) for(int k = 0; k < ACCUMULATOR_NODES_PER_SIDE; k++) gradientSums[HELPER_THREAD_COUNT].weights1[k][j] +=  gradientSums[i].weights1[k][j]; 
    }

    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            gradientSums[HELPER_THREAD_COUNT].weights1[j][i] /= POSITIONS_PER_FILE;
            updateWeight(&trainingNNUE->weights1[j][i], &gradientSums[HELPER_THREAD_COUNT].weights1[j][i], &firstMoment->weights1[j][i], &secondMoment->weights1[j][i]);
        }
    }
}

VOID CALLBACK UpdateWeights1_bias(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work) {
    updateContext* c = (updateContext*)Context;

    network_weights_training *gradientSums = c->gradientSums;
    network_weights_training *firstMoment = c->firstMoment;
    network_weights_training *secondMoment = c->secondMoment;

    
    for(int i = 0; i < HELPER_THREAD_COUNT; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++) gradientSums[HELPER_THREAD_COUNT].weights1_bias[j] +=  gradientSums[i].weights1_bias[j];  
    }

    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
    {
        gradientSums[HELPER_THREAD_COUNT].weights1_bias[i] /= POSITIONS_PER_FILE;
        updateWeight(&trainingNNUE->weights1_bias[i], &gradientSums[HELPER_THREAD_COUNT].weights1_bias[i], &firstMoment->weights1_bias[i], &secondMoment->weights1_bias[i]);
    }
}

void updateAllWeights(network_weights_training* gradientSums, network_weights_training* firstMoment, network_weights_training* secondMoment)
{
    PTP_POOL pool = CreateThreadpool(NULL);
    
    SetThreadpoolThreadMaximum(pool, HELPER_THREAD_COUNT);
    SetThreadpoolThreadMinimum(pool, 1);

    
    TP_CALLBACK_ENVIRON threadEnvironment;
    InitializeThreadpoolEnvironment(&threadEnvironment);
    SetThreadpoolCallbackPool(&threadEnvironment, pool);

    PTP_CLEANUP_GROUP cleanupGroup = CreateThreadpoolCleanupGroup();
    SetThreadpoolCallbackCleanupGroup(&threadEnvironment, cleanupGroup, NULL);

    updateContext context = {
        .gradientSums = gradientSums,
        .firstMoment = firstMoment,
        .secondMoment = secondMoment
    };
    
    biasCorrection1 = powf(ADAM_BETA1, timestamp);
    biasCorrection2 = powf(ADAM_BETA2, timestamp);

    SubmitThreadpoolWork(CreateThreadpoolWork(UpdateWeights4, &context, &threadEnvironment));
    SubmitThreadpoolWork(CreateThreadpoolWork(UpdateWeights3, &context, &threadEnvironment));
    SubmitThreadpoolWork(CreateThreadpoolWork(UpdateWeights3_bias, &context, &threadEnvironment));
    SubmitThreadpoolWork(CreateThreadpoolWork(UpdateWeights2, &context, &threadEnvironment));
    SubmitThreadpoolWork(CreateThreadpoolWork(UpdateWeights2_bias, &context, &threadEnvironment));
    SubmitThreadpoolWork(CreateThreadpoolWork(UpdateWeights1, &context, &threadEnvironment));
    SubmitThreadpoolWork(CreateThreadpoolWork(UpdateWeights1_bias, &context, &threadEnvironment));

    for(int i = 0; i < HELPER_THREAD_COUNT; i++) gradientSums[HELPER_THREAD_COUNT].weights4_bias +=  gradientSums[i].weights4_bias;
    gradientSums[HELPER_THREAD_COUNT].weights4_bias /= POSITIONS_PER_FILE;
    updateWeight(&trainingNNUE->weights4_bias, &gradientSums[HELPER_THREAD_COUNT].weights4_bias, &firstMoment->weights4_bias, &secondMoment->weights4_bias);
    
    CloseThreadpoolCleanupGroupMembers(cleanupGroup, FALSE, NULL);
    CloseThreadpoolCleanupGroup(cleanupGroup);
    DestroyThreadpoolEnvironment(&threadEnvironment);
    CloseThreadpool(pool);

    timestamp++;
}

void train(int saveEveryNBlocks, int maxIterations, float maxAllowedError, accumulator_training* floatAccumulator)
{
    assert(floatAccumulator);

    int* blockNumbers = CALLOC(FILE_COUNT, sizeof(int));
    for(int i = 0; i < FILE_COUNT; i++)
    {
        blockNumbers[i] = i + 1;
    }

    int totalIterations = 0;

    
    HANDLE helperThreads[HELPER_THREAD_COUNT] = {0};
    DWORD helperThreadID[HELPER_THREAD_COUNT] = {0};
    rpropThreadData threadData[HELPER_THREAD_COUNT] = {0};

    network_weights_training* gradientSums = CALLOC(HELPER_THREAD_COUNT + 1, sizeof(network_weights_training)); //Final index is the accumlated sum
    network_weights_training* firstMoment = CALLOC(1, sizeof(network_weights_training)); //Final index is the accumlated sum
    network_weights_training* secondMoment = CALLOC(1, sizeof(network_weights_training));

    double* sumSquaredErrors = CALLOC(HELPER_THREAD_COUNT, sizeof(double));
    double totalSumSquaredError = 0.0; //accumulated value.

    char fileName[40] = {'\0'};
    for(int i = 0; i < HELPER_THREAD_COUNT; i++)
    {
        threadData[i].gradientSums = &gradientSums[i];
        threadData[i].entriesToTrack = (int) floor(POSITIONS_PER_FILE / HELPER_THREAD_COUNT);
        threadData[i].fileName = fileName;
        threadData[i].sumSquaredError = &sumSquaredErrors[i];
    }

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
            fseek(trainingData, 0, SEEK_END);
            long fileBytes = ftell(trainingData);
            fclose(trainingData);
            for(int i = 0; i < HELPER_THREAD_COUNT; i++)
            {
                *threadData[i].sumSquaredError = 0;
                threadData[i].seekByteOffset = i * (long) floor(fileBytes / HELPER_THREAD_COUNT);
                helperThreads[i] = CreateThread(NULL, 0, calculateGradients, &threadData[i], 0, &helperThreadID[i]);
            }

            for(int i = 0; i < HELPER_THREAD_COUNT; i++) 
            {
                WaitForSingleObject(helperThreads[i], INFINITE);
                CloseHandle(helperThreads[i]);
            }

            memset(&gradientSums[HELPER_THREAD_COUNT], 0, sizeof(network_weights_training));

            double sumSquaredError = 0.0;
            for(int i = 0; i < HELPER_THREAD_COUNT; i++) sumSquaredError+= sumSquaredErrors[i];
            totalSumSquaredError+= sumSquaredError;

            updateAllWeights(gradientSums, firstMoment, secondMoment);
            

            if(saveEveryNBlocks && blockIndex > 0 && blockIndex%saveEveryNBlocks == 0) save_trainingWeights();
            printf("\33[2K\r\tAnalyzed block %d/%d; MSE = %e", blockIndex + 1, FILE_COUNT, sumSquaredError/POSITIONS_PER_FILE);
        }
        totalIterations++;
        
        printf("Total sum squared error: %e\n", totalSumSquaredError);
        printf("\33[2K\rIteration %d MSE = %e\n", totalIterations, totalSumSquaredError/(POSITIONS_PER_FILE * FILE_COUNT));


        //Mean squared error.
        
    } while((totalSumSquaredError/(POSITIONS_PER_FILE * FILE_COUNT)) > maxAllowedError && totalIterations < maxIterations);

    FREE(blockNumbers);
    save_trainingWeights();
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
                evaluation = (evaluation / 127) + 0.5;
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