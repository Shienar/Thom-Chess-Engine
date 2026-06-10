#include "analyze/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/engine.h"
#include <math.h>
#include <float.h>
#include <immintrin.h>
#include <string.h>

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
void calculateAccumulator(uint64_t* inputNodes, float* outputValues, int kingBucket, network_weights* weights)
{
    for (int outputIndex = 0; outputIndex < ACCUMULATOR_NODES_PER_SIDE; outputIndex+=8) 
    {
        __m256 v_output = _mm256_loadu_ps(&weights->weights1_bias[outputIndex]);

        for(int piece = 0; piece < 12; piece++)
        {
            uint64_t pieceMask = inputNodes[kingBucket * 12 + piece];
            int baseIndex = kingBucket * 768 + (piece * 64);
            while(pieceMask)
            {
                int featureIndex =  baseIndex + __builtin_ctzll(pieceMask);

                v_output = _mm256_add_ps(v_output, _mm256_loadu_ps(&weights->weights1[featureIndex][outputIndex]));

                pieceMask &= (pieceMask - 1);
            }
        }

        _mm256_storeu_ps(&outputValues[outputIndex], v_output);
    } 
}

void activateAccumulator(float* rawValues, float* activatedValues)
{
    const __m256 v_min = _mm256_setzero_ps();
    const __m256 v_max = _mm256_set1_ps(1.0f);

    for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 8) 
    {
        __m256 v_clamped =_mm256_max_ps(_mm256_min_ps(_mm256_loadu_ps(&rawValues[i]), v_max), v_min);
        _mm256_storeu_ps(&activatedValues[i], _mm256_mul_ps(v_clamped, v_clamped));
    }
}

void loadInputAccumulator(bitboard* board, accumulator* acc, int color)
{
    assert(board);
    assert(acc);
    
    uint64_t* inputs = acc->inputNodes;

    memset(inputs, 0, 384*sizeof(uint64_t));

    int baseIndex_w = 12*kingBuckets[board->kingSquare_w];
    int baseIndex_b = BITBOARDS_PER_INPUT_SIDE + 12*kingBuckets[FLIP_SQUARE(board->kingSquare_b)];

    inputs[baseIndex_w + 0]  = board->pieces[WHITE_PAWN];
    inputs[baseIndex_w + 1]  = board->pieces[WHITE_KNIGHT];
    inputs[baseIndex_w + 2]  = board->pieces[WHITE_BISHOP];
    inputs[baseIndex_w + 3]  = board->pieces[WHITE_ROOK];
    inputs[baseIndex_w + 4]  = board->pieces[WHITE_QUEEN];
    inputs[baseIndex_w + 5]  = board->pieces[WHITE_KING];
    inputs[baseIndex_w + 6]  = board->pieces[BLACK_PAWN];
    inputs[baseIndex_w + 7]  = board->pieces[BLACK_KNIGHT];
    inputs[baseIndex_w + 8]  = board->pieces[BLACK_BISHOP];
    inputs[baseIndex_w + 9]  = board->pieces[BLACK_ROOK];
    inputs[baseIndex_w + 10] = board->pieces[BLACK_QUEEN];
    inputs[baseIndex_w + 11] = board->pieces[BLACK_KING];

    inputs[baseIndex_b + 0]  = FLIP_MASK(board->pieces[BLACK_PAWN]);
    inputs[baseIndex_b + 1]  = FLIP_MASK(board->pieces[BLACK_KNIGHT]);
    inputs[baseIndex_b + 2]  = FLIP_MASK(board->pieces[BLACK_BISHOP]);
    inputs[baseIndex_b + 3]  = FLIP_MASK(board->pieces[BLACK_ROOK]);
    inputs[baseIndex_b + 4]  = FLIP_MASK(board->pieces[BLACK_QUEEN]);
    inputs[baseIndex_b + 5]  = FLIP_MASK(board->pieces[BLACK_KING]);
    inputs[baseIndex_b + 6]  = FLIP_MASK(board->pieces[WHITE_PAWN]);
    inputs[baseIndex_b + 7]  = FLIP_MASK(board->pieces[WHITE_KNIGHT]);
    inputs[baseIndex_b + 8]  = FLIP_MASK(board->pieces[WHITE_BISHOP]);
    inputs[baseIndex_b + 9]  = FLIP_MASK(board->pieces[WHITE_ROOK]);
    inputs[baseIndex_b + 10] = FLIP_MASK(board->pieces[WHITE_QUEEN]);
    inputs[baseIndex_b + 11] = FLIP_MASK(board->pieces[WHITE_KING]);

    if(getColumn(board->kingSquare_w) > 3)
    {
        for (int p = 0; p < 12; p++) 
        {
            inputs[baseIndex_w + p] = mirrorBoard(inputs[baseIndex_w + p]);
        }
    }
    if(getColumn(board->kingSquare_b) > 3)
    {
        for (int p = 0; p < 12; p++) {
            inputs[baseIndex_b + p] = mirrorBoard(inputs[baseIndex_b + p]);
        }
    }

    if(ISWHITE(color)) 
    {
        calculateAccumulator(inputs, acc->rawAccumulator[WHITE], kingBuckets[board->kingSquare_w], nnue_weights);
        activateAccumulator(acc->rawAccumulator[WHITE], acc->accumulator[WHITE]);
    }
    if(ISBLACK(color)) 
    {
        calculateAccumulator(&inputs[BITBOARDS_PER_INPUT_SIDE], acc->rawAccumulator[BLACK], kingBuckets[FLIP_SQUARE(board->kingSquare_b)], nnue_weights);
        activateAccumulator(acc->rawAccumulator[BLACK], acc->accumulator[BLACK]);
    }
}

void updateMoveAccumulator(bitboard* board, move lastMove, int shouldUndoMove, accumulator* acc,  accumulatorRefreshTable* refreshTable)
{
    assert(acc);
    assert(refreshTable);

    if(ISKING(lastMove.piece)) 
    {
        updateAccumulatorFromTable(board, acc, refreshTable);
        return;
    }

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
        
        //mirroring
        if(getColumn(ksq) > 3)
        {
            fromSq ^= 7;
            toSq ^= 7;
        }

        pieceOffset = ISWHITE(pieceOffset) ? PIECE(pieceOffset) / 2 :  6 + PIECE(pieceOffset) / 2;
        
        int inputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * i) + (12 * kingBuckets[ksq]) + pieceOffset;
        uint64_t xorMask = singleBitMask(fromSq) | singleBitMask(toSq);

        acc->inputNodes[inputNodeIndex]^=xorMask;

        int fromIdx = (64 * ((12 * kingBuckets[ksq]) + pieceOffset)) + fromSq;
        int toIdx   = (64 * ((12 * kingBuckets[ksq]) + pieceOffset)) + toSq;

        int capIdx = -1;
        if(lastMove.capturedPiece != EMPTY_PIECE)
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
            
            //mirroring
            if(getColumn(ksq) > 3)
            {
                capturedPieceSquare ^= 7;
            }

            assert(!ISKING(lastMove.capturedPiece));
            capturedPieceOffset = ISWHITE(capturedPieceOffset) ? PIECE(capturedPieceOffset) / 2 :  6 + PIECE(capturedPieceOffset) / 2;
            int capturedInputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * i) + (12 * kingBuckets[ksq]) + capturedPieceOffset;

            acc->inputNodes[capturedInputNodeIndex]^=(1ull<<capturedPieceSquare);

            capIdx = (64 * ((12 * kingBuckets[ksq]) + capturedPieceOffset)) + capturedPieceSquare;
        }

        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=8)
        {
            __m256 v_acc = _mm256_add_ps(_mm256_loadu_ps(&acc->rawAccumulator[i][j]), _mm256_sub_ps(_mm256_loadu_ps(&nnue_weights->weights1[toIdx][j]), 
                                                                                            _mm256_loadu_ps(&nnue_weights->weights1[fromIdx][j])));
            
            _mm256_storeu_ps(&acc->rawAccumulator[i][j], v_acc);
        }
        if(capIdx != -1)
        {
            if(shouldUndoMove)
            {
                for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=8)
                {
                    _mm256_storeu_ps(&acc->rawAccumulator[i][j], 
                                        _mm256_add_ps(_mm256_loadu_ps(&acc->rawAccumulator[i][j]), 
                                                         _mm256_loadu_ps(&nnue_weights->weights1[capIdx][j])));
                }
            }
            else
            {
                for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=8)
                {
                    _mm256_storeu_ps(&acc->rawAccumulator[i][j], 
                                        _mm256_sub_ps(_mm256_loadu_ps(&acc->rawAccumulator[i][j]), 
                                                         _mm256_loadu_ps(&nnue_weights->weights1[capIdx][j])));
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
    assert(((ISBLACK(color) && kingBuckets[FLIP_SQUARE(currentBoard->kingSquare_b)] == kingBuckets[FLIP_SQUARE(accumulatorBoard->kingSquare_b)]) || 
                (ISWHITE(color) && kingBuckets[currentBoard->kingSquare_w] == kingBuckets[accumulatorBoard->kingSquare_w])));

    uint64_t curBoard[12] = {0};
    uint64_t accumBoard[12] = {0};

    int ksq;
    if(ISWHITE(color))
    {
        ksq = accumulatorBoard->kingSquare_w;
        
        
        curBoard[0]  = currentBoard->pieces[WHITE_PAWN];
        curBoard[1]  = currentBoard->pieces[WHITE_KNIGHT];
        curBoard[2]  = currentBoard->pieces[WHITE_BISHOP];
        curBoard[3]  = currentBoard->pieces[WHITE_ROOK];
        curBoard[4]  = currentBoard->pieces[WHITE_QUEEN];
        curBoard[5]  = currentBoard->pieces[WHITE_KING];
        curBoard[6]  = currentBoard->pieces[BLACK_PAWN];
        curBoard[7]  = currentBoard->pieces[BLACK_KNIGHT];
        curBoard[8]  = currentBoard->pieces[BLACK_BISHOP];
        curBoard[9]  = currentBoard->pieces[BLACK_ROOK];
        curBoard[10] = currentBoard->pieces[BLACK_QUEEN];
        curBoard[11] = currentBoard->pieces[BLACK_KING];

        accumBoard[0]  = accumulatorBoard->pieces[BLACK_PAWN];
        accumBoard[1]  = accumulatorBoard->pieces[BLACK_KNIGHT];
        accumBoard[2]  = accumulatorBoard->pieces[BLACK_BISHOP];
        accumBoard[3]  = accumulatorBoard->pieces[BLACK_ROOK];
        accumBoard[4]  = accumulatorBoard->pieces[BLACK_QUEEN];
        accumBoard[5]  = accumulatorBoard->pieces[BLACK_KING];
        accumBoard[6]  = accumulatorBoard->pieces[WHITE_PAWN];
        accumBoard[7]  = accumulatorBoard->pieces[WHITE_KNIGHT];
        accumBoard[8]  = accumulatorBoard->pieces[WHITE_BISHOP];
        accumBoard[9]  = accumulatorBoard->pieces[WHITE_ROOK];
        accumBoard[10] = accumulatorBoard->pieces[WHITE_QUEEN];
        accumBoard[11] = accumulatorBoard->pieces[WHITE_KING];
    }
    else
    {
        ksq = FLIP_SQUARE(accumulatorBoard->kingSquare_b);

        curBoard[0]  = FLIP_MASK(currentBoard->pieces[BLACK_PAWN]);
        curBoard[1]  = FLIP_MASK(currentBoard->pieces[BLACK_KNIGHT]);
        curBoard[2]  = FLIP_MASK(currentBoard->pieces[BLACK_BISHOP]);
        curBoard[3]  = FLIP_MASK(currentBoard->pieces[BLACK_ROOK]);
        curBoard[4]  = FLIP_MASK(currentBoard->pieces[BLACK_QUEEN]);
        curBoard[5]  = FLIP_MASK(currentBoard->pieces[BLACK_KING]);
        curBoard[6]  = FLIP_MASK(currentBoard->pieces[WHITE_PAWN]);
        curBoard[7]  = FLIP_MASK(currentBoard->pieces[WHITE_KNIGHT]);
        curBoard[8]  = FLIP_MASK(currentBoard->pieces[WHITE_BISHOP]);
        curBoard[9]  = FLIP_MASK(currentBoard->pieces[WHITE_ROOK]);
        curBoard[10] = FLIP_MASK(currentBoard->pieces[WHITE_QUEEN]);
        curBoard[11] = FLIP_MASK(currentBoard->pieces[WHITE_KING]);

        accumBoard[0]  = FLIP_MASK(accumulatorBoard->pieces[BLACK_PAWN]);
        accumBoard[1]  = FLIP_MASK(accumulatorBoard->pieces[BLACK_KNIGHT]);
        accumBoard[2]  = FLIP_MASK(accumulatorBoard->pieces[BLACK_BISHOP]);
        accumBoard[3]  = FLIP_MASK(accumulatorBoard->pieces[BLACK_ROOK]);
        accumBoard[4]  = FLIP_MASK(accumulatorBoard->pieces[BLACK_QUEEN]);
        accumBoard[5]  = FLIP_MASK(accumulatorBoard->pieces[BLACK_KING]);
        accumBoard[6]  = FLIP_MASK(accumulatorBoard->pieces[WHITE_PAWN]);
        accumBoard[7]  = FLIP_MASK(accumulatorBoard->pieces[WHITE_KNIGHT]);
        accumBoard[8]  = FLIP_MASK(accumulatorBoard->pieces[WHITE_BISHOP]);
        accumBoard[9]  = FLIP_MASK(accumulatorBoard->pieces[WHITE_ROOK]);
        accumBoard[10] = FLIP_MASK(accumulatorBoard->pieces[WHITE_QUEEN]);
        accumBoard[11] = FLIP_MASK(accumulatorBoard->pieces[WHITE_KING]);
    }
    if(getColumn(ksq) > 3)
    {
        for(int i = 0; i < 12; i++)
        {
            curBoard[i] = mirrorBoard(curBoard[i]);
            accumBoard[i] = mirrorBoard(accumBoard[i]);
        }
    }

    for(int piece = 0; piece < 12; piece++)
    {
        uint64_t difference = curBoard[piece]^accumBoard[piece];

        //Input node updates
        int inputNodeIndex = (12 * kingBuckets[ksq]) + piece;
        if(ISBLACK(color)) inputNodeIndex+=BITBOARDS_PER_INPUT_SIDE;

        acc->inputNodes[inputNodeIndex]^=difference;

        while(difference)
        {
            int square = __builtin_ctzll(difference);

            int featureIndex_Black = (64 * (12 * kingBuckets[ksq] + piece)) + square;
            int featureIndex_White = (64 * (12 * kingBuckets[ksq] + piece)) + square;

            char wasAdded = curBoard[piece]&singleBitMask(square) ? 1 : 0;

            float* targetAcc = acc->rawAccumulator[color];
            float* targetWeights = (color == WHITE) ? nnue_weights->weights1[featureIndex_White] : nnue_weights->weights1[featureIndex_Black];

            if (wasAdded) 
            {
                for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 8) 
                {
                    _mm256_storeu_ps(&targetAcc[i], 
                                        _mm256_add_ps(_mm256_loadu_ps(&targetAcc[i]),
                                                         _mm256_loadu_ps(&targetWeights[i])));
                }
            } 
            else 
            {
                for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 8) 
                {
                    _mm256_storeu_ps(&targetAcc[i], 
                                        _mm256_sub_ps(_mm256_loadu_ps(&targetAcc[i]),
                                                         _mm256_loadu_ps(&targetWeights[i])));
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

accumulatorRefreshTable* createRefreshTable()
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
    if(!refreshTable) return;

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