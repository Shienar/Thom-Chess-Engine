#include "neuralnet.h"
#include "../debug.h"
#include "../board/bitboard.h"
#include "../board/moves.h"
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
    assert((accumulatorType == PLAYING && playerNNUE) || (accumulatorType == TRAINING && trainingNNUE));
    
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

    inputs[baseIndex_w + 0] = board->pieces[WHITE_PAWN];
    inputs[baseIndex_w + 1] = board->pieces[WHITE_KNIGHT];
    inputs[baseIndex_w + 2] = board->pieces[WHITE_BISHOP];
    inputs[baseIndex_w + 3] = board->pieces[WHITE_ROOK];
    inputs[baseIndex_w + 4] = board->pieces[WHITE_QUEEN];
    inputs[baseIndex_w + 5] = board->pieces[BLACK_PAWN];
    inputs[baseIndex_w + 6] = board->pieces[BLACK_KNIGHT];
    inputs[baseIndex_w + 7] = board->pieces[BLACK_BISHOP];
    inputs[baseIndex_w + 8] = board->pieces[BLACK_ROOK];
    inputs[baseIndex_w + 9] = board->pieces[BLACK_QUEEN];

    inputs[baseIndex_b + 0] = FLIP_MASK(board->pieces[BLACK_PAWN]);
    inputs[baseIndex_b + 1] = FLIP_MASK(board->pieces[BLACK_KNIGHT]);
    inputs[baseIndex_b + 2] = FLIP_MASK(board->pieces[BLACK_BISHOP]);
    inputs[baseIndex_b + 3] = FLIP_MASK(board->pieces[BLACK_ROOK]);
    inputs[baseIndex_b + 4] = FLIP_MASK(board->pieces[BLACK_QUEEN]);
    inputs[baseIndex_b + 5] = FLIP_MASK(board->pieces[WHITE_PAWN]);
    inputs[baseIndex_b + 6] = FLIP_MASK(board->pieces[WHITE_KNIGHT]);
    inputs[baseIndex_b + 7] = FLIP_MASK(board->pieces[WHITE_BISHOP]);
    inputs[baseIndex_b + 8] = FLIP_MASK(board->pieces[WHITE_ROOK]);
    inputs[baseIndex_b + 9] = FLIP_MASK(board->pieces[WHITE_QUEEN]);

    if(getColumn(board->kingSquare_w) > 3)
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
    if(getColumn(board->kingSquare_b) > 3)
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
            calculateLayer_IntBytes(inputArray, byteAccumulator->rawAccumulator[WHITE], 640, ACCUMULATOR_NODES_PER_SIDE, playerNNUE->weights1, extendedBaseIndex_w, playerNNUE->weights1_bias, 0);
            memcpy(&byteAccumulator->accumulator[WHITE], &byteAccumulator->rawAccumulator[WHITE], ACCUMULATOR_NODES_PER_SIDE * sizeof(char));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=32) SIMD_SCReLU(&byteAccumulator->accumulator[WHITE][i], v_min, v_max);
        }
        if(ISBLACK(color)) 
        {
            calculateLayer_IntBytes(&inputArray[640], byteAccumulator->rawAccumulator[BLACK], 640, ACCUMULATOR_NODES_PER_SIDE, playerNNUE->weights1, extendedBaseIndex_b, playerNNUE->weights1_bias, 0);
            memcpy(&byteAccumulator->accumulator[BLACK], &byteAccumulator->rawAccumulator[BLACK], ACCUMULATOR_NODES_PER_SIDE * sizeof(char));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=32) SIMD_SCReLU(&byteAccumulator->accumulator[BLACK][i], v_min, v_max);
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
            calculateLayer_Floats(inputArray, floatAccumulator->rawAccumulator[WHITE], 640, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights1, extendedBaseIndex_w, trainingNNUE->weights1_bias, 0);
            memcpy(&floatAccumulator->accumulator[WHITE], &floatAccumulator->rawAccumulator[WHITE], ACCUMULATOR_NODES_PER_SIDE * sizeof(float));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=8) SIMD_SCReLU_Float(&floatAccumulator->accumulator[WHITE][i], v_min, v_max, v_grad);
        }
        if(ISBLACK(color)) 
        {
            calculateLayer_Floats(&inputArray[640], floatAccumulator->rawAccumulator[BLACK], 640, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights1, extendedBaseIndex_b, trainingNNUE->weights1_bias, 0);
            memcpy(&floatAccumulator->accumulator[BLACK], &floatAccumulator->rawAccumulator[BLACK], ACCUMULATOR_NODES_PER_SIDE * sizeof(float));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=8) SIMD_SCReLU_Float(&floatAccumulator->accumulator[BLACK][i], v_min, v_max, v_grad);
        }
    }
}

void updateMoveAccumulator(bitboard* board, move lastMove, int shouldUndoMove, void* accumulator, void* refreshTable, int accumulatorType)
{
    assert(accumulator);

    if(ISKING(lastMove.piece)) updateAccumulatorFromTable(board, accumulator, refreshTable, accumulatorType);
    
    accumulator_playing* byteAccumulator = NULL;
    accumulator_training* floatAccumulator = NULL;
    if(accumulatorType == PLAYING) byteAccumulator = (accumulator_playing*) accumulator;
    else floatAccumulator = (accumulator_training*) accumulator;

    for(int i = 0; i < 2; i++)
    {
        int ksq, fromSq, toSq, pieceOffset;
        if(i == 0)
        {
            ksq = board->kingSquare_w;
            if(shouldUndoMove)
            {
                toSq = lastMove.startSquare;
                fromSq = lastMove.endSquare;
            }
            else
            {
                fromSq = lastMove.startSquare;
                toSq = lastMove.endSquare;
            }
            pieceOffset = lastMove.piece;
        }
        else
        {
            ksq = FLIP_SQUARE(board->kingSquare_b);
            if(shouldUndoMove)
            {
                toSq = FLIP_SQUARE(lastMove.startSquare);
                fromSq = FLIP_SQUARE(lastMove.endSquare);
            }
            else
            {
                fromSq = FLIP_SQUARE(lastMove.startSquare);
                toSq = FLIP_SQUARE(lastMove.endSquare);
            }
            pieceOffset = FLIP_COLOR(lastMove.piece);
        }
        
        pieceOffset = ISWHITE(pieceOffset) ? PIECE(pieceOffset) / 2 :  5 + PIECE(pieceOffset) / 2;
        
        int inputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * i) + (10 * kingBuckets[ksq]) + pieceOffset;
        uint64_t xorMask = singleBitMask(fromSq) | singleBitMask(toSq);

        if(byteAccumulator) byteAccumulator->inputNodes[inputNodeIndex]^=xorMask;
        else floatAccumulator->inputNodes[inputNodeIndex]^=xorMask;

        int fromIdx = (64 * ((10 * kingBuckets[ksq]) + pieceOffset)) + fromSq;
        int toIdx   = (64 * ((10 * kingBuckets[ksq]) + pieceOffset)) + toSq;

        int capIdx = -1;
        if(lastMove.capturedPiece)
        {
            int capturedPieceOffset, capturedPieceSquare;
            if(i == 0)
            {
                capturedPieceOffset = lastMove.capturedPiece;
                capturedPieceSquare = lastMove.capturedPieceSquare;
            }
            else
            {
                capturedPieceOffset = FLIP_COLOR(lastMove.capturedPiece);
                capturedPieceSquare = FLIP_SQUARE(lastMove.capturedPieceSquare);
            }

            capturedPieceOffset = ISWHITE(pieceOffset) ? PIECE(capturedPieceOffset) / 2 :  5 + PIECE(capturedPieceOffset) / 2;
            int capturedInputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * i) + (10 * kingBuckets[ksq]) + capturedPieceOffset;

            if(byteAccumulator) byteAccumulator->inputNodes[capturedInputNodeIndex]^=(1ull<<capturedPieceSquare);
            else floatAccumulator->inputNodes[capturedInputNodeIndex]^=(1ull<<capturedPieceSquare);

            capIdx = (64 * ((10 * kingBuckets[ksq]) + capturedPieceOffset)) + capturedPieceSquare;
        }

        int isCapture = shouldUndoMove ? 1 : -1;

        if(byteAccumulator)
        {
            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            {
                byteAccumulator->accumulator[i][j]+= playerNNUE->weights1[j][toIdx] - playerNNUE->weights1[j][fromIdx];
                if(capIdx != -1) byteAccumulator->accumulator[i][j]+= (isCapture * playerNNUE->weights1[j][capIdx]);
            }
        }
        else
        {
            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            {
                floatAccumulator->accumulator[i][j]+= trainingNNUE->weights1[j][toIdx] - trainingNNUE->weights1[j][fromIdx];
                if(capIdx != -1) floatAccumulator->accumulator[i][j]+= (isCapture * trainingNNUE->weights1[j][capIdx]);
            }
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
                (ISWHITE(color) && kingBuckets[currentBoard->kingSquare_w] == kingBuckets[accumulatorBoard->kingSquare_w])));
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

        memcpy(curBoard, currentBoard->pieces, 10 * sizeof(uint64_t));
        memcpy(accumBoard, accumulatorBoard->pieces, 10 * sizeof(uint64_t));
    }
    else
    {
        ksq = FLIP_SQUARE(accumulatorBoard->kingSquare_b);

        curBoard[0] = FLIP_MASK(currentBoard->pieces[BLACK_PAWN]);
        curBoard[1] = FLIP_MASK(currentBoard->pieces[BLACK_KNIGHT]);
        curBoard[2] = FLIP_MASK(currentBoard->pieces[BLACK_BISHOP]);
        curBoard[3] = FLIP_MASK(currentBoard->pieces[BLACK_ROOK]);
        curBoard[4] = FLIP_MASK(currentBoard->pieces[BLACK_QUEEN]);
        curBoard[5] = FLIP_MASK(currentBoard->pieces[WHITE_PAWN]);
        curBoard[6] = FLIP_MASK(currentBoard->pieces[WHITE_KNIGHT]);
        curBoard[7] = FLIP_MASK(currentBoard->pieces[WHITE_BISHOP]);
        curBoard[8] = FLIP_MASK(currentBoard->pieces[WHITE_ROOK]);
        curBoard[9] = FLIP_MASK(currentBoard->pieces[WHITE_QUEEN]);

        accumBoard[0] = FLIP_MASK(accumulatorBoard->pieces[BLACK_PAWN]);
        accumBoard[1] = FLIP_MASK(accumulatorBoard->pieces[BLACK_KNIGHT]);
        accumBoard[2] = FLIP_MASK(accumulatorBoard->pieces[BLACK_BISHOP]);
        accumBoard[3] = FLIP_MASK(accumulatorBoard->pieces[BLACK_ROOK]);
        accumBoard[4] = FLIP_MASK(accumulatorBoard->pieces[BLACK_QUEEN]);
        accumBoard[5] = FLIP_MASK(accumulatorBoard->pieces[WHITE_PAWN]);
        accumBoard[6] = FLIP_MASK(accumulatorBoard->pieces[WHITE_KNIGHT]);
        accumBoard[7] = FLIP_MASK(accumulatorBoard->pieces[WHITE_BISHOP]);
        accumBoard[8] = FLIP_MASK(accumulatorBoard->pieces[WHITE_ROOK]);
        accumBoard[9] = FLIP_MASK(accumulatorBoard->pieces[WHITE_QUEEN]);
    }
    if(getColumn(ksq) > 3)
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

            char sign = curBoard[piece]&singleBitMask(square) ? 1 : -1;

            if(byteAccumulator)
            {
                if(ISWHITE(color))
                {
                    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
                    {
                        byteAccumulator->rawAccumulator[WHITE][i] += playerNNUE->weights1[i][featureIndex_White] * sign;
                    }
                }
                if(ISBLACK(color))
                {
                    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
                    {
                        byteAccumulator->rawAccumulator[BLACK][i] += playerNNUE->weights1[i][featureIndex_Black] * sign;
                    }
                }
            }
            else
            {

                if(ISWHITE(color))
                {
                    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
                    {
                        floatAccumulator->rawAccumulator[WHITE][i] += trainingNNUE->weights1[i][featureIndex_White] * sign;
                    }
                }
                if(ISBLACK(color))
                {
                    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
                    {
                        floatAccumulator->rawAccumulator[BLACK][i] += trainingNNUE->weights1[i][featureIndex_Black] * sign;
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
            memcpy(&byteAccumulator->accumulator[WHITE], &byteAccumulator->rawAccumulator[WHITE], ACCUMULATOR_NODES_PER_SIDE * sizeof(char));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=32) SIMD_SCReLU(&byteAccumulator->accumulator[WHITE][i], v_min, v_max);
        }
        if(ISBLACK(color))
        {
            memcpy(&byteAccumulator->accumulator[BLACK], &byteAccumulator->rawAccumulator[BLACK], ACCUMULATOR_NODES_PER_SIDE * sizeof(char));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=32) SIMD_SCReLU(&byteAccumulator->accumulator[BLACK][i], v_min, v_max);
        }
    }
    else if(floatAccumulator)
    {
        __m256 v_min = _mm256_setzero_ps();
        __m256 v_max = _mm256_set1_ps(1.0);
        __m256 v_grad = _mm256_set1_ps(LEAK_FACTOR);
        if(ISWHITE(color))
        {
            memcpy(&floatAccumulator->accumulator[WHITE], &floatAccumulator->rawAccumulator[WHITE], ACCUMULATOR_NODES_PER_SIDE * sizeof(float));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=8) SIMD_SCReLU_Float(&floatAccumulator->accumulator[WHITE][i], v_min, v_max, v_grad);
        }
        if(ISBLACK(color))
        {
            memcpy(&floatAccumulator->accumulator[BLACK], &floatAccumulator->rawAccumulator[BLACK], ACCUMULATOR_NODES_PER_SIDE * sizeof(float));
            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i+=8) SIMD_SCReLU_Float(&floatAccumulator->accumulator[BLACK][i], v_min, v_max, v_grad);
        }
    }
    

    memcpy(accumulatorBoard, currentBoard, sizeof(bitboard));
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
        if(byteTable->boards[WHITE][kingBucket_w]->kingSquare_w == 0 && byteTable->boards[WHITE][kingBucket_w]->kingSquare_b == 0)
        {
            memcpy(byteTable->boards[WHITE][kingBucket_w], board, sizeof(bitboard));
            loadInputAccumulator(byteTable->boards[WHITE][kingBucket_w], &byteTable->accumulators[kingBucket_w], accumulatorType, WHITE);
        }
        if(byteTable->boards[BLACK][kingBucket_b]->kingSquare_w == 0 && byteTable->boards[BLACK][kingBucket_b]->kingSquare_b == 0)
        {
            memcpy(byteTable->boards[BLACK][kingBucket_b], board, sizeof(bitboard));
            loadInputAccumulator(byteTable->boards[BLACK][kingBucket_b], &byteTable->accumulators[kingBucket_b], accumulatorType, BLACK);
        }

        updateBoardAccumulator(board, byteTable->boards[WHITE][kingBucket_w], &byteTable->accumulators[kingBucket_w], accumulatorType, WHITE);
        updateBoardAccumulator(board, byteTable->boards[BLACK][kingBucket_b], &byteTable->accumulators[kingBucket_b], accumulatorType, BLACK);
        
        accumulator_playing* acc = (accumulator_playing*) accumulator;
        memcpy(acc->accumulator[WHITE], byteTable->accumulators[kingBucket_w].accumulator[WHITE], sizeof(int8_t) * ACCUMULATOR_NODES_PER_SIDE);
        memcpy(acc->accumulator[BLACK], byteTable->accumulators[kingBucket_b].accumulator[BLACK], sizeof(int8_t) * ACCUMULATOR_NODES_PER_SIDE);
    }
    else 
    {
        accumulator_training_refreshTable* floatTable = (accumulator_training_refreshTable*) refreshTable;

        //Uninitialized bitboard in accumulator refresh table.
        if(floatTable->boards[WHITE][kingBucket_w]->kingSquare_w == 0 && floatTable->boards[WHITE][kingBucket_w]->kingSquare_b == 0)
        {
            memcpy(floatTable->boards[WHITE][kingBucket_w], board, sizeof(bitboard));
            loadInputAccumulator(floatTable->boards[WHITE][kingBucket_w], &floatTable->accumulators[kingBucket_w], accumulatorType, WHITE);
        }
        if(floatTable->boards[BLACK][kingBucket_b]->kingSquare_w == 0 && floatTable->boards[BLACK][kingBucket_b]->kingSquare_b == 0)
        {
            memcpy(floatTable->boards[BLACK][kingBucket_b], board, sizeof(bitboard));
            loadInputAccumulator(floatTable->boards[BLACK][kingBucket_b], &floatTable->accumulators[kingBucket_b], accumulatorType, BLACK);
        }

        updateBoardAccumulator(board, floatTable->boards[WHITE][kingBucket_w], &floatTable->accumulators[kingBucket_w], accumulatorType, WHITE);
        updateBoardAccumulator(board, floatTable->boards[BLACK][kingBucket_b], &floatTable->accumulators[kingBucket_b], accumulatorType, BLACK);
        
        accumulator_training* acc = (accumulator_training*) accumulator;
        memcpy(acc->accumulator[WHITE], floatTable->accumulators[kingBucket_w].accumulator[WHITE], sizeof(float) * ACCUMULATOR_NODES_PER_SIDE);
        memcpy(acc->accumulator[BLACK], floatTable->accumulators[kingBucket_b].accumulator[BLACK], sizeof(float) * ACCUMULATOR_NODES_PER_SIDE);
    }
}

accumulator_playing_refreshTable* createPlayingRefreshTable()
{
    accumulator_playing_refreshTable* table = calloc(1, sizeof(accumulator_playing_refreshTable));
    for(int color = 0; color < 2; color++)
    {
        for(int j = 0; j < KING_BUCKETS; j++)
        {
            int newKingSquare = kingBucketMap[j];
            table->boards[color][j] = create_board();
            if(color == WHITE) 
            {
                board_clear_square(table->boards[color][j], table->boards[color][j]->kingSquare_w);
                table->boards[color][j]->kingSquare_w = newKingSquare;
                board_set(table->boards[color][j], newKingSquare, KING|color);
            }
            else
            {   
                board_clear_square(table->boards[color][j], table->boards[color][j]->kingSquare_b);
                table->boards[color][j]->kingSquare_b = FLIP_SQUARE(newKingSquare);
                board_set(table->boards[color][j], FLIP_SQUARE(newKingSquare), KING|color);
            }

        }
    }
    return table;
}

accumulator_training_refreshTable* createTrainingRefreshTable()
{
    accumulator_training_refreshTable* table = calloc(1, sizeof(accumulator_training_refreshTable));
    for(int color = 0; color < 2; color++)
    {
        for(int j = 0; j < KING_BUCKETS; j++)
        {
            int newKingSquare = kingBucketMap[j];
            table->boards[color][j] = create_board();
            if(color == WHITE) 
            {
                board_clear_square(table->boards[color][j], table->boards[color][j]->kingSquare_w);
                table->boards[color][j]->kingSquare_w = newKingSquare;
                board_set(table->boards[color][j], newKingSquare, KING|color);
            }
            else
            {   
                board_clear_square(table->boards[color][j], table->boards[color][j]->kingSquare_b);
                table->boards[color][j]->kingSquare_b = newKingSquare;
                board_set(table->boards[color][j], FLIP_SQUARE(newKingSquare), KING|color);
            }

            board_set(table->boards[color][j], newKingSquare, KING|color);
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
                free(trainingTable->boards[i][j]);
            }
        }
        free(trainingTable);
    }
    else
    {
        accumulator_playing_refreshTable* playingTable = (accumulator_playing_refreshTable*) table;
        for(int i = 0; i < 2; i++)
        {
            for(int j = 0; j < KING_BUCKETS; j++)
            {
                free(playingTable->boards[i][j]);
            }
        }
        free(playingTable);
    }

    table = NULL;
}