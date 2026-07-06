#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/engine.h"
#include "train/train.h"
#include "omp.h"
#include "binpack/viri_binpack.h"
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

int offsetGenerator_xorshift32(uint32_t* state, int kingBucket)
{
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    int y = x % 1000;
    if(y < PERMUTE_BUCKET_PROBABILITY)
    {
        if(kingBucket >= 0)
            return neighboringKingBuckets[kingBucket][y % 5];
        else
        {
            if(y < PERMUTE_BUCKET_PROBABILITY / 2) return -1;
            else return 1;
        }
    }
    return 0;
}

void prepareMinibatchData(binpackDetails* details, int inputGroup, bitboard* board, uint32_t* xorRNGState,
                            short* activeInputs_A, float* expectedOutputs_A, char* outputBuckets_A,
                            short* activeInputs_B, float* expectedOutputs_B, char* outputBuckets_B)
{
    for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
    {
        Viri_Score score;
        uint8_t result;
        binpack_next(details, board, &score, &result, 1);
        float relativeResult = 0.5f;
        if(result == VIRI_DRAW) relativeResult = 0.5f;
        else if(result == VIRI_WHITE_WIN)
        {
            if(board->turn == WHITE) relativeResult = 1.0f;
            else relativeResult = 0.0f;
        }
        else if(result == VIRI_BLACK_WIN)
        {
            if(board->turn == BLACK) relativeResult = 1.0f;
            else relativeResult = 0.0f;
        }

        if(inputGroup == INPUT_GROUP_A) 
            expectedOutputs_A[entryNumber] = LAMBDA * (SIGMOID(score / EVAL_SCALE)) + (1.0f - LAMBDA) * relativeResult;
        else 
            expectedOutputs_B[entryNumber] = LAMBDA * (SIGMOID(score / EVAL_SCALE)) + (1.0f - LAMBDA) * relativeResult;

        uint64_t inputs[2 * PIECE_COUNT] = {0};

        int trackedPiecesPerColor = PIECE_COUNT / 2;
        for(int i = 0; i < PIECE_COUNT / 2; i++)
        {
            //White's perspective
            inputs[i] = board->pieces[2 * i];
            inputs[trackedPiecesPerColor + i] = board->pieces[2 * i + 1];

            //Black's perspective
            inputs[PIECE_COUNT + i] = FLIP_MASK(board->pieces[2 * i + 1]);
            inputs[PIECE_COUNT + trackedPiecesPerColor + i] = FLIP_MASK(board->pieces[2 * i]);
        }

        if(getColumn(board->kingSquare_w) > 3)
            for(int p = 0; p < PIECE_COUNT; p++) inputs[p] = mirrorBoard(inputs[p]);
        if(getColumn(board->kingSquare_b) > 3)
            for(int p = PIECE_COUNT; p < 2 * PIECE_COUNT; p++) inputs[p] = mirrorBoard(inputs[p]);
        
        for(int color = 0; color < 2; color++)
        {
            #ifndef COMPRESS_KING_BUCKET
            int baseIndex = (color == WHITE) ? kingBuckets[board->kingSquare_w] : kingBuckets[FLIP_SQUARE(board->kingSquare_b)];
            baseIndex += offsetGenerator_xorshift32(xorRNGState, baseIndex);
            baseIndex = clamp(baseIndex, 0, KING_BUCKETS - 1);
            baseIndex *= BITS_PER_KING_BUCKET;
            #else
            int baseIndex = 0;
            #endif

            int trackedInputs = 0;

            //First half matches side to move.
            // side = 0 if color matches board->turn
            int side = (color != board->turn);
            
            for(int piece = 0; piece < PIECE_COUNT; piece++)
            {
                uint64_t mask = inputs[PIECE_COUNT * color + piece];
                while(mask)
                {
                    if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                    else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                    trackedInputs++;
                    mask&=(mask - 1);
                }
            }

            #ifndef COMPRESS_OUTPUT_BUCKET
            int outputBucket = ((trackedInputs - 1) / 4) + offsetGenerator_xorshift32(xorRNGState, -1);
            outputBucket = clamp(outputBucket, 0, OUTPUT_BUCKETS - 1)
            if(inputGroup == INPUT_GROUP_A) outputBuckets_A[entryNumber] = outputBucket;
            else outputBuckets_B[entryNumber] = outputBucket;
            #else
            if(inputGroup == INPUT_GROUP_A) outputBuckets_A[entryNumber] = 0;
            else outputBuckets_B[entryNumber] = 0;
            #endif

            //-1 padding
            while(trackedInputs < 32)
            {
                if(inputGroup == INPUT_GROUP_A) activeInputs_A[entryNumber * 64 + 32 * side + trackedInputs] = -1;
                else activeInputs_B[entryNumber * 64 + 32 * side + trackedInputs] = -1;
                trackedInputs++;
            }
        }
    }
}

void train(int maxIterations, float maxAllowedError)
{
    binpackDetails trainingBinpack = binpack_open(TRAINING_DATA_PATH, 0);
    binpackDetails validationBinpack = binpack_open(VALIDATION_DATA_PATH, 0);

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

    int totalIterations = 0;
    float totalLoss = 0.0; //accumulated value per iteration
    clock_t startTime, endTime;
    double duration_sec;
    int inputGroup = INPUT_GROUP_A;
    bitboard* board = create_board(NULL);

    startTime = clock();
    for(int i = 0; i < VALIDATION_BINPACK_MINIBATCHES; i++)
    {
        inputGroup ^= 1;
        uint32_t xorRNGState = time(NULL);
        prepareMinibatchData(&validationBinpack, inputGroup, board, &xorRNGState,
                                activeInputs_A, expectedOutputs_A, outputBuckets_A,
                                activeInputs_B, expectedOutputs_B, outputBuckets_B);

                                
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
            uint32_t xorRNGState = (uint32_t) time(NULL);
            prepareMinibatchData(&trainingBinpack, inputGroup, board, &xorRNGState,
                                    activeInputs_A, expectedOutputs_A, outputBuckets_A,
                                    activeInputs_B, expectedOutputs_B, outputBuckets_B);

            //If we comment out hipEventSynchronize and enqueueKernels, we end up with 1.4 million positions/second.
            //This is primarily bounded by input processing speed, not binpack I/O.
            enqueueKernels(inputGroup, 1);
            hipEventSynchronize(hip_events.readLoss);
            
            float loss = sumLoss(loss_buffer);
            totalLoss+=loss;

            if((minibatchNumber + 1) % 25 == 0) printf("\33[2K\r\tAnalyzed block %d/%d; Loss = %e", minibatchNumber + 1, MINIBATCHES_PER_EPOCH, loss / MINIBATCH_SIZE);
        }
        totalIterations++;
        endTime = clock();
        duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC; 

        totalLoss = totalLoss/(MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH);
        printf("\33[2K\rEpoch %d Loss = %e (%.1fs at %d pos/sec); ", totalIterations, totalLoss, duration_sec, (int) ((MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH) / duration_sec));

        totalLoss = 0.0;

        startTime = clock();
        for(int i = 0; i < VALIDATION_BINPACK_MINIBATCHES; i++)
        {
            inputGroup ^= 1;
            uint32_t xorRNGState = time(NULL);
            prepareMinibatchData(&validationBinpack, inputGroup, board, &xorRNGState,
                                    activeInputs_A, expectedOutputs_A, outputBuckets_A,
                                    activeInputs_B, expectedOutputs_B, outputBuckets_B);

                                    
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
        

    } while(minimumLoss > maxAllowedError && totalIterations < maxIterations);

    free(board);
    freeHIP();
    binpack_close(&trainingBinpack);
    binpack_close(&validationBinpack);
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