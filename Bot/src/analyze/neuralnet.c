#include "analyze/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/engine.h"
#include "gpu/gpu_funcs.h"
#include "omp.h"
#include <float.h>
#include <string.h>

network_weights* nnue_weights = NULL;

/*** Creating/loading weights ***/
//Box-Muller transform.
void sampleNormalDistribution(float* dest, double standardDeviation) 
{
    do{
        double u1; 
        do { u1 = (double)rand() / (double) RAND_MAX; } while(u1 == 0);
        *dest = standardDeviation * sqrt(-2.0 * log(u1)) * cos(2 * PI *  (double)rand()/(double)RAND_MAX);
    }while(*dest == 0.0f);
}

void loadWeights()
{
    if(nnue_weights) return;
    nnue_weights = calloc(1, sizeof(network_weights));

    FILE* input = fopen(PROJECT_CWD "/import/weights.nnue", "rb");
    if(input)
    {
        size_t size = fread(nnue_weights, sizeof(network_weights), 1, input);
        fclose(input);
        if(size == 1) return;
    }
    
    DEBUG_ERROR("Failed to load neural network from file.");

    //Biases get left at 0.0 from calloc.

    double standardDeviation = sqrt(2.0/30.0);
    
    for(int i = 0; i < HALF_INPUT_BITS; i++)
    {
        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
        {
            sampleNormalDistribution(&nnue_weights->weights1[i][j], standardDeviation);
        }
    }

    standardDeviation = sqrt(2.0 / 512.0);
    for(int i = 0; i < ACCUMULATOR_NODES; i++)
    {
        for(int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++)
        {
            sampleNormalDistribution(&nnue_weights->weights2[j][i], standardDeviation);
        }
    }

    standardDeviation = sqrt(2.0 / 32.0);
    for(int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++)
    {
        for(int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++)
        {
            sampleNormalDistribution(&nnue_weights->weights3[j][i], standardDeviation);
        }
    }

    for(int b = 0; b < OUTPUT_BUCKETS; b++)
        {
            for(int i = 0; i < THIRD_HIDDEN_LAYER_NODES; i++)
            {
                sampleNormalDistribution(&nnue_weights->weights4[b][i], standardDeviation);
            }
        }
}

void saveWeights()
{
    FILE* output = fopen(PROJECT_CWD "/import/weights.nnue", "wb");
    if(output) fwrite(nnue_weights, sizeof(network_weights), 1, output);
    else DEBUG_ERROR("Failed to write neural network to file.");
    fclose(output);
}

void print_weight_stats(const char* name, const float* data, size_t size) 
{
    assert(name);
    assert(data);
    assert(size > 0);

    float max_val = -FLT_MAX;
    float min_val = FLT_MAX;
    float abs_max = 0.0f;
    float abs_min = FLT_MAX;
    
    double mean = 0.0;
    double M2 = 0.0;
    double abs_mean = 0.0;
    double abs_M2 = 0.0;

    size_t nanCount = 0;
    size_t zeroCount = 0;
    size_t infinityCount = 0;

    for (size_t i = 0; i < size; i++) 
    {
        //Raw
        float val = data[i];

        if(isnan(val)) nanCount++;
        if(isinf(val)) infinityCount++;
        if(!val) zeroCount++;

        if(val > max_val) max_val = val;
        if(val < min_val) min_val = val;
        
        double delta = val - mean;
        mean += delta / (i + 1);
        double delta2 = val - mean;
        M2 += delta * delta2;

        //Absolute
        float abs_val = fabsf(val);

        if(abs_val > abs_max) abs_max = abs_val;
        if(abs_val < abs_min) abs_min = abs_val;

        double abs_delta = abs_val - abs_mean;
        abs_mean += abs_delta / (i + 1);
        double abs_delta2 = abs_val - abs_mean;
        abs_M2 += abs_delta * abs_delta2;
    }

    double variance = (size > 1) ? M2 / (size - 1) : 0.0;

    double abs_variance = (size > 1) ? abs_M2 / (size - 1) : 0.0;

    printf("===============================\n");
    printf("%s (Count: %" PRIu64 ")\n", name, size);
    printf("-------------------------------\n");
    printf("  Raw Min:        %11.6f | Abs Min:      %11.6f\n", min_val, abs_min);
    printf("  Raw Max:        %11.6f | Abs Max:      %11.6f\n", max_val, abs_max);
    printf("  Raw Mean:       %11.6f | Abs Mean:     %11.6f\n", mean, abs_mean);
    printf("  Raw Variance:   %11.6f | Abs Variance: %11.6f\n", variance, abs_variance);
    if(nanCount) printf("  NaNs: %" PRIu64 "\n", nanCount);
    if(zeroCount) printf("  Zeros: %" PRIu64 "\n", zeroCount);
    if(infinityCount) printf("  Infinities: %" PRIu64 "\n", infinityCount);
}

void print_network_statistics() 
{
    if(!nnue_weights) return;

    print_weight_stats("weights1", &nnue_weights->weights1[0][0], sizeof(nnue_weights->weights1) / sizeof(float));
    print_weight_stats("weights1_bias", nnue_weights->weights1_bias, sizeof(nnue_weights->weights1_bias) / sizeof(float));
    print_weight_stats("weights2", &nnue_weights->weights2[0][0], sizeof(nnue_weights->weights2) / sizeof(float));
    print_weight_stats("weights2_bias", nnue_weights->weights2_bias,sizeof(nnue_weights->weights2_bias) / sizeof(float));
    print_weight_stats("weights3", &nnue_weights->weights3[0][0], sizeof(nnue_weights->weights3) / sizeof(float));
    print_weight_stats("weights3_bias",nnue_weights->weights3_bias, sizeof(nnue_weights->weights3_bias) / sizeof(float));
    print_weight_stats("weights4", &nnue_weights->weights4[0][0], sizeof(nnue_weights->weights4) / sizeof(float));
    print_weight_stats("weights4_bias", nnue_weights->weights4_bias, sizeof(nnue_weights->weights4_bias) / sizeof(float));
}

/*** Inference ***/

void calculateHiddenLayer(float* inputValuesA, float* inputValuesB, float* outputValues, 
                            int numInputsA, int numInputsB, int numOutputs, 
                            float weights[numOutputs][numInputsA + numInputsB], float* biasWeights)
{

    __m128 zero_vec = _mm_setzero_ps();
    __m128 one_vec  = _mm_set1_ps(1.0f);

    for(int outputIndex = 0; outputIndex < numOutputs; outputIndex+=4)
    {
        __m256 v_output1 = _mm256_setzero_ps();
        __m256 v_output2 = _mm256_setzero_ps();
        __m256 v_output3 = _mm256_setzero_ps();
        __m256 v_output4 = _mm256_setzero_ps();

        for(int inputIndex = 0; inputIndex < numInputsA; inputIndex+=8)
        {
            __m256 v_inputBatch = _mm256_loadu_ps(&inputValuesA[inputIndex]);

            v_output1 = _mm256_fmadd_ps(v_inputBatch, _mm256_loadu_ps(&weights[outputIndex + 0][inputIndex]), v_output1);
            v_output2 = _mm256_fmadd_ps(v_inputBatch, _mm256_loadu_ps(&weights[outputIndex + 1][inputIndex]), v_output2);
            v_output3 = _mm256_fmadd_ps(v_inputBatch, _mm256_loadu_ps(&weights[outputIndex + 2][inputIndex]), v_output3);
            v_output4 = _mm256_fmadd_ps(v_inputBatch, _mm256_loadu_ps(&weights[outputIndex + 3][inputIndex]), v_output4);
        }

        if(inputValuesB)
        {
            for(int weightIndex = numInputsA, loadIndex = 0; weightIndex < numInputsA + numInputsB; weightIndex +=8, loadIndex+=8)
            {
                __m256 v_inputBatch = _mm256_loadu_ps(&inputValuesB[loadIndex]);

                v_output1 = _mm256_fmadd_ps(v_inputBatch, _mm256_loadu_ps(&weights[outputIndex + 0][weightIndex]), v_output1);
                v_output2 = _mm256_fmadd_ps(v_inputBatch, _mm256_loadu_ps(&weights[outputIndex + 1][weightIndex]), v_output2);
                v_output3 = _mm256_fmadd_ps(v_inputBatch, _mm256_loadu_ps(&weights[outputIndex + 2][weightIndex]), v_output3);
                v_output4 = _mm256_fmadd_ps(v_inputBatch, _mm256_loadu_ps(&weights[outputIndex + 3][weightIndex]), v_output4);
            
            }
        }

        //reduce 
        __m256 v_partial12 = _mm256_hadd_ps(v_output1, v_output2);
        __m256 v_partial34 = _mm256_hadd_ps(v_output3, v_output4);
        __m256 v_partial1234 = _mm256_hadd_ps(v_partial12, v_partial34);

        __m128 v_final_sums = _mm_add_ps(_mm256_castps256_ps128(v_partial1234), _mm256_extractf128_ps(v_partial1234, 1));
        v_final_sums = _mm_add_ps(v_final_sums, _mm_loadu_ps(&biasWeights[outputIndex]));

        //crelu
        v_final_sums = _mm_max_ps(v_final_sums, zero_vec);
        v_final_sums = _mm_min_ps(v_final_sums, one_vec);

        _mm_storeu_ps(&outputValues[outputIndex], v_final_sums);
    }
}

float calculateOutputLayer(float* h3, float weights[THIRD_HIDDEN_LAYER_NODES], float bias)
{
    __m256 v_output = _mm256_setzero_ps(); 

    for(int inputIndex = 0; inputIndex < THIRD_HIDDEN_LAYER_NODES; inputIndex+=8)
    {
        v_output = _mm256_fmadd_ps(_mm256_loadu_ps(&h3[inputIndex]), 
                                    _mm256_loadu_ps(&weights[inputIndex]), 
                                    v_output);
    }

    
    __m128 sum128 = _mm_add_ps(_mm256_castps256_ps128(v_output), _mm256_extractf128_ps(v_output, 1));
    
    //_MM_SHUFFLE() reorganizes from default indices (3, 2, 1, 0)
    // 0 and 1 come from second vector
    // 2 and 3 come from first vector
    sum128 = _mm_add_ps(sum128, _mm_shuffle_ps(sum128, sum128, _MM_SHUFFLE(2, 3, 0, 1)));
    sum128 = _mm_add_ps(sum128, _mm_shuffle_ps(sum128, sum128, _MM_SHUFFLE(1, 1, 1, 1)));

    return _mm_cvtss_f32(sum128) + bias;
}

/** PESTO (temp) **/

int mg_value[6] = { 82, 337, 365, 477, 1025,  0};
int eg_value[6] = { 94, 281, 297, 512,  936,  0};

int mg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0,
};

int eg_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

int mg_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

int eg_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

int mg_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

int eg_bishop_table[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

int mg_rook_table[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};

int eg_rook_table[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};

int mg_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

int eg_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

int mg_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

int eg_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

int* mg_pesto_table[6] =
{
    mg_pawn_table,
    mg_knight_table,
    mg_bishop_table,
    mg_rook_table,
    mg_queen_table,
    mg_king_table
};

int* eg_pesto_table[6] =
{
    eg_pawn_table,
    eg_knight_table,
    eg_bishop_table,
    eg_rook_table,
    eg_queen_table,
    eg_king_table
};

int gamephaseInc[12] = {0,0,1,1,1,1,2,2,4,4,0,0};
int mg_table[12][64];
int eg_table[12][64];

int valid = 0;
void init_tables()
{
    for (int p = 0; p < 6; p++) {
        int pc= 2 * p;
        for (int sq = 0; sq < 64; sq++) {
            mg_table[pc]  [sq] = mg_value[p] + mg_pesto_table[p][sq];
            eg_table[pc]  [sq] = eg_value[p] + eg_pesto_table[p][sq];
            mg_table[pc+1][sq] = mg_value[p] + mg_pesto_table[p][FLIP_SQUARE(sq)];
            eg_table[pc+1][sq] = eg_value[p] + eg_pesto_table[p][FLIP_SQUARE(sq)];
        }
    }
}

int eval(bitboard* board)
{
    if(!valid)
    {
        init_tables();
        valid = 1;
    }
    int mg[2];
    int eg[2];
    int gamePhase = 0;

    mg[WHITE] = 0;
    mg[BLACK] = 0;
    eg[WHITE] = 0;
    eg[BLACK] = 0;

    // evaluate each piece 
    for (int sq = 0; sq < 64; ++sq) {
        int pc = board->pieceArr[sq];
        if (pc != EMPTY_PIECE) {
            mg[COLOR(pc)] += mg_table[pc][sq];
            eg[COLOR(pc)] += eg_table[pc][sq];
            gamePhase += gamephaseInc[pc];
        }
    }

    // tapered eval
    int mgScore = mg[board->turn] - mg[FLIP_COLOR(board->turn)];
    int egScore = eg[board->turn] - eg[FLIP_COLOR(board->turn)];
    int mgPhase = gamePhase;
    if (mgPhase > 24) mgPhase = 24; // in case of early promotion 
    int egPhase = 24 - mgPhase;
    return (mgScore * mgPhase + egScore * egPhase) / 24;
}

float forwardPropagate(bitboard* board, accumulator* acc)
{
    if(1) return eval(board);
    int turn = board->turn;

    int bucket = 0;

    float* side_us;
    float* side_them;

    if(ISWHITE(turn))
    {
        side_us = acc->accumulator[WHITE];
        side_them = acc->accumulator[BLACK];
    }
    else
    {
        side_us = acc->accumulator[BLACK];
        side_them = acc->accumulator[WHITE];
    }

    float h2[SECOND_HIDDEN_LAYER_NODES];
    calculateHiddenLayer(side_us, side_them, h2, ACCUMULATOR_NODES_PER_SIDE, ACCUMULATOR_NODES_PER_SIDE, SECOND_HIDDEN_LAYER_NODES, nnue_weights->weights2, nnue_weights->weights2_bias);

    float h3[THIRD_HIDDEN_LAYER_NODES];
    calculateHiddenLayer(h2, NULL, h3, SECOND_HIDDEN_LAYER_NODES, 0, THIRD_HIDDEN_LAYER_NODES, nnue_weights->weights3, nnue_weights->weights3_bias);

    return calculateOutputLayer(h3, nnue_weights->weights4[bucket], nnue_weights->weights4_bias[bucket]);
}

/*** Training Weights ***/

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


static inline float sumLoss(float* loss)
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

void train(int maxIterations, float maxAllowedError)
{
    FILE* trainingDataFile = fopen(PROJECT_CWD "/import/trainingData.bin", "rb");
    if(!trainingDataFile)
    {
        DEBUG_ERROR("Failed to open training data file.");
        exit(EXIT_FAILURE);
    }
    fseek_64(trainingDataFile, 0, SEEK_END);
    uint64_t file_size = ftell_64(trainingDataFile);
    rewind(trainingDataFile);

    uint64_t positionCount = file_size / sizeof(CompactPosition);

    //Used to skip to random position in the .bin file.
    //The division in blockcount effectively truncates extraneous positions that don't fit within a full MINIBATCH_SIZE * MINIBATCHES_PER_SHUFFLE_BLOCK block.
    int blockCount = positionCount /  (MINIBATCH_SIZE * FEN_SKIP);
    uint64_t* blockIndices = calloc(blockCount,  sizeof(uint64_t));
    for (int i = 0; i < blockCount; i++) 
    {
        blockIndices[i] = (MINIBATCH_SIZE * FEN_SKIP) * i * sizeof(CompactPosition);
    }
    
    short* activeInputs_A = NULL;
    float* expectedOutputs_A = NULL;

    short* activeInputs_B = NULL;
    float* expectedOutputs_B = NULL;
    float* loss_buffer = NULL;
    
    int cl_errorcode = initHIP(nnue_weights, &activeInputs_A, &expectedOutputs_A, &activeInputs_B, &expectedOutputs_B, &loss_buffer);
    if(cl_errorcode != hipSuccess) 
    {
        DEBUG_ERROR("Failed to init kernels - Error Code: %d\n%s", cl_errorcode, hipGetErrorString(cl_errorcode));
        exit(EXIT_FAILURE);
    }

    int totalIterations = 0;

    float totalLoss = 0.0; //accumulated value per iteration

    clock_t startTime, endTime;
    double duration_sec;

    //Read data for next few minibatches, then shuffle data amongst them.
    CompactPosition* batchData = calloc(MINIBATCH_SIZE, sizeof(CompactPosition));
    CompactPosition* unsortedData = calloc(MINIBATCH_SIZE * FEN_SKIP, sizeof(CompactPosition));

    FILE* validationData = fopen(PROJECT_CWD  "/import/validationData.bin", "rb");

    fseek_64(validationData, 0, SEEK_END);
    int validationEntries = ftell_64(validationData) / sizeof(CompactPosition);
    validationEntries -= (validationEntries % MINIBATCH_SIZE);
    int validationBlocks = validationEntries / MINIBATCH_SIZE;
    float validationLoss = 0.0;
    rewind(validationData);

    int inputGroup = INPUT_GROUP_A;

    startTime = clock();
    for(int i = 0; i < validationBlocks; i++)
    {
        size_t size = fread(batchData, sizeof(CompactPosition), MINIBATCH_SIZE, validationData);
        if(size < MINIBATCH_SIZE) { DEBUG_ERROR("Failed reading validation Data"); continue; }
        inputGroup ^= 1;

        #pragma omp parallel
        {
            bitboard* board = create_board(NULL);
            
            #pragma omp for schedule(static)
            for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
            {
                if(inputGroup == INPUT_GROUP_A)  loadTrainingData(batchData[entryNumber], board, &expectedOutputs_A[entryNumber]);
                else loadTrainingData(batchData[entryNumber], board, &expectedOutputs_B[entryNumber]);

                uint64_t inputs[2 * TRACKED_PIECES] = {0};

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
                    for(int p = 0; p < TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                }
                if(getColumn(board->kingSquare_b) > 3)
                {
                    for(int p = TRACKED_PIECES; p < 2 * TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                }

                for(int color = 0; color < 2; color++)
                {
                    int baseIndex = (color == WHITE) ? kingBuckets[board->kingSquare_w] * BITS_PER_BUCKET : kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * BITS_PER_BUCKET;
                    
                    int trackedInputs = 0;

                    //First half matches side to move.
                    // side = 0 if color matches board->turn
                    int side = (color != board->turn);
                    
                    for(int piece = 0; piece < TRACKED_PIECES; piece++)
                    {
                        uint64_t mask = inputs[TRACKED_PIECES * color + piece];
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

        enqueueKernels(inputGroup, 0);
        hipEventSynchronize(hip_events.readLoss);


        validationLoss+=sumLoss(loss_buffer);
    }
    endTime = clock();
    duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC;
    validationLoss /= validationEntries;
    float minimumLoss = validationLoss;

    //Yellow text
    //Validation pos/sec is roughly double that of training. It doesn't have to perform backprop.
    printf("Initial Validation Loss = \033[0;33m%e\033[0m (%.1fs at %d pos/sec)\n", validationLoss, duration_sec, (int) (validationEntries / duration_sec));

    do{
        totalLoss = 0.0;

        shuffle_long(blockIndices, blockCount);
        startTime = clock();
        for(int minibatchNumber = 0; minibatchNumber < MINIBATCHES_PER_EPOCH; minibatchNumber++)
        {    

            int offset = minibatchNumber%FEN_SKIP;
            if(offset == 0)
            {
                fseek_64(trainingDataFile, blockIndices[minibatchNumber / FEN_SKIP], SEEK_SET);
                size_t size = fread(unsortedData, sizeof(CompactPosition), MINIBATCH_SIZE * FEN_SKIP, trainingDataFile);
                if(size < MINIBATCH_SIZE * FEN_SKIP) { DEBUG_ERROR("Failed reading trainingData from file."); continue; }
            }

            for(int i = 0; i < MINIBATCH_SIZE; i++)
            {
                batchData[i] = unsortedData[offset + i * FEN_SKIP];
            }

            inputGroup ^= 1;

            #pragma omp parallel
            {
                bitboard* board = create_board(NULL);
                
                #pragma omp for schedule(static)
                for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
                {
                    if(inputGroup == INPUT_GROUP_A)  loadTrainingData(batchData[entryNumber], board, &expectedOutputs_A[entryNumber]);
                    else loadTrainingData(batchData[entryNumber], board, &expectedOutputs_B[entryNumber]);

                    uint64_t inputs[2 * TRACKED_PIECES] = {0};

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
                        for(int p = 0; p < TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }
                    if(getColumn(board->kingSquare_b) > 3)
                    {
                        for(int p = TRACKED_PIECES; p < 2 * TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }


                    for(int color = 0; color < 2; color++)
                    {
                        int baseIndex = (color == 0) ? kingBuckets[board->kingSquare_w] * BITS_PER_BUCKET : kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * BITS_PER_BUCKET;

                        int trackedInputs = 0;

                        //First half matches side to move.
                        // side = 0 if color matches board->turn
                        int side = (color != board->turn);
                        
                        for(int piece = 0; piece < TRACKED_PIECES; piece++)
                        {
                            uint64_t mask = inputs[TRACKED_PIECES * color + piece];
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

            enqueueKernels(inputGroup, 1);
            hipEventSynchronize(hip_events.readLoss);
            
            float loss = sumLoss(loss_buffer);
            totalLoss+=loss;

            if((minibatchNumber + 1) % 10 == 0) printf("\33[2K\r\tAnalyzed block %d/%d; Loss = %e", minibatchNumber + 1, MINIBATCHES_PER_EPOCH, loss / MINIBATCH_SIZE);
        }
        totalIterations++;
        endTime = clock();
        duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC; 

        //If we comment out hipEventSynchronize, we end up with 17.4 million positions/second.
        //Commenting out enqueueKernels as well nets us 18.4 million positions/second.
        //This is the upper limit bounded by the CPU I/O, and it is far above the current GPU-bound pos/sec.
        printf("\33[2K\rEpoch %d Loss = %e (%.1fs at %d pos/sec)", totalIterations, totalLoss/(MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH), duration_sec, (int) ((MINIBATCH_SIZE * MINIBATCHES_PER_EPOCH) / duration_sec));

        //Validation
        rewind(validationData);
        startTime = clock();
        for(int i = 0; i < validationBlocks; i++)
        {
            size_t size = fread(batchData, sizeof(CompactPosition), MINIBATCH_SIZE, validationData);
            if(size < MINIBATCH_SIZE) { DEBUG_ERROR("Failed reading validation Data"); continue; }
            inputGroup ^= 1;

            #pragma omp parallel
            {
                bitboard* board = create_board(NULL);
                
                #pragma omp for schedule(static)
                for(int entryNumber = 0; entryNumber < MINIBATCH_SIZE; entryNumber++)
                {
                    if(inputGroup == INPUT_GROUP_A)  loadTrainingData(batchData[entryNumber], board, &expectedOutputs_A[entryNumber]);
                    else loadTrainingData(batchData[entryNumber], board, &expectedOutputs_B[entryNumber]);

                    uint64_t inputs[2 * TRACKED_PIECES] = {0};

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
                        for(int p = 0; p < TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }
                    if(getColumn(board->kingSquare_b) > 3)
                    {
                        for(int p = TRACKED_PIECES; p < 2 * TRACKED_PIECES; p++) inputs[p] = mirrorBoard(inputs[p]);
                    }

                    for(int color = 0; color < 2; color++)
                    {
                        int baseIndex = (color == WHITE) ? kingBuckets[board->kingSquare_w] * BITS_PER_BUCKET : kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * BITS_PER_BUCKET;

                        int trackedInputs = 0;

                        //First half matches side to move.
                        // side = 0 if color matches board->turn
                        int side = (color != board->turn);
                        
                        for(int piece = 0; piece < TRACKED_PIECES; piece++)
                        {
                            uint64_t mask = inputs[TRACKED_PIECES * color + piece];
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
        
            enqueueKernels(inputGroup, 0);
            hipEventSynchronize(hip_events.readLoss);

            validationLoss+=sumLoss(loss_buffer);
        }
        
        endTime = clock();
        duration_sec = (double) (endTime - startTime) / CLOCKS_PER_SEC;

        validationLoss /= validationEntries;

        if(validationLoss < minimumLoss)
        {
            getWeights(nnue_weights);
            saveWeights();
            minimumLoss = validationLoss;

            //Green text
            printf("; Validation Loss = \033[0;32m%e\033[0m", validationLoss);
        }
        else 
        {
            //Red text
            printf("; Validation Loss = \033[0;31m%e\033[0m", validationLoss);
        }
        printf(" (%.1fs at %d pos/sec)\n", duration_sec, (int) (validationEntries / duration_sec));
    } while(validationLoss > maxAllowedError && totalIterations < maxIterations);

    
    fclose(trainingDataFile);
    fclose(validationData);
    free(blockIndices);
    free(batchData);
    free(unsortedData);

    freeHIP();
}