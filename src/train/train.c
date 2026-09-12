#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/search.h"
#include "train/train.h"
#include "binpack/viri_binpack.h"
#include <string.h>
#include <float.h>

/*** Training Weights ***/

//Box-Muller Transform
double sampleNormalDistribution(double mean, double stddev)
{
    double u1, u2;
    do { u1 = (double)rand() / RAND_MAX; } while(u1 <= 0.0);
    u2 = (double) rand() / RAND_MAX;
    return mean + stddev * (sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2));
}

training_weights* initializeTrainingWeights()
{
    training_weights* raw_weights = calloc(1, sizeof(training_weights));

    float stddev = sqrtf(2.0 / BITS_PER_KING_BUCKET);

    for(int i = 0; i < BITS_PER_KING_BUCKET; i++)
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            raw_weights->factorizer_weights[i][j] = clamp(sampleNormalDistribution(0.0, stddev), -1.98f / 2, 1.98f / 2);

    for(int i = 0; i < INPUT_BITS_PER_SIDE; i++)
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            raw_weights->accumulator_weights[i][j] = clamp(sampleNormalDistribution(0.0, stddev), -1.98f / 2, 1.98f / 2);

    stddev = sqrtf(2.0 / ACCUMULATOR_NODES);
    for(int b = 0; b < OUTPUT_BUCKETS; b++)
        for(int i = 0; i < ACCUMULATOR_NODES; i++)
            raw_weights->output_weights[b][i] = clamp(sampleNormalDistribution(0.0, stddev), -1.98f, 1.98f);

    return raw_weights;
}

void saveRawWeights(training_weights* weights)
{
    assert(weights);
    FILE* output = fopen("./import/raw.bin", "wb");
    fwrite(weights, sizeof(training_weights), 1, output);
    fclose(output);
}

void quantizeWeights(training_weights* raw, nnue_weights* quantized)
{
    for(int i = 0; i < INPUT_BITS_PER_SIDE; i++)
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            quantized->accumulator_weights[i][j] = (int16_t) lroundf(QA * (raw->factorizer_weights[i % BITS_PER_KING_BUCKET][j] + raw->accumulator_weights[i][j]));
    
    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
        quantized->accumulator_bias[i] = (int16_t) lroundf(QA * raw->accumulator_bias[i]);

    for(int b = 0; b < OUTPUT_BUCKETS; b++)
    {
        for(int i = 0; i < ACCUMULATOR_NODES; i++)
            quantized->output_weights[b][i] = (int16_t) lroundf(QB * raw->output_weights[b][i]);
        quantized->output_bias[b] = (int16_t) lroundf(QA * QB * raw->output_bias[b]);
    }
}

void saveQuantizedWeights(nnue_weights* weights)
{
    FILE* output = fopen("./import/quantized.bin", "wb");
    fwrite(weights, sizeof(nnue_weights), 1, output);
    fclose(output);
}

float sumLoss(float* loss)
{
    __m256 tempSum = _mm256_setzero_ps();
    for(int i = 0; i < MINIBATCH_SIZE; i+=8)
        tempSum = _mm256_add_ps(tempSum, _mm256_loadu_ps(&loss[i]));

    __m128 sum128 = _mm_add_ps(_mm256_castps256_ps128((__m256)tempSum), _mm256_extractf128_ps(tempSum, 1));
    
    sum128 = _mm_add_ps(sum128, _mm_movehl_ps(sum128, sum128)); 
    sum128 = _mm_add_ss(sum128, _mm_shuffle_ps(sum128, sum128, _MM_SHUFFLE(1, 1, 1, 1))); 
    return _mm_cvtss_f32(sum128);
}

void prepareMinibatchData(binpackDetails* details, int inputGroup, bitboard* board,
                            short* activeInputs_A, float* expectedOutputs_A, char* outputBuckets_A,
                            short* activeInputs_B, float* expectedOutputs_B, char* outputBuckets_B)
{
    short* activeInputs = (inputGroup == INPUT_GROUP_A) ? activeInputs_A : activeInputs_B;
    float* expectedOutputs = (inputGroup == INPUT_GROUP_A) ? expectedOutputs_A : expectedOutputs_B;
    char* outputBuckets = (inputGroup == INPUT_GROUP_A) ? outputBuckets_A : outputBuckets_B;
    for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
    {
        Viri_Score score;
        uint8_t result;
        binpack_next(details, board, &score, &result, 1, FEN_SKIP_TRAINING);

        //Convert from white-relative
        if(ISBLACK(board->turn))
            score = -score;

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
        
        expectedOutputs[entryNumber] = LAMBDA * (SIGMOID((float) score / EVAL_SCALE)) + (1.0f - LAMBDA) * relativeResult;

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

        if(getColumn(board->kingSquare[WHITE]) > 3)
            for(int p = 0; p < PIECE_COUNT; p++) 
                inputs[p] = mirrorBoard(inputs[p]);
        if(getColumn(board->kingSquare[BLACK]) > 3)
            for(int p = PIECE_COUNT; p < 2 * PIECE_COUNT; p++) 
                inputs[p] = mirrorBoard(inputs[p]);
        
        for(int color = 0; color < 2; color++)
        {
            int baseIndex = (color == WHITE) ? kingBuckets[board->kingSquare[WHITE]] : kingBuckets[FLIP_SQUARE(board->kingSquare[BLACK])];
            baseIndex *= BITS_PER_KING_BUCKET;

            int trackedInputs = 0;

            //First half matches side to move.
            // side = 0 if color matches board->turn. side indexes into [entryNumber][side][trackedInputs]
            int side = (color != board->turn);
            
            for(int piece = 0; piece < PIECE_COUNT; piece++)
            {
                uint64_t mask = inputs[PIECE_COUNT * color + piece];
                while(mask)
                {
                    activeInputs[entryNumber * 64 + 32 * side + trackedInputs] = baseIndex + 64 * piece + __builtin_ctzll(mask);
                    
                    trackedInputs++;
                    mask&=(mask - 1);
                }
            }

            if(color == 0)
                outputBuckets[entryNumber] = (trackedInputs - 2) / 4;
            
            //-1 terminator
            if(trackedInputs < 32)
                activeInputs[entryNumber * 64 + 32 * side + trackedInputs] = -1;
        }
    }
}

void train(int maxIterations, const char* binpackFileName, const char* kernelFileName)
{
    short* activeInputs_A = NULL;
    float* expectedOutputs_A = NULL;
    char* outputBuckets_A = NULL;

    short* activeInputs_B = NULL;
    float* expectedOutputs_B = NULL;
    char* outputBuckets_B = NULL;

    float* loss_buffer = NULL;
    
    training_weights* raw_weights;
    FILE* raw_weights_file;
    if((raw_weights_file = fopen("./import/raw.bin", "rb")) != NULL)
    {
        raw_weights = calloc(1, sizeof(training_weights));
        fread(raw_weights, sizeof(training_weights), 1, raw_weights_file);
        fclose(raw_weights_file);
        raw_weights_file = NULL;
    }
    else
        raw_weights = initializeTrainingWeights();
    nnue_weights* quantized_weights = calloc(1, sizeof(nnue_weights));

    int cl_errorcode = initHIP(raw_weights, kernelFileName, &activeInputs_A, &expectedOutputs_A, &outputBuckets_A, &activeInputs_B, &expectedOutputs_B, &outputBuckets_B, &loss_buffer);
    if(cl_errorcode != hipSuccess) 
    {
        printf("Failed to init kernels - Error Code: %d\n%s", cl_errorcode, hipGetErrorString(cl_errorcode));
        return;
    }
    
    binpackDetails trainingBinpack;
    binpack_open(&trainingBinpack, binpackFileName, 1);
    
    int totalEpochs = 0;
    float totalLoss = 0.0; //accumulated value per iteration
    clock_t startTime, endTime;
    double duration_sec;
    int inputGroup = INPUT_GROUP_A;
    bitboard board;
    do{
        totalLoss = 0.0;

        startTime = clock();
        for(int minibatchNumber = 0; minibatchNumber < MINIBATCHES_PER_EPOCH; minibatchNumber++)
        {
            prepareMinibatchData(&trainingBinpack, inputGroup, &board,
                                    activeInputs_A, expectedOutputs_A, outputBuckets_A,
                                    activeInputs_B, expectedOutputs_B, outputBuckets_B);

            enqueueKernels(inputGroup);
            hipEventSynchronize(hip_events.readLoss);
            
            float loss = sumLoss(loss_buffer);
            totalLoss+=loss;

            if((minibatchNumber + 1) % 25 == 0) 
                printf("\33[2K\r\tAnalyzed block %d/%d; Loss = %e", minibatchNumber + 1, MINIBATCHES_PER_EPOCH, loss / MINIBATCH_SIZE);
            inputGroup ^= 1;
        }
        endTime = clock();

        duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC; 
        totalLoss = totalLoss/(MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH);
        printf("\33[2K\rEpoch %d Loss = %e (%.1fs at %d pos/sec)\n", ++totalEpochs, totalLoss, duration_sec, (int) ((MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH) / duration_sec));

        getWeights(raw_weights);
        saveRawWeights(raw_weights);
        quantizeWeights(raw_weights, quantized_weights);
        saveQuantizedWeights(quantized_weights);
    } while(totalEpochs < maxIterations);

    free(raw_weights);
    free(quantized_weights);

    freeHIP();
    binpack_close(&trainingBinpack);

}