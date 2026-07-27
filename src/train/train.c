#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/search.h"
#include "train/train.h"
#include "binpack/viri_binpack.h"
#include "train/data_queue.h"
#include <string.h>
#include <float.h>

/*** Training Weights ***/

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

float sumLoss(float* loss)
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

//Currently bottlenecked by single-threaded binpack I/O & processing, not gpu kernels.
void train(int maxIterations, float maxAllowedError)
{
    //I'm not too worried about opening 2 * threadCount threads since
    //half should wait once their queue fills up.
    const int BINPACK_READERS = threadCount;
    binpackDetails trainingBinpack = binpack_open(TRAINING_DATA_PATH, BINPACK_READERS);
    binpackDetails validationBinpack = binpack_open(VALIDATION_DATA_PATH, BINPACK_READERS);

    #ifdef COMPRESS_KING_BUCKET
    compressKingBucket(raw_weights);
    #endif
    #ifdef COMPRESS_OUTPUT_BUCKET
    compressOutputBucket(raw_weights);
    #endif
    
    short* activeInputs_A = NULL;
    float* expectedOutputs_A = NULL;
    char* outputBuckets_A = NULL;

    short* activeInputs_B = NULL;
    float* expectedOutputs_B = NULL;
    char* outputBuckets_B = NULL;

    float* loss_buffer = NULL;
    
    int cl_errorcode = initHIP(raw_weights, &activeInputs_A, &expectedOutputs_A, &outputBuckets_A, &activeInputs_B, &expectedOutputs_B, &outputBuckets_B, &loss_buffer);
    if(cl_errorcode != hipSuccess) 
    {
        DEBUG_ERROR("Failed to init kernels - Error Code: %d\n%s", cl_errorcode, hipGetErrorString(cl_errorcode));
        binpack_close(&trainingBinpack);
        binpack_close(&validationBinpack);
        return;
    }

    //Each queue gets one reader that continuously generates data.
    MinibatchQueue* trainingQueue = calloc(1, sizeof(MinibatchQueue));
    MinibatchQueue* validationQueue = calloc(1, sizeof(MinibatchQueue));
    
    binpack_queue_init(validationQueue);
    binpack_queue_init(trainingQueue);
    
    dataWorkerArgs* trainingDataParserArgs = calloc(BINPACK_READERS, sizeof(dataWorkerArgs));
    dataWorkerArgs* validationDataParserArgs = calloc(BINPACK_READERS, sizeof(dataWorkerArgs));

    for(int i = 0; i < BINPACK_READERS; i++)
    {
        trainingDataParserArgs[i].details = &trainingBinpack;
        trainingDataParserArgs[i].queue = trainingQueue;
        trainingDataParserArgs[i].fenSkip = FEN_SKIP_TRAINING;
        trainingDataParserArgs[i].seed = time(NULL);
        trainingDataParserArgs[i].readerIndex = i;
        
        validationDataParserArgs[i].details = &validationBinpack;
        validationDataParserArgs[i].queue = validationQueue;
        validationDataParserArgs[i].fenSkip = FEN_SKIP_TRAINING;
        validationDataParserArgs[i].seed = time(NULL);
        validationDataParserArgs[i].readerIndex = i;
    }


    THREADTYPE trainDataThread[BINPACK_READERS];
    THREADTYPE validationDataThread[BINPACK_READERS];
    
    for(int i = 0; i < BINPACK_READERS; i++)
    {
        THREAD_START(validationDataThread[i], fillMinibatchQueue, &validationDataParserArgs[i]);
        THREAD_START(trainDataThread[i], fillMinibatchQueue, &trainingDataParserArgs[i]);
    }
    
    PreparedMinibatch* readyMinibatch = calloc(1, sizeof(PreparedMinibatch));

    int totalEpochs = 0;
    float totalLoss = 0.0; //accumulated value per iteration
    clock_t startTime, endTime;
    double duration_sec;
    int inputGroup = INPUT_GROUP_A;
    
    startTime = clock();
    for(int i = 0; i < VALIDATION_BINPACK_MINIBATCHES; i++)
    {
        inputGroup ^= 1;

        binpack_queue_pop(validationQueue, readyMinibatch);

        if(inputGroup == INPUT_GROUP_A) 
        {
            memcpy(activeInputs_A, &readyMinibatch->activeInputs, sizeof(readyMinibatch->activeInputs));
            memcpy(expectedOutputs_A, &readyMinibatch->expectedOutputs, sizeof(readyMinibatch->expectedOutputs));
            memcpy(outputBuckets_A, &readyMinibatch->outputBuckets, sizeof(readyMinibatch->outputBuckets));
        } 
        else 
        {
            memcpy(activeInputs_B, &readyMinibatch->activeInputs, sizeof(readyMinibatch->activeInputs));
            memcpy(expectedOutputs_B, &readyMinibatch->expectedOutputs, sizeof(readyMinibatch->expectedOutputs));
            memcpy(outputBuckets_B, &readyMinibatch->outputBuckets, sizeof(readyMinibatch->outputBuckets));
        }
                                
        enqueueKernels(inputGroup, 0);
        hipEventSynchronize(hip_events.readLoss);
        
        float loss = sumLoss(loss_buffer);
        totalLoss+=loss;
    }
    endTime = clock();

    duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC; 
    totalLoss = totalLoss/(MINIBATCH_SIZE * VALIDATION_BINPACK_MINIBATCHES);
    float minimumLoss = totalLoss;

    //Yellow text
    //Validation pos/sec is roughly double that of training. It doesn't have to perform backprop.
    printf("Initial Validation Loss = \033[0;33m%e\033[0m (%.1fs at %d pos/sec)\n", totalLoss, duration_sec, (int) ((MINIBATCH_SIZE * VALIDATION_BINPACK_MINIBATCHES) / duration_sec));

    do{
        totalLoss = 0.0;

        startTime = clock();
        for(int minibatchNumber = 0; minibatchNumber < MINIBATCHES_PER_EPOCH; minibatchNumber++)
        {
            inputGroup ^= 1;

            binpack_queue_pop(trainingQueue, readyMinibatch);

            if(inputGroup == INPUT_GROUP_A) 
            {
                memcpy(activeInputs_A, &readyMinibatch->activeInputs, sizeof(readyMinibatch->activeInputs));
                memcpy(expectedOutputs_A, &readyMinibatch->expectedOutputs, sizeof(readyMinibatch->expectedOutputs));
                memcpy(outputBuckets_A, &readyMinibatch->outputBuckets, sizeof(readyMinibatch->outputBuckets));
            } 
            else
            {
                memcpy(activeInputs_B, &readyMinibatch->activeInputs, sizeof(readyMinibatch->activeInputs));
                memcpy(expectedOutputs_B, &readyMinibatch->expectedOutputs, sizeof(readyMinibatch->expectedOutputs));
                memcpy(outputBuckets_B, &readyMinibatch->outputBuckets, sizeof(readyMinibatch->outputBuckets));
            }

            enqueueKernels(inputGroup, 1);
            hipEventSynchronize(hip_events.readLoss);
            
            float loss = sumLoss(loss_buffer);
            totalLoss+=loss;

            if((minibatchNumber + 1) % 25 == 0) printf("\33[2K\r\tAnalyzed block %d/%d; Loss = %e", minibatchNumber + 1, MINIBATCHES_PER_EPOCH, loss / MINIBATCH_SIZE);
        }
        endTime = clock();

        duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC; 
        totalLoss = totalLoss/(MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH);
        printf("\33[2K\rEpoch %d Loss = %e (%.1fs at %d pos/sec); ", ++totalEpochs, totalLoss, duration_sec, (int) ((MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH) / duration_sec));

        totalLoss = 0.0;

        startTime = clock();
        for(int i = 0; i < VALIDATION_BINPACK_MINIBATCHES; i++)
        {
             inputGroup ^= 1;

            binpack_queue_pop(validationQueue, readyMinibatch);

            if(inputGroup == INPUT_GROUP_A) 
            {
                memcpy(activeInputs_A, &readyMinibatch->activeInputs, sizeof(readyMinibatch->activeInputs));
                memcpy(expectedOutputs_A, &readyMinibatch->expectedOutputs, sizeof(readyMinibatch->expectedOutputs));
                memcpy(outputBuckets_A, &readyMinibatch->outputBuckets, sizeof(readyMinibatch->outputBuckets));
            } 
            else 
            {
                memcpy(activeInputs_B, &readyMinibatch->activeInputs, sizeof(readyMinibatch->activeInputs));
                memcpy(expectedOutputs_B, &readyMinibatch->expectedOutputs, sizeof(readyMinibatch->expectedOutputs));
                memcpy(outputBuckets_B, &readyMinibatch->outputBuckets, sizeof(readyMinibatch->outputBuckets));
            }
                                    
            enqueueKernels(inputGroup, 0);
            hipEventSynchronize(hip_events.readLoss);
            
            float loss = sumLoss(loss_buffer);
            totalLoss+=loss;
        }
        endTime = clock();
        duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC; 
        totalLoss = totalLoss/(MINIBATCH_SIZE * VALIDATION_BINPACK_MINIBATCHES);

        if(totalLoss < minimumLoss)
        {
            getWeights(raw_weights);
            #ifdef COMPRESS_KING_BUCKET
            broadcastKingBucket(raw_weights);
            #endif
            #ifdef COMPRESS_OUTPUT_BUCKET
            broadcastOutputBucket(raw_weights);
            #endif
            saveRawWeights();
            quantizeWeights(raw_weights, int_weights);
            saveQuantizedWeights();
            minimumLoss = totalLoss;

            //Green text
            printf("Validation Loss = \033[0;32m%e\033[0m (%.1fs at %d pos/sec)\n", totalLoss, duration_sec, (int) ((MINIBATCH_SIZE * VALIDATION_BINPACK_MINIBATCHES) / duration_sec));
        }
        else
        {
            //Red text
            printf("Validation Loss = \033[0;31m%e\033[0m (%.1fs at %d pos/sec)\n", totalLoss, duration_sec, (int) ((MINIBATCH_SIZE * VALIDATION_BINPACK_MINIBATCHES) / duration_sec));
        }
    } while(minimumLoss > maxAllowedError && totalEpochs < maxIterations);

    validationQueue->stop_signal = 1;
    trainingQueue->stop_signal = 1;
    BROADCAST_COND_VARIABLE(validationQueue->not_full);
    BROADCAST_COND_VARIABLE(validationQueue->not_empty);
    BROADCAST_COND_VARIABLE(trainingQueue->not_full);
    BROADCAST_COND_VARIABLE(trainingQueue->not_empty);

    freeHIP();

    free(trainingQueue);
    free(validationQueue);
    free(readyMinibatch);

    binpack_close(&trainingBinpack);
    binpack_close(&validationBinpack);

    for(int i = 0; i < BINPACK_READERS; i++)
    {
        THREAD_WAIT(validationDataThread[i]);
        THREAD_WAIT(trainDataThread[i]);
    }
}

void compressKingBucket(training_weights* weights)
{
    int weightsPerKingBucket = BITS_PER_KING_BUCKET * ACCUMULATOR_NODES_PER_SIDE;
    float* flatWeightPtr = &weights->weights1[0][0];
    for(int weightIndex = 0; weightIndex < weightsPerKingBucket; weightIndex++)
    {
        float sum = 0.0f;
        for(int b = 0; b < KING_BUCKETS; b++)
        {
            sum += flatWeightPtr[b * weightsPerKingBucket + weightIndex];
        }
        flatWeightPtr[weightIndex] = sum / KING_BUCKETS;
    }
}

void compressOutputBucket(training_weights* weights)
{
    for(int weightIndex = 0; weightIndex < ACCUMULATOR_NODES; weightIndex++)
    {
        float sum = 0.0f;
        for(int b = 0; b < OUTPUT_BUCKETS; b++)
        {
            sum += weights->weights2[b][weightIndex];
        }
        weights->weights2[0][weightIndex] = sum / OUTPUT_BUCKETS;
    }
    
    float biasSum = 0.0f;
    for(int b = 0; b < OUTPUT_BUCKETS; b++)
        biasSum += weights->weights2_bias[b];
    
    weights->weights2_bias[0] = biasSum / OUTPUT_BUCKETS;
}

void broadcastKingBucket(training_weights* weights)
{
    int weightsPerKingBucket = BITS_PER_KING_BUCKET * ACCUMULATOR_NODES_PER_SIDE;
    float* flatWeightPtr = &weights->weights1[0][0];
    for(int offset = weightsPerKingBucket; offset < KING_BUCKETS * weightsPerKingBucket; offset += weightsPerKingBucket)
    {
        memcpy(&flatWeightPtr[offset], &flatWeightPtr[0], weightsPerKingBucket * sizeof(float));
    }
}

void broadcastOutputBucket(training_weights* weights)
{
    for(int b = 1; b < OUTPUT_BUCKETS; b++)
    {
        memcpy(weights->weights2[b], weights->weights2[0], sizeof(weights->weights2[0]));
        memcpy(&weights->weights2_bias[b], &weights->weights2_bias[0], sizeof(weights->weights2_bias[0]));
    }
}