#include "neuralnet.h"
#include "../debug.h"
#include "../board/bitboard.h"
#include "../board/moves.h"
#include "engine.h"
#include <math.h>
#include <float.h>
#include <immintrin.h>

accumulator* playerAccumulator = NULL;
accumulatorRefreshTable* playingRefreshTable = NULL;

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

//full refresh of raw values.
void calculateAccumulator(uint64_t* inputNodes, int16_t* outputValues, int kingBucket, network_weights_playing* weights)
{
    for (int outputIndex = 0; outputIndex < ACCUMULATOR_NODES_PER_SIDE; outputIndex+=16) 
    {
        __m256i v_output = _mm256_loadu_si256((__m256i*)&weights->weights1_bias[outputIndex]);

        for(int piece = 0; piece < 10; piece++)
        {
            uint64_t pieceMask = inputNodes[kingBucket * 10 + piece];
            int baseIndex = kingBucket * 640 + (piece * 64);
            while(pieceMask)
            {
                int featureIndex =  baseIndex + __builtin_ctzll(pieceMask);

                v_output = _mm256_add_epi16(v_output, _mm256_loadu_si256((__m256i*)&weights->weights1[featureIndex][outputIndex]));

                pieceMask &= (pieceMask - 1);
            }
        }

        _mm256_storeu_si256((__m256i*)&outputValues[outputIndex], v_output);
    } 
}

void activateAccumulator(int16_t* rawValues, uint8_t* activatedValues)
{
    const __m256i v_min = _mm256_setzero_si256();
    const __m256i v_max = _mm256_set1_epi16(QA);

    for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 32) 
    {
        //Activate 16-bit values in two 16-value groups, then combine them & store as 32 8-bit values.
        __m256i v_first =_mm256_max_epi16(_mm256_min_epi16(_mm256_loadu_si256((__m256i*)&rawValues[i]), v_max), v_min);
        
        __m256i v_first_lower_sq = _mm256_mullo_epi32(_mm256_cvtepi16_epi32(_mm256_castsi256_si128(v_first)), 
                                                      _mm256_cvtepi16_epi32(_mm256_castsi256_si128(v_first)));
        __m256i v_first_upper_sq = _mm256_mullo_epi32(_mm256_cvtepi16_epi32(_mm256_extracti128_si256(v_first, 1)), 
                                                      _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v_first, 1)));
        
        //Shift right 8 to keep in uint8_t range.
        v_first = _mm256_packus_epi32(_mm256_srli_epi32(v_first_lower_sq, 8), _mm256_srli_epi32(v_first_upper_sq, 8));

        //second group.
        __m256i v_second =_mm256_max_epi16(_mm256_min_epi16(_mm256_loadu_si256((__m256i*)&rawValues[i + 16]), v_max), v_min);
        
        __m256i v_second_lower_sq = _mm256_mullo_epi32(_mm256_cvtepi16_epi32(_mm256_castsi256_si128(v_second)), 
                                          _mm256_cvtepi16_epi32(_mm256_castsi256_si128(v_second)));
        __m256i v_second_upper_sq = _mm256_mullo_epi32(_mm256_cvtepi16_epi32(_mm256_extracti128_si256(v_second, 1)), 
                                          _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v_second, 1)));
        
        v_second = _mm256_packus_epi32(_mm256_srli_epi32(v_second_lower_sq, 8), _mm256_srli_epi32(v_second_upper_sq, 8));

        //Pack 2 x 16 16-bit -> 1 x 32 8-bit
        __m256i v_final = _mm256_packus_epi16(v_first, v_second);

        // Permute required because packus_epi16 works within 128-bit lanes
        v_final = _mm256_permute4x64_epi64(v_final, _MM_SHUFFLE(3, 1, 2, 0));

        _mm256_storeu_si256((__m256i*)&activatedValues[i], v_final);
    }
}

void loadInputAccumulator(bitboard* board, accumulator* acc, int color)
{
    assert(board);
    assert(acc);
    
    uint64_t* inputs = acc->inputNodes;

    memset(inputs, 0, 320*sizeof(uint64_t));

    int baseIndex_w = 10*kingBuckets[board->kingSquare_w];
    int baseIndex_b = BITBOARDS_PER_INPUT_SIDE + 10*kingBuckets[FLIP_SQUARE(board->kingSquare_b)];

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

    if(ISWHITE(color)) 
    {
        calculateAccumulator(inputs, acc->rawAccumulator[WHITE], kingBuckets[board->kingSquare_w], playerNNUE);
        activateAccumulator(acc->rawAccumulator[WHITE], acc->accumulator[WHITE]);
    }
    if(ISBLACK(color)) 
    {
        calculateAccumulator(&inputs[BITBOARDS_PER_INPUT_SIDE], acc->rawAccumulator[BLACK], kingBuckets[FLIP_SQUARE(board->kingSquare_b)], playerNNUE);
        activateAccumulator(acc->rawAccumulator[BLACK], acc->accumulator[BLACK]);
    }
}

void updateMoveAccumulator(bitboard* board, move lastMove, int shouldUndoMove, accumulator* acc,  accumulatorRefreshTable* refreshTable)
{
    assert(acc);
    assert(refreshTable);

    if(ISKING(lastMove.piece)) updateAccumulatorFromTable(board, acc, refreshTable);

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

        acc->inputNodes[inputNodeIndex]^=xorMask;

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

            assert(!ISKING(lastMove.capturedPiece));
            capturedPieceOffset = ISWHITE(pieceOffset) ? PIECE(capturedPieceOffset) / 2 :  5 + PIECE(capturedPieceOffset) / 2;
            int capturedInputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * i) + (10 * kingBuckets[ksq]) + capturedPieceOffset;

            acc->inputNodes[capturedInputNodeIndex]^=(1ull<<capturedPieceSquare);

            capIdx = (64 * ((10 * kingBuckets[ksq]) + capturedPieceOffset)) + capturedPieceSquare;
        }

        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=16)
        {
            __m256i v_acc = _mm256_add_epi16(_mm256_loadu_si256((__m256i*)&acc->accumulator[i][j]), _mm256_sub_epi16(_mm256_loadu_si256((__m256i*)&playerNNUE->weights1[toIdx][j]), 
                                                                                            _mm256_loadu_si256((__m256i*)&playerNNUE->weights1[fromIdx][j])));
            
            _mm256_storeu_si256((__m256i*)&acc->accumulator[i][j], v_acc);
        }
        if(capIdx != -1)
        {
            if(shouldUndoMove)
            {
                for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=16)
                {
                    _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[i][j], 
                                        _mm256_add_epi16(_mm256_loadu_si256((__m256i*)&acc->rawAccumulator[i][j]), 
                                                         _mm256_loadu_si256((__m256i*)&playerNNUE->weights1[capIdx][j])));
                }
            }
            else
            {
                for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=16)
                {
                    _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[i][j], 
                                        _mm256_sub_epi16(_mm256_loadu_si256((__m256i*)&acc->rawAccumulator[i][j]), 
                                                         _mm256_loadu_si256((__m256i*)&playerNNUE->weights1[capIdx][j])));
                }
            }
        }
        activateAccumulator(acc->rawAccumulator[i], acc->accumulator[i]);
    }
}

void updateBoardAccumulator(bitboard* currentBoard, bitboard* accumulatorBoard, accumulator* acc, int color)
{
    assert(currentBoard);
    assert(accumulatorBoard);
    assert(acc);
    assert(((ISBLACK(color) && kingBuckets[currentBoard->kingSquare_b] == kingBuckets[accumulatorBoard->kingSquare_b]) || 
                (ISWHITE(color) && kingBuckets[currentBoard->kingSquare_w] == kingBuckets[accumulatorBoard->kingSquare_w])));

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

        acc->inputNodes[inputNodeIndex]^=difference;

        while(difference)
        {
            int square = __builtin_ctzll(difference);

            int featureIndex_Black = (64 * (10 * kingBuckets[ksq] + piece)) + square;
            int featureIndex_White = (64 * (10 * kingBuckets[ksq] + piece)) + square;

            char wasAdded = curBoard[piece]&singleBitMask(square) ? 1 : 0;

            int16_t* targetAcc = acc->rawAccumulator[color];
            int16_t* targetWeights = (color == WHITE) ? playerNNUE->weights1[featureIndex_White] : playerNNUE->weights1[featureIndex_Black];

            if (wasAdded) 
            {
                for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 16) 
                {
                    _mm256_storeu_si256((__m256i*)&targetAcc[i], 
                                        _mm256_add_epi16(_mm256_loadu_si256((__m256i*)&targetAcc[i]),
                                                         _mm256_loadu_si256((__m256i*)&targetWeights[i])));
                }
            } 
            else 
            {
                for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 16) 
                {
                    _mm256_storeu_si256((__m256i*)&targetAcc[i], 
                                        _mm256_sub_epi16(_mm256_loadu_si256((__m256i*)&targetAcc[i]),
                                                         _mm256_loadu_si256((__m256i*)&targetWeights[i])));
                }
            }

            difference&=(difference - 1);
        }
    }

    if(ISWHITE(color)) activateAccumulator(acc->rawAccumulator[WHITE], acc->accumulator[WHITE]);
    if(ISBLACK(color)) activateAccumulator(acc->rawAccumulator[BLACK], acc->accumulator[BLACK]);

    memcpy(accumulatorBoard, currentBoard, sizeof(bitboard));
}

void updateAccumulatorFromTable(bitboard* board, accumulator* acc,  accumulatorRefreshTable* refreshTable)
{
    assert(board);
    assert(acc);
    assert(refreshTable);

    int kingBucket_w = kingBuckets[board->kingSquare_w];
    int kingBucket_b = kingBuckets[FLIP_SQUARE(board->kingSquare_b)]; 
        
    //Uninitialized bitboard in accumulator refresh table.
    if(refreshTable->boards[WHITE][kingBucket_w]->kingSquare_w == 0 && refreshTable->boards[WHITE][kingBucket_w]->kingSquare_b == 0)
    {
        memcpy(refreshTable->boards[WHITE][kingBucket_w], board, sizeof(bitboard));
        loadInputAccumulator(refreshTable->boards[WHITE][kingBucket_w], &refreshTable->accumulators[kingBucket_w], WHITE);
    }
    if(refreshTable->boards[BLACK][kingBucket_b]->kingSquare_w == 0 && refreshTable->boards[BLACK][kingBucket_b]->kingSquare_b == 0)
    {
        memcpy(refreshTable->boards[BLACK][kingBucket_b], board, sizeof(bitboard));
        loadInputAccumulator(refreshTable->boards[BLACK][kingBucket_b], &refreshTable->accumulators[kingBucket_b], BLACK);
    }

    updateBoardAccumulator(board, refreshTable->boards[WHITE][kingBucket_w], &refreshTable->accumulators[kingBucket_w], WHITE);
    updateBoardAccumulator(board, refreshTable->boards[BLACK][kingBucket_b], &refreshTable->accumulators[kingBucket_b], BLACK);
    
    memcpy(acc->accumulator[WHITE], refreshTable->accumulators[kingBucket_w].accumulator[WHITE], sizeof(int8_t) * ACCUMULATOR_NODES_PER_SIDE);
    memcpy(acc->accumulator[BLACK], refreshTable->accumulators[kingBucket_b].accumulator[BLACK], sizeof(int8_t) * ACCUMULATOR_NODES_PER_SIDE);
}

accumulatorRefreshTable* createPlayingRefreshTable()
{
    accumulatorRefreshTable* table = calloc(1, sizeof(accumulatorRefreshTable));
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

void destroyRefreshTable(accumulatorRefreshTable* refreshTable)
{
    assert(refreshTable);

    for(int i = 0; i < 2; i++)
    {
        for(int j = 0; j < KING_BUCKETS; j++)
        {
            free(refreshTable->boards[i][j]);
        }
    }
    free(refreshTable);

    refreshTable = NULL;
}