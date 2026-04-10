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

    memset(inputs, 0, 1280*sizeof(uint64_t));

    int baseIndex_w = 10*board->kingSquare_w;
    int baseIndex_b = 640 + 10*FLIP_SQUARE(board->kingSquare_b);

/**
 * To find the index given COLOR's KINGSQUARE and 
 * PIECE's PIECE_SQUARE:
 * 
 * index = (40960 * ISBLACK(COLOR)) + (KINGSQUARE * 640) + 64 * PIECE + PIECE_SQUARE
 * 
 * Black pieces are flipped to be viewed from white's perspective.
 */
    int extendedBaseIndex_w = (board->kingSquare_w * 640);
    int extendedBaseIndex_b = (FLIP_SQUARE(board->kingSquare_b) * 640);

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
            memcpy(&byteAccumulator->accumulator[1], &byteAccumulator->rawAccumulator[1], ACCUMULATOR_NODES_PER_SIDE * sizeof(float));
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
    assert(((ISBLACK(color) && currentBoard->kingSquare_b == accumulatorBoard->kingSquare_b) || 
                (ISWHITE(color) && currentBoard->kingSquare_w == accumulatorBoard->kingSquare_w)) && 
            "Bitboards must have matching king squares");
    assert((accumulatorType == PLAYING && playerNNUE) || (accumulatorType == TRAINING && trainingNNUE));

    accumulator_playing* byteAccumulator = NULL;
    accumulator_training* floatAccumulator = NULL;
    if(accumulatorType == PLAYING) byteAccumulator = (accumulator_playing*) accumulator;
    else floatAccumulator = (accumulator_training*) accumulator;

    uint64_t curBoard[10] = {currentBoard->pawn_w, currentBoard->knight_w, currentBoard->bishop_w, currentBoard->rook_w, currentBoard->queen_w,
                             currentBoard->pawn_b, currentBoard->knight_b, currentBoard->bishop_b, currentBoard->rook_b, currentBoard->queen_b};

    uint64_t accumBoard[10] = {accumulatorBoard->pawn_w, accumulatorBoard->knight_w, accumulatorBoard->bishop_w, accumulatorBoard->rook_w, accumulatorBoard->queen_w,
                             accumulatorBoard->pawn_b, accumulatorBoard->knight_b, accumulatorBoard->bishop_b, accumulatorBoard->rook_b, accumulatorBoard->queen_b};

    for(int piece = 0; piece < 10; piece++)
    {
        uint64_t difference = curBoard[piece]^accumBoard[piece];

        //Input node updates
        int ksq = ISWHITE(color) ? accumulatorBoard->kingSquare_w : accumulatorBoard->kingSquare_b;
        int inputNodeIndex = (10 * ksq) + piece;
        if(ISBLACK(color)) inputNodeIndex+=640;

        if(accumulatorType == PLAYING) byteAccumulator->inputNodes[inputNodeIndex]^=difference;
        else floatAccumulator->inputNodes[inputNodeIndex]^=difference;

        while(difference)
        {
            int square = __builtin_ctzll(difference);

            int featureIndex_Black = (64 * (10 * accumulatorBoard->kingSquare_b + piece)) + square;
            int featureIndex_White = (64 * (10 * accumulatorBoard->kingSquare_w + piece)) + square;

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

    if(accumulatorType == PLAYING) 
    {
        accumulator_playing_refreshTable* byteTable = (accumulator_playing_refreshTable*) refreshTable;
        
        //Uninitialized bitboard in accumulator refresh table.
        if(byteTable->boards[0][board->kingSquare_w]->kingSquare_w == 0 && byteTable->boards[0][board->kingSquare_w]->kingSquare_b == 0)
        {
            copy_board(byteTable->boards[0][board->kingSquare_w], board, 0, 0);
            loadInputAccumulator(byteTable->boards[0][board->kingSquare_w], &byteTable->accumulators[board->kingSquare_w], accumulatorType, WHITE);
        }
        if(byteTable->boards[1][board->kingSquare_b]->kingSquare_w == 0 && byteTable->boards[1][board->kingSquare_b]->kingSquare_b == 0)
        {
            copy_board(byteTable->boards[1][board->kingSquare_b], board, 0, 0);
            loadInputAccumulator(byteTable->boards[1][board->kingSquare_b], &byteTable->accumulators[board->kingSquare_b], accumulatorType, BLACK);
        }

        updateBoardAccumulator(board, byteTable->boards[0][board->kingSquare_w], &byteTable->accumulators[board->kingSquare_w], accumulatorType, WHITE);
        updateBoardAccumulator(board, byteTable->boards[1][board->kingSquare_b], &byteTable->accumulators[board->kingSquare_b], accumulatorType, BLACK);
        
        accumulator_playing* acc = (accumulator_playing*) accumulator;
        memcpy(acc->accumulator[0], byteTable->accumulators[board->kingSquare_w].accumulator[0], sizeof(int8_t) * ACCUMULATOR_NODES_PER_SIDE);
        memcpy(acc->accumulator[1], byteTable->accumulators[board->kingSquare_b].accumulator[1], sizeof(int8_t) * ACCUMULATOR_NODES_PER_SIDE);
    }
    else 
    {
        accumulator_training_refreshTable* floatTable = (accumulator_training_refreshTable*) refreshTable;

        //Uninitialized bitboard in accumulator refresh table.
        if(floatTable->boards[0][board->kingSquare_w]->kingSquare_w == 0 && floatTable->boards[0][board->kingSquare_w]->kingSquare_b == 0)
        {
            copy_board(floatTable->boards[0][board->kingSquare_w], board, 0, 0);
            loadInputAccumulator(floatTable->boards[0][board->kingSquare_w], &floatTable->accumulators[board->kingSquare_w], accumulatorType, WHITE);
        }
        if(floatTable->boards[1][board->kingSquare_b]->kingSquare_w == 0 && floatTable->boards[1][board->kingSquare_b]->kingSquare_b == 0)
        {
            copy_board(floatTable->boards[1][board->kingSquare_b], board, 0, 0);
            loadInputAccumulator(floatTable->boards[1][board->kingSquare_b], &floatTable->accumulators[board->kingSquare_b], accumulatorType, BLACK);
        }

        updateBoardAccumulator(board, floatTable->boards[0][board->kingSquare_w], &floatTable->accumulators[board->kingSquare_w], accumulatorType, WHITE);
        updateBoardAccumulator(board, floatTable->boards[1][board->kingSquare_b], &floatTable->accumulators[board->kingSquare_b], accumulatorType, BLACK);
        
        accumulator_training* acc = (accumulator_training*) accumulator;
        memcpy(acc->accumulator[0], floatTable->accumulators[board->kingSquare_w].accumulator[0], sizeof(float) * ACCUMULATOR_NODES_PER_SIDE);
        memcpy(acc->accumulator[1], floatTable->accumulators[board->kingSquare_b].accumulator[1], sizeof(float) * ACCUMULATOR_NODES_PER_SIDE);
    }
}

accumulator_playing_refreshTable* createPlayingRefreshTable()
{
    accumulator_playing_refreshTable* table = CALLOC(1, sizeof(accumulator_playing_refreshTable));
    for(int i = 0; i < 2; i++)
    {
        int color = (i == 0) ? WHITE : BLACK;
        for(int j = 0; j < 64; j++)
        {
            table->boards[i][j] = create_board();
            if(color == WHITE) 
            {
                board_clear_square(table->boards[i][j], table->boards[i][j]->kingSquare_w, KING|WHITE);
                table->boards[i][j]->kingSquare_w = j;
            }
            else
            {   
                board_clear_square(table->boards[i][j], table->boards[i][j]->kingSquare_b, KING|BLACK);
                table->boards[i][j]->kingSquare_b = j;
            }

            board_set(table->boards[i][j], j, KING|color);
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
        for(int j = 0; j < 64; j++)
        {
            table->boards[i][j] = create_board();
            if(color == WHITE) 
            {
                board_clear_square(table->boards[i][j], table->boards[i][j]->kingSquare_w, KING|WHITE);
                table->boards[i][j]->kingSquare_w = j;
            }
            else
            {   
                board_clear_square(table->boards[i][j], table->boards[i][j]->kingSquare_b, KING|BLACK);
                table->boards[i][j]->kingSquare_b = j;
            }

            board_set(table->boards[i][j], j, KING|color);
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
            for(int j = 0; j < 64; j++)
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
            for(int j = 0; j < 64; j++)
            {
                destroy_board(playingTable->boards[i][j]);
            }
        }
        FREE(playingTable);
    }

    table = NULL;
}