#include "neuralnet.h"
#include "../debug.h"
#include "../board/bitboard.h"
#include "../board/moves.h"
#include "../structs.h"
#include "engine.h"
#include <math.h>
#include <float.h>
#include <immintrin.h>

accumulator_training* trainingAccumulator = NULL;
accumulator_playing* playerAccumulator = NULL;
accumulator_training_refreshTable* trainingRefreshTable = NULL;
accumulator_playing_refreshTable* playingRefreshTable = NULL;

/**
 * - Index X contains the king bucket index at square X.
 * - Created for white's perspective; must be flipped for black.
 * - Mirrored accross middle of board, split between d/e files.
 *      - When king column > d, the squares of all pieces ^= 7  &  masks get mirrored.
 * 
 * topleft: a1
 * bottomright: h8
 */
int kingBuckets[64] = {
     0,  1,  2,  3,      3,  2,  1,  0,
     4,  5,  6,  7,      7,  6,  5,  4,
     8,  8,  9,  9,      9,  9,  8,  8,
    10, 10, 11, 11,     11, 11, 10, 10,
    12, 12, 13, 13,     13, 13, 12, 12,
    12, 12, 13, 13,     13, 13, 12, 12,
    14, 14, 15, 15,     15, 15, 14, 14,
    14, 14, 15, 15,     15, 15, 14, 14
};

//Contains example squares that a king in bucket X can exist on.
int kingBucketMap[KING_BUCKETS] = {
     0, 1,  2,  3, 
     8, 9, 10, 11,
    16, 18,
    24, 26,
    32, 34,
    48, 52
};


//https://www.chessprogramming.org/Flipping_Mirroring_and_Rotating#Vertical
#define k1 0x5555555555555555
#define k2 0x3333333333333333
#define k4 0x0f0f0f0f0f0f0f0f
static inline uint64_t mirrorBoard(uint64_t x)
{
   x = ((x >> 1) & k1) | ((x & k1) << 1);
   x = ((x >> 2) & k2) | ((x & k2) << 2);
   x = ((x >> 4) & k4) | ((x & k4) << 4);
   return x;
}

void extractInputLayerToArray(uint64_t* inputLayerCompact_w, uint64_t* inputLayerCompact_b, void* output, int outputType)
{
    assert(inputLayerCompact_w);
    assert(inputLayerCompact_b);
    assert(output);
    assert(outputType == TRAINING || outputType == PLAYING);
    
    if(outputType == TRAINING)
    {
        float* inputLayerFloats = (float*) output;
        for(int i = 0; i < 640; i++)
        {
            if((inputLayerCompact_w[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerFloats[i] = 1.0;
            else inputLayerFloats[i] = 0.0;

            if((inputLayerCompact_b[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerFloats[640 + i] = 1.0;
            else inputLayerFloats[640 + i] = 0.0;
        }
    }
    else if(outputType == PLAYING)
    {
        int8_t* inputLayerBytes = (int8_t*) output;
        for(int i = 0; i < 640; i++)
        {
            if((inputLayerCompact_w[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerBytes[i] = 1;
            else inputLayerBytes[i] = 0;

            if((inputLayerCompact_b[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerBytes[640 + i] = 1;
            else inputLayerBytes[640 + i] = 0;
        }
    }
}

void loadInputAccumulator(bitboard* board, void* accumulator, int accumulatorType, int color)
{
    assert(board);
    assert(accumulator);
    assert(accumulatorType == TRAINING || accumulatorType == PLAYING);
    
    if(accumulatorType == PLAYING && !playerNNUE) load_playingWeights();
    else if(accumulatorType == TRAINING && !trainingNNUE) load_trainingWeights();
    
    accumulator_playing* byteAccumulator = NULL;
    accumulator_training* floatAccumulator = NULL;
    uint64_t* inputs = NULL;
    if(accumulatorType == PLAYING) 
    {
        byteAccumulator = (accumulator_playing*) accumulator;
        inputs = byteAccumulator->inputNodes;
    }
    else 
    {
        floatAccumulator = (accumulator_training*) accumulator;
        inputs = floatAccumulator->inputNodes;
    }

    memset(inputs, 0, 320*sizeof(uint64_t));

    int baseIndex_w = 10*kingBuckets[board->kingSquare_w];
    int baseIndex_b = BITBOARDS_PER_INPUT_SIDE + 10*kingBuckets[FLIP_SQUARE(board->kingSquare_b)];

    int extendedBaseIndex_w = (kingBuckets[board->kingSquare_w] * 640);
    int extendedBaseIndex_b = kingBuckets[FLIP_SQUARE(board->kingSquare_b)] * 640;

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

    inputs[baseIndex_b + 0] = FLIP_MASK(board->pawn_b);
    inputs[baseIndex_b + 1] = FLIP_MASK(board->knight_b);
    inputs[baseIndex_b + 2] = FLIP_MASK(board->bishop_b);
    inputs[baseIndex_b + 3] = FLIP_MASK(board->rook_b);
    inputs[baseIndex_b + 4] = FLIP_MASK(board->queen_b);
    inputs[baseIndex_b + 5] = FLIP_MASK(board->pawn_w);
    inputs[baseIndex_b + 6] = FLIP_MASK(board->knight_w);
    inputs[baseIndex_b + 7] = FLIP_MASK(board->bishop_w);
    inputs[baseIndex_b + 8] = FLIP_MASK(board->rook_w);
    inputs[baseIndex_b + 9] = FLIP_MASK(board->queen_w);

    if(getColumn(board->kingSquare_w) > 4)
    {
        inputs[baseIndex_w + 0] = mirrorBoard(inputs[baseIndex_w + 0]);
        inputs[baseIndex_w + 1] = mirrorBoard(inputs[baseIndex_w + 1]);
        inputs[baseIndex_w + 2] = mirrorBoard(inputs[baseIndex_w + 2]);
        inputs[baseIndex_w + 3] = mirrorBoard(inputs[baseIndex_w + 3]);
        inputs[baseIndex_w + 4] = mirrorBoard(inputs[baseIndex_w + 4]);
        inputs[baseIndex_w + 5] = mirrorBoard(inputs[baseIndex_w + 5]);
        inputs[baseIndex_w + 6] = mirrorBoard(inputs[baseIndex_w + 6]);
        inputs[baseIndex_w + 7] = mirrorBoard(inputs[baseIndex_w + 7]);
        inputs[baseIndex_w + 8] = mirrorBoard(inputs[baseIndex_w + 8]);
        inputs[baseIndex_w + 9] = mirrorBoard(inputs[baseIndex_w + 9]);
    }
    if(getColumn(board->kingSquare_b) > 4)
    {
        inputs[baseIndex_b + 0] = mirrorBoard(inputs[baseIndex_b + 0]);
        inputs[baseIndex_b + 1] = mirrorBoard(inputs[baseIndex_b + 1]);
        inputs[baseIndex_b + 2] = mirrorBoard(inputs[baseIndex_b + 2]);
        inputs[baseIndex_b + 3] = mirrorBoard(inputs[baseIndex_b + 3]);
        inputs[baseIndex_b + 4] = mirrorBoard(inputs[baseIndex_b + 4]);
        inputs[baseIndex_b + 5] = mirrorBoard(inputs[baseIndex_b + 5]);
        inputs[baseIndex_b + 6] = mirrorBoard(inputs[baseIndex_b + 6]);
        inputs[baseIndex_b + 7] = mirrorBoard(inputs[baseIndex_b + 7]);
        inputs[baseIndex_b + 8] = mirrorBoard(inputs[baseIndex_b + 8]);
        inputs[baseIndex_b + 9] = mirrorBoard(inputs[baseIndex_b + 9]);
    }

    if(byteAccumulator)
    {
        int8_t inputArray[1280];
        extractInputLayerToArray(&inputs[baseIndex_w], &inputs[baseIndex_b], inputArray, PLAYING);

        __m256i v_min = _mm256_setzero_si256();
        __m256i v_max = _mm256_set1_epi8(127);
        if(ISWHITE(color)) 
        {
            calculateLayer_IntBytes(inputArray, byteAccumulator->rawAccumulator[0], 640, ACCUMULATOR_NODES_PER_SIDE, playerNNUE->weights1, extendedBaseIndex_w, playerNNUE->weights1_bias, 0);
            memcpy(&byteAccumulator->accumulator[0], &byteAccumulator->rawAccumulator[0], ACCUMULATOR_NODES_PER_SIDE * sizeof(char));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=32) SIMD_SCReLU(&byteAccumulator->accumulator[0][i], v_min, v_max);
        }
        if(ISBLACK(color)) 
        {
            calculateLayer_IntBytes(&inputArray[640], byteAccumulator->rawAccumulator[1], 640, ACCUMULATOR_NODES_PER_SIDE, playerNNUE->weights1, extendedBaseIndex_b, playerNNUE->weights1_bias, 0);
            memcpy(&byteAccumulator->accumulator[1], &byteAccumulator->rawAccumulator[1], ACCUMULATOR_NODES_PER_SIDE * sizeof(char));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=32) SIMD_SCReLU(&byteAccumulator->accumulator[1][i], v_min, v_max);
        }
    }
    else
    {
        float inputArray[1280];
        extractInputLayerToArray(&inputs[baseIndex_w], &inputs[baseIndex_b], inputArray, TRAINING);

        __m256 v_min = _mm256_setzero_ps();
        __m256 v_max = _mm256_set1_ps(1.0);
        __m256 v_grad = _mm256_set1_ps(LEAK_FACTOR);
        if(ISWHITE(color)) 
        {
            calculateLayer_Floats(inputArray, floatAccumulator->rawAccumulator[0], 640, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights1, extendedBaseIndex_w, trainingNNUE->weights1_bias, 0);
            memcpy(&floatAccumulator->accumulator[0], &floatAccumulator->rawAccumulator[0], ACCUMULATOR_NODES_PER_SIDE * sizeof(float));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=8) SIMD_SCReLU_Float(&floatAccumulator->accumulator[0][i], v_min, v_max, v_grad);
        }
        if(ISBLACK(color)) 
        {
            calculateLayer_Floats(&inputArray[640], floatAccumulator->rawAccumulator[1], 640, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights1, extendedBaseIndex_b, trainingNNUE->weights1_bias, 0);
            memcpy(&floatAccumulator->accumulator[1], &floatAccumulator->rawAccumulator[1], ACCUMULATOR_NODES_PER_SIDE * sizeof(float));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=8) SIMD_SCReLU_Float(&floatAccumulator->accumulator[1][i], v_min, v_max, v_grad);
        }
    }
}

void updateBoardAccumulator(bitboard* currentBoard, bitboard* accumulatorBoard, void* accumulator, int accumulatorType, int color)
{
    assert(currentBoard);
    assert(accumulatorBoard);
    assert(accumulator);
    assert(accumulatorType == TRAINING || accumulatorType == PLAYING);
    assert(((ISBLACK(color) && kingBuckets[currentBoard->kingSquare_b] == kingBuckets[accumulatorBoard->kingSquare_b]) || 
                (ISWHITE(color) && kingBuckets[currentBoard->kingSquare_w] == kingBuckets[accumulatorBoard->kingSquare_w])) && 
            "Bitboards must have matching king square buckets.");
    assert((accumulatorType == PLAYING && playerNNUE) || (accumulatorType == TRAINING && trainingNNUE));

    accumulator_playing* byteAccumulator = NULL;
    accumulator_training* floatAccumulator = NULL;
    if(accumulatorType == PLAYING) byteAccumulator = (accumulator_playing*) accumulator;
    else floatAccumulator = (accumulator_training*) accumulator;

    uint64_t curBoard[10] = {0};
    uint64_t accumBoard[10] = {0};

    int ksq;
    if(ISWHITE(color))
    {
        ksq = accumulatorBoard->kingSquare_w;

        curBoard[0] = currentBoard->pawn_w;
        curBoard[1] = currentBoard->knight_w;
        curBoard[2] = currentBoard->bishop_w;
        curBoard[3] = currentBoard->rook_w;
        curBoard[4] = currentBoard->queen_w;
        curBoard[5] = currentBoard->pawn_b;
        curBoard[6] = currentBoard->knight_b;
        curBoard[7] = currentBoard->bishop_b;
        curBoard[8] = currentBoard->rook_b;
        curBoard[9] = currentBoard->queen_b;

        accumBoard[0] = accumulatorBoard->pawn_w;
        accumBoard[1] = accumulatorBoard->knight_w;
        accumBoard[2] = accumulatorBoard->bishop_w;
        accumBoard[3] = accumulatorBoard->rook_w;
        accumBoard[4] = accumulatorBoard->queen_w;
        accumBoard[5] = accumulatorBoard->pawn_b;
        accumBoard[6] = accumulatorBoard->knight_b;
        accumBoard[7] = accumulatorBoard->bishop_b;
        accumBoard[8] = accumulatorBoard->rook_b;
        accumBoard[9] = accumulatorBoard->queen_b;
    }
    else
    {
        ksq = FLIP_SQUARE(accumulatorBoard->kingSquare_b);

        curBoard[0] = FLIP_MASK(currentBoard->pawn_b);
        curBoard[1] = FLIP_MASK(currentBoard->knight_b);
        curBoard[2] = FLIP_MASK(currentBoard->bishop_b);
        curBoard[3] = FLIP_MASK(currentBoard->rook_b);
        curBoard[4] = FLIP_MASK(currentBoard->queen_b);
        curBoard[5] = FLIP_MASK(currentBoard->pawn_w);
        curBoard[6] = FLIP_MASK(currentBoard->knight_w);
        curBoard[7] = FLIP_MASK(currentBoard->bishop_w);
        curBoard[8] = FLIP_MASK(currentBoard->rook_w);
        curBoard[9] = FLIP_MASK(currentBoard->queen_w);

        accumBoard[0] = FLIP_MASK(accumulatorBoard->pawn_b);
        accumBoard[1] = FLIP_MASK(accumulatorBoard->knight_b);
        accumBoard[2] = FLIP_MASK(accumulatorBoard->bishop_b);
        accumBoard[3] = FLIP_MASK(accumulatorBoard->rook_b);
        accumBoard[4] = FLIP_MASK(accumulatorBoard->queen_b);
        accumBoard[5] = FLIP_MASK(accumulatorBoard->pawn_w);
        accumBoard[6] = FLIP_MASK(accumulatorBoard->knight_w);
        accumBoard[7] = FLIP_MASK(accumulatorBoard->bishop_w);
        accumBoard[8] = FLIP_MASK(accumulatorBoard->rook_w);
        accumBoard[9] = FLIP_MASK(accumulatorBoard->queen_w);
    }
    if(getColumn(ksq) > 4)
    {
        for(int i = 0; i < 10; i++)
        {
            curBoard[i] = mirrorBoard(curBoard[i]);
            accumBoard[i] = mirrorBoard(accumBoard[i]);
        }
    }

    for(int piece = 0; piece < 10; piece++)
    {
        uint64_t difference = curBoard[piece]^accumBoard[piece];

        //Input node updates
        int inputNodeIndex = (10 * kingBuckets[ksq]) + piece;
        if(ISBLACK(color)) inputNodeIndex+=BITBOARDS_PER_INPUT_SIDE;

        if(accumulatorType == PLAYING) byteAccumulator->inputNodes[inputNodeIndex]^=difference;
        else floatAccumulator->inputNodes[inputNodeIndex]^=difference;

        while(difference)
        {
            int square = __builtin_ctzll(difference);

            int featureIndex_Black = (64 * (10 * kingBuckets[ksq] + piece)) + square;
            int featureIndex_White = (64 * (10 * kingBuckets[ksq] + piece)) + square;

            char sign = curBoard[piece]&(1ull << square) ? 1 : -1;

            if(byteAccumulator)
            {
                
                if(ISWHITE(color))
                {
                    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
                    {
                        byteAccumulator->rawAccumulator[0][i] += playerNNUE->weights1[i][featureIndex_White] * sign;
                    }
                }
                if(ISBLACK(color))
                {
                    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
                    {
                        byteAccumulator->rawAccumulator[1][i] += playerNNUE->weights1[i][featureIndex_Black] * sign;
                    }
                }
            }
            else
            {

                if(ISWHITE(color))
                {
                    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
                    {
                        floatAccumulator->rawAccumulator[0][i] += trainingNNUE->weights1[i][featureIndex_White] * sign;
                    }
                }
                if(ISBLACK(color))
                {
                    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
                    {
                        floatAccumulator->rawAccumulator[1][i] += trainingNNUE->weights1[i][featureIndex_Black] * sign;
                    }
                }
            }

            difference&=(difference - 1);
        }
    }

    //Store activated values.
    if(byteAccumulator)
    {
        __m256i v_min = _mm256_setzero_si256();
        __m256i v_max = _mm256_set1_epi8(127);
        if(ISWHITE(color))
        {
            memcpy(&byteAccumulator->accumulator[0], &byteAccumulator->rawAccumulator[0], ACCUMULATOR_NODES_PER_SIDE * sizeof(char));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=32) SIMD_SCReLU(&byteAccumulator->accumulator[0][i], v_min, v_max);
        }
        if(ISBLACK(color))
        {
            memcpy(&byteAccumulator->accumulator[1], &byteAccumulator->rawAccumulator[1], ACCUMULATOR_NODES_PER_SIDE * sizeof(char));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=32) SIMD_SCReLU(&byteAccumulator->accumulator[1][i], v_min, v_max);
        }
    }
    else if(floatAccumulator)
    {
        __m256 v_min = _mm256_setzero_ps();
        __m256 v_max = _mm256_set1_ps(1.0);
        __m256 v_grad = _mm256_set1_ps(LEAK_FACTOR);
        if(ISWHITE(color))
        {
            memcpy(&floatAccumulator->accumulator[0], &floatAccumulator->rawAccumulator[0], ACCUMULATOR_NODES_PER_SIDE * sizeof(float));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=8) SIMD_SCReLU_Float(&floatAccumulator->accumulator[0][i], v_min, v_max, v_grad);
        }
        if(ISBLACK(color))
        {
            memcpy(&floatAccumulator->accumulator[1], &floatAccumulator->rawAccumulator[1], ACCUMULATOR_NODES_PER_SIDE * sizeof(float));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=8) SIMD_SCReLU_Float(&floatAccumulator->accumulator[1][i], v_min, v_max, v_grad);
        }
    }
    

    copy_board(accumulatorBoard, currentBoard, 0, 0);
}

void updateAccumulatorFromTable(bitboard* board, void* accumulator, void* refreshTable, int accumulatorType)
{
    assert(board);
    assert(accumulator);
    assert(refreshTable);
    assert(accumulatorType == TRAINING || accumulatorType == PLAYING);

    int kingBucket_w = kingBuckets[board->kingSquare_w];
    int kingBucket_b = kingBuckets[FLIP_SQUARE(board->kingSquare_b)]; 

    if(accumulatorType == PLAYING) 
    {
        accumulator_playing_refreshTable* byteTable = (accumulator_playing_refreshTable*) refreshTable;
        
        //Uninitialized bitboard in accumulator refresh table.
        if(byteTable->boards[0][kingBucket_w]->kingSquare_w == 0 && byteTable->boards[0][kingBucket_w]->kingSquare_b == 0)
        {
            copy_board(byteTable->boards[0][kingBucket_w], board, 0, 0);
            loadInputAccumulator(byteTable->boards[0][kingBucket_w], &byteTable->accumulators[kingBucket_w], accumulatorType, WHITE);
        }
        if(byteTable->boards[1][kingBucket_b]->kingSquare_w == 0 && byteTable->boards[1][kingBucket_b]->kingSquare_b == 0)
        {
            copy_board(byteTable->boards[1][kingBucket_b], board, 0, 0);
            loadInputAccumulator(byteTable->boards[1][kingBucket_b], &byteTable->accumulators[kingBucket_b], accumulatorType, BLACK);
        }

        updateBoardAccumulator(board, byteTable->boards[0][kingBucket_w], &byteTable->accumulators[kingBucket_w], accumulatorType, WHITE);
        updateBoardAccumulator(board, byteTable->boards[1][kingBucket_b], &byteTable->accumulators[kingBucket_b], accumulatorType, BLACK);
        
        accumulator_playing* acc = (accumulator_playing*) accumulator;
        memcpy(acc->accumulator[0], byteTable->accumulators[kingBucket_w].accumulator[0], sizeof(int8_t) * ACCUMULATOR_NODES_PER_SIDE);
        memcpy(acc->accumulator[1], byteTable->accumulators[kingBucket_b].accumulator[1], sizeof(int8_t) * ACCUMULATOR_NODES_PER_SIDE);
    }
    else 
    {
        accumulator_training_refreshTable* floatTable = (accumulator_training_refreshTable*) refreshTable;

        //Uninitialized bitboard in accumulator refresh table.
        if(floatTable->boards[0][kingBucket_w]->kingSquare_w == 0 && floatTable->boards[0][kingBucket_w]->kingSquare_b == 0)
        {
            copy_board(floatTable->boards[0][kingBucket_w], board, 0, 0);
            loadInputAccumulator(floatTable->boards[0][kingBucket_w], &floatTable->accumulators[kingBucket_w], accumulatorType, WHITE);
        }
        if(floatTable->boards[1][kingBucket_b]->kingSquare_w == 0 && floatTable->boards[1][kingBucket_b]->kingSquare_b == 0)
        {
            copy_board(floatTable->boards[1][kingBucket_b], board, 0, 0);
            loadInputAccumulator(floatTable->boards[1][kingBucket_b], &floatTable->accumulators[kingBucket_b], accumulatorType, BLACK);
        }

        updateBoardAccumulator(board, floatTable->boards[0][kingBucket_w], &floatTable->accumulators[kingBucket_w], accumulatorType, WHITE);
        updateBoardAccumulator(board, floatTable->boards[1][kingBucket_b], &floatTable->accumulators[kingBucket_b], accumulatorType, BLACK);
        
        accumulator_training* acc = (accumulator_training*) accumulator;
        memcpy(acc->accumulator[0], floatTable->accumulators[kingBucket_w].accumulator[0], sizeof(float) * ACCUMULATOR_NODES_PER_SIDE);
        memcpy(acc->accumulator[1], floatTable->accumulators[kingBucket_b].accumulator[1], sizeof(float) * ACCUMULATOR_NODES_PER_SIDE);
    }
}

accumulator_playing_refreshTable* createPlayingRefreshTable()
{
    accumulator_playing_refreshTable* table = CALLOC(1, sizeof(accumulator_playing_refreshTable));
    for(int i = 0; i < 2; i++)
    {
        int color = (i == 0) ? WHITE : BLACK;
        for(int j = 0; j < KING_BUCKETS; j++)
        {
            int newKingSquare = kingBucketMap[j];
            table->boards[i][j] = create_board();
            if(color == WHITE) 
            {
                board_clear_square(table->boards[i][j], table->boards[i][j]->kingSquare_w, KING|WHITE);
                table->boards[i][j]->kingSquare_w = newKingSquare;
            }
            else
            {   
                board_clear_square(table->boards[i][j], table->boards[i][j]->kingSquare_b, KING|BLACK);
                table->boards[i][j]->kingSquare_b = newKingSquare;
            }

            board_set(table->boards[i][j], newKingSquare, KING|color);
        }
    }
    return table;
}

accumulator_training_refreshTable* createTrainingRefreshTable()
{
    accumulator_training_refreshTable* table = CALLOC(1, sizeof(accumulator_training_refreshTable));
    for(int i = 0; i < 2; i++)
    {
        int color = (i == 0) ? WHITE : BLACK;
        for(int j = 0; j < KING_BUCKETS; j++)
        {
            int newKingSquare = kingBucketMap[j];
            table->boards[i][j] = create_board();
            if(color == WHITE) 
            {
                board_clear_square(table->boards[i][j], table->boards[i][j]->kingSquare_w, KING|WHITE);
                table->boards[i][j]->kingSquare_w = newKingSquare;
            }
            else
            {   
                board_clear_square(table->boards[i][j], table->boards[i][j]->kingSquare_b, KING|BLACK);
                table->boards[i][j]->kingSquare_b = newKingSquare;
            }

            board_set(table->boards[i][j], newKingSquare, KING|color);
        }
    }
    return table;
}

void destroyRefreshTable(void* table, int accumulatorType)
{
    assert(table);
    assert(accumulatorType == TRAINING || accumulatorType == PLAYING);

    if(accumulatorType == TRAINING)
    {
        accumulator_training_refreshTable* trainingTable = (accumulator_training_refreshTable*) table;
        for(int i = 0; i < 2; i++)
        {
            for(int j = 0; j < KING_BUCKETS; j++)
            {
                destroy_board(trainingTable->boards[i][j]);
            }
        }
        FREE(trainingTable);
    }
    else
    {
        accumulator_playing_refreshTable* playingTable = (accumulator_playing_refreshTable*) table;
        for(int i = 0; i < 2; i++)
        {
            for(int j = 0; j < KING_BUCKETS; j++)
            {
                destroy_board(playingTable->boards[i][j]);
            }
        }
        FREE(playingTable);
    }

    table = NULL;
}