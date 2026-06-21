#include "analyze/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/engine.h"
#include <math.h>
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
    0, 1, 2, 3,     3, 2, 1, 0,
    4, 4, 5, 5,     5, 5, 4, 4,
    6, 6, 6, 6,     6, 6, 6, 6,
    7, 7, 7, 7,     7, 7, 7, 7,
    8, 8, 8, 8,     8, 8, 8, 8,
    8, 8, 8, 8,     8, 8, 8, 8,
    9, 9, 9, 9,     9, 9, 9, 9,
    9, 9, 9, 9,     9, 9, 9, 9,
};

//full refresh of raw values.
void calculateAccumulator(uint64_t* inputNodes, int16_t* outputValues, int kingBucket, quantized_weights* weights)
{
    for (int outputIndex = 0; outputIndex < ACCUMULATOR_NODES_PER_SIDE; outputIndex+=16) 
    {
        __m256i v_output = _mm256_loadu_si256((const __m256i*)&weights->weights1_bias[outputIndex]);

        for(int piece = 0; piece < TRACKED_PIECES; piece++)
        {
            uint64_t pieceMask = inputNodes[kingBucket * TRACKED_PIECES + piece];
            int baseIndex = kingBucket * BITS_PER_BUCKET + (piece * 64);
            while(pieceMask)
            {
                int featureIndex =  baseIndex + __builtin_ctzll(pieceMask);

                v_output = _mm256_adds_epi16(v_output, _mm256_loadu_si256((const __m256i*)&weights->weights1[featureIndex][outputIndex]));

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
        //Clamp
        __m256i v_clamped = _mm256_max_epi16(_mm256_min_epi16(_mm256_loadu_si256((const __m256i*)&rawValues[i]), v_max), v_min); 
        
        //Seperate
        __m256i v_low = _mm256_unpacklo_epi16(v_clamped, v_min);
        __m256i v_high = _mm256_unpackhi_epi16(v_clamped, v_min);

        //Square
        v_low = _mm256_mullo_epi32(v_low, v_low);
        v_high = _mm256_mullo_epi32(v_high, v_high);

        //Downscale to [0, 254]
        v_low = _mm256_srli_epi32(v_low, 8);
        v_high = _mm256_srli_epi32(v_high, 8);

        //Pack 1
        __m256i v_output1 = _mm256_packus_epi32(v_low, v_high);
        v_output1 = _mm256_permute4x64_epi64(v_output1, _MM_SHUFFLE(3, 1, 2, 0));

        //Clamp
        v_clamped = _mm256_max_epi16(_mm256_min_epi16(_mm256_loadu_si256((const __m256i*)&rawValues[i + 16]), v_max), v_min); 
        
        //Seperate
        v_low = _mm256_unpacklo_epi16(v_clamped, v_min);
        v_high = _mm256_unpackhi_epi16(v_clamped, v_min);

        //Square
        v_low = _mm256_mullo_epi32(v_low, v_low);
        v_high = _mm256_mullo_epi32(v_high, v_high);

        //Downscale to [0, 254]
        v_low = _mm256_srli_epi32(v_low, 8);
        v_high = _mm256_srli_epi32(v_high, 8);

        //Pack 2
        __m256i v_output2 = _mm256_packus_epi32(v_low, v_high);
        v_output2 = _mm256_permute4x64_epi64(v_output2, _MM_SHUFFLE(3, 1, 2, 0));

        //Merge packed
        __m256i v_packed = _mm256_packus_epi16(v_output1, v_output2);
        v_packed = _mm256_permute4x64_epi64(v_packed, _MM_SHUFFLE(3, 1, 2, 0));

        _mm256_storeu_si256((__m256i*)&activatedValues[i], v_packed);
    }
}

void loadInputAccumulator(bitboard* board, accumulator* acc, int color)
{
    assert(board);
    assert(acc);
    
    uint64_t* inputs = acc->inputNodes;

    memset(inputs, 0, 2 * BITBOARDS_PER_INPUT_SIDE * sizeof(uint64_t));

    int baseIndex_w = TRACKED_PIECES*kingBuckets[board->kingSquare_w];
    int baseIndex_b = BITBOARDS_PER_INPUT_SIDE + TRACKED_PIECES*kingBuckets[FLIP_SQUARE(board->kingSquare_b)];

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
        for (int p = 0; p < TRACKED_PIECES; p++) inputs[baseIndex_w + p] = mirrorBoard(inputs[baseIndex_w + p]);
    }
    if(getColumn(board->kingSquare_b) > 3)
    {
        for (int p = 0; p < TRACKED_PIECES; p++) inputs[baseIndex_b + p] = mirrorBoard(inputs[baseIndex_b + p]);
    }

    if(ISWHITE(color)) 
    {
        calculateAccumulator(inputs, acc->rawAccumulator[WHITE], kingBuckets[board->kingSquare_w], int_weights);
        activateAccumulator(acc->rawAccumulator[WHITE], acc->accumulator[WHITE]);
    }
    if(ISBLACK(color)) 
    {
        calculateAccumulator(&inputs[BITBOARDS_PER_INPUT_SIDE], acc->rawAccumulator[BLACK], kingBuckets[FLIP_SQUARE(board->kingSquare_b)], int_weights);
        activateAccumulator(acc->rawAccumulator[BLACK], acc->accumulator[BLACK]);
    }
}

void updateMoveAccumulator(bitboard* board, move lastMove, int shouldUndoMove, accumulator* acc,  accumulatorRefreshTable* refreshTable)
{
    assert(board);
    assert(acc);
    assert(refreshTable);

    if(ISKING(lastMove.piece)) 
    {
        updateAccumulatorFromTable(board, acc, refreshTable);
        return;
    }

    for(int side = WHITE; side <= BLACK; side++)
    {
        int ksq, fromSq, toSq, pieceOffset;
        if(side == WHITE)
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

        pieceOffset = ISWHITE(pieceOffset) ? PIECE(pieceOffset) / 2 :  (TRACKED_PIECES / 2) + PIECE(pieceOffset) / 2;

        int inputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * side) + (TRACKED_PIECES * kingBuckets[ksq]) + pieceOffset;
        
        int fromIdx, toIdx;
        if(lastMove.promoteTo) 
        {
            int promotePieceOffset = ISWHITE(pieceOffset) ? PIECE(lastMove.promoteTo) / 2 :  (TRACKED_PIECES / 2) + PIECE(lastMove.promoteTo) / 2;
            int promoteInputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * side) + (TRACKED_PIECES * kingBuckets[ksq]) + promotePieceOffset;
            
            if(shouldUndoMove)
            {
                uint64_t xorMask_promote = singleBitMask(fromSq);
                uint64_t xorMask_pawn = singleBitMask(toSq);

                acc->inputNodes[inputNodeIndex] ^= xorMask_pawn;
                acc->inputNodes[promoteInputNodeIndex] ^= xorMask_promote;
                
                fromIdx = (64 * ((TRACKED_PIECES * kingBuckets[ksq]) + promotePieceOffset)) + fromSq;
                toIdx = (64 * ((TRACKED_PIECES * kingBuckets[ksq]) + pieceOffset)) + toSq;
            }
            else
            {
                uint64_t xorMask_promote = singleBitMask(toSq);
                uint64_t xorMask_pawn = singleBitMask(fromSq);

                acc->inputNodes[inputNodeIndex] ^= xorMask_pawn;
                acc->inputNodes[promoteInputNodeIndex] ^= xorMask_promote;
                
                fromIdx = (64 * ((TRACKED_PIECES * kingBuckets[ksq]) + pieceOffset)) + fromSq;
                toIdx = (64 * ((TRACKED_PIECES * kingBuckets[ksq]) + promotePieceOffset)) + toSq;
            }
        }
        else
        {
            
            uint64_t xorMask = singleBitMask(fromSq) | singleBitMask(toSq);

            acc->inputNodes[inputNodeIndex]^=xorMask;

            fromIdx = (64 * ((TRACKED_PIECES * kingBuckets[ksq]) + pieceOffset)) + fromSq;
            toIdx = (64 * ((TRACKED_PIECES * kingBuckets[ksq]) + pieceOffset)) + toSq;

        }

        int capIdx = -1;
        if(lastMove.capturedPiece != EMPTY_PIECE)
        {
            int capturedPieceOffset, capturedPieceSquare;
            if(side == WHITE)
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

            capturedPieceOffset = ISWHITE(capturedPieceOffset) ? PIECE(capturedPieceOffset) / 2 :  (TRACKED_PIECES / 2) + PIECE(capturedPieceOffset) / 2;
            int capturedInputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * side) + (TRACKED_PIECES * kingBuckets[ksq]) + capturedPieceOffset;

            acc->inputNodes[capturedInputNodeIndex]^=(1ull<<capturedPieceSquare);

            capIdx = (64 * ((TRACKED_PIECES * kingBuckets[ksq]) + capturedPieceOffset)) + capturedPieceSquare;
        }

        for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=16)
        {
            __m256i v_acc   = _mm256_loadu_si256((__m256i const*)&acc->rawAccumulator[side][j]);
            __m256i v_to    = _mm256_loadu_si256((__m256i const*)&int_weights->weights1[toIdx][j]);
            __m256i v_from  = _mm256_loadu_si256((__m256i const*)&int_weights->weights1[fromIdx][j]);
            
            v_acc = _mm256_adds_epi16(v_acc, v_to);
            v_acc = _mm256_subs_epi16(v_acc, v_from);
            
            _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[side][j], v_acc);
        }
        if(capIdx != -1)
        {
            if(shouldUndoMove)
            {
                for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=16)
                {
                    __m256i v_acc = _mm256_loadu_si256((__m256i const*)&acc->rawAccumulator[side][j]);
                    __m256i v_cap = _mm256_loadu_si256((__m256i const*)&int_weights->weights1[capIdx][j]);
                    
                    v_acc = _mm256_adds_epi16(v_acc, v_cap);
                    
                    _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[side][j], v_acc);
                }
            }
            else
            {
                for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=16)
                {
                    __m256i v_acc = _mm256_loadu_si256((__m256i const*)&acc->rawAccumulator[side][j]);
                    __m256i v_cap = _mm256_loadu_si256((__m256i const*)&int_weights->weights1[capIdx][j]);
                    
                    v_acc = _mm256_subs_epi16(v_acc, v_cap);
                    
                    _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[side][j], v_acc);
                }
            }
        }
        activateAccumulator(acc->rawAccumulator[side], acc->accumulator[side]);
    }

    //Keeping this commented out most of the time. 
    //It makes the engine  slower but it is necessary for debugging.
    /*
    #ifndef NDEBUG
        accumulator realAccumValues = {0};
        loadInputAccumulator(board, &realAccumValues, WHITE);
        loadInputAccumulator(board, &realAccumValues, BLACK);
        assert(memcmp(&realAccumValues.inputNodes, &acc->inputNodes, sizeof(acc->inputNodes)) == 0);
        assert(memcmp(&realAccumValues.rawAccumulator, &acc->rawAccumulator, sizeof(acc->rawAccumulator)) == 0);
        assert(memcmp(&realAccumValues.accumulator, &acc->accumulator, sizeof(acc->accumulator)) == 0);
    #endif
    */
}

void updateBoardAccumulator(bitboard* currentBoard, bitboard* accumulatorBoard, accumulator* acc, int color)
{
    assert(currentBoard);
    assert(accumulatorBoard);
    assert(acc);
    assert(((ISBLACK(color) && currentBoard->kingSquare_b == accumulatorBoard->kingSquare_b) || 
                (ISWHITE(color) && currentBoard->kingSquare_w == accumulatorBoard->kingSquare_w)));

    uint64_t curBoard[TRACKED_PIECES] = {0};
    uint64_t accumBoard[TRACKED_PIECES] = {0};

    int ksq; 
    if(ISWHITE(color))
    {
        ksq = accumulatorBoard->kingSquare_w;
        
        curBoard[0] = currentBoard->pieces[WHITE_PAWN];
        curBoard[1] = currentBoard->pieces[WHITE_KNIGHT];
        curBoard[2] = currentBoard->pieces[WHITE_BISHOP];
        curBoard[3] = currentBoard->pieces[WHITE_ROOK];
        curBoard[4] = currentBoard->pieces[WHITE_QUEEN];
        curBoard[5] = currentBoard->pieces[BLACK_PAWN];
        curBoard[6] = currentBoard->pieces[BLACK_KNIGHT];
        curBoard[7] = currentBoard->pieces[BLACK_BISHOP];
        curBoard[8] = currentBoard->pieces[BLACK_ROOK];
        curBoard[9] = currentBoard->pieces[BLACK_QUEEN];

        accumBoard[0] = accumulatorBoard->pieces[WHITE_PAWN];
        accumBoard[1] = accumulatorBoard->pieces[WHITE_KNIGHT];
        accumBoard[2] = accumulatorBoard->pieces[WHITE_BISHOP];
        accumBoard[3] = accumulatorBoard->pieces[WHITE_ROOK];
        accumBoard[4] = accumulatorBoard->pieces[WHITE_QUEEN];
        accumBoard[5] = accumulatorBoard->pieces[BLACK_PAWN];
        accumBoard[6] = accumulatorBoard->pieces[BLACK_KNIGHT];
        accumBoard[7] = accumulatorBoard->pieces[BLACK_BISHOP];
        accumBoard[8] = accumulatorBoard->pieces[BLACK_ROOK];
        accumBoard[9] = accumulatorBoard->pieces[BLACK_QUEEN];
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
        for(int i = 0; i < TRACKED_PIECES; i++)
        {
            curBoard[i] = mirrorBoard(curBoard[i]);
            accumBoard[i] = mirrorBoard(accumBoard[i]);
        }
    }

    for(int piece = 0; piece < TRACKED_PIECES; piece++)
    {
        uint64_t difference = curBoard[piece]^accumBoard[piece];

        //Input node updates
        int inputNodeIndex = (TRACKED_PIECES * kingBuckets[ksq]) + piece;
        if(ISBLACK(color)) inputNodeIndex+=BITBOARDS_PER_INPUT_SIDE;

        acc->inputNodes[inputNodeIndex]^=difference;

        while(difference)
        {
            int square = __builtin_ctzll(difference);

            int featureIndex_Black = (64 * (TRACKED_PIECES * kingBuckets[ksq] + piece)) + square;
            int featureIndex_White = (64 * (TRACKED_PIECES * kingBuckets[ksq] + piece)) + square;

            char wasAdded = curBoard[piece]&singleBitMask(square) ? 1 : 0;

            int16_t* targetAcc = acc->rawAccumulator[color];
            int16_t* targetWeights = (color == WHITE) ? int_weights->weights1[featureIndex_White] : int_weights->weights1[featureIndex_Black];

            if(wasAdded) 
            {
                for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 16) 
                {
                    __m256i v_acc = _mm256_loadu_si256((__m256i const*)&targetAcc[i]);
                    __m256i v_weight = _mm256_loadu_si256((__m256i const*)&targetWeights[i]);
                    
                    v_acc = _mm256_adds_epi16(v_acc, v_weight);
                    
                    _mm256_storeu_si256((__m256i*)&targetAcc[i], v_acc);
                }
            } 
            else 
            {
                for (int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 16) 
                {
                    __m256i v_acc = _mm256_loadu_si256((__m256i const*)&targetAcc[i]);
                    __m256i v_w   = _mm256_loadu_si256((__m256i const*)&targetWeights[i]);
                    
                    // Saturated integer subtraction
                    v_acc = _mm256_subs_epi16(v_acc, v_w);
                    
                    _mm256_storeu_si256((__m256i*)&targetAcc[i], v_acc);
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

    //Black square is flipped for weight index tracking, but not here in refresh table indices.
    int kingSq_w = board->kingSquare_w;
    int kingSq_b = board->kingSquare_b; 
        
    //Uninitialized bitboard in accumulator refresh table.
    if (!refreshTable->initialized[WHITE][kingSq_w])
    {
        memcpy(refreshTable->boards[WHITE][kingSq_w], board, sizeof(bitboard));
        loadInputAccumulator(refreshTable->boards[WHITE][kingSq_w], &refreshTable->accumulators[WHITE][kingSq_w], WHITE);
        loadInputAccumulator(refreshTable->boards[WHITE][kingSq_w], &refreshTable->accumulators[WHITE][kingSq_w], BLACK);
        refreshTable->initialized[WHITE][kingSq_w] = 1;
    }
    else updateBoardAccumulator(board, refreshTable->boards[WHITE][kingSq_w], &refreshTable->accumulators[WHITE][kingSq_w], WHITE);

    if (!refreshTable->initialized[BLACK][kingSq_b])
    {
        memcpy(refreshTable->boards[BLACK][kingSq_b], board, sizeof(bitboard));
        loadInputAccumulator(refreshTable->boards[BLACK][kingSq_b], &refreshTable->accumulators[BLACK][kingSq_b], WHITE);
        loadInputAccumulator(refreshTable->boards[BLACK][kingSq_b], &refreshTable->accumulators[BLACK][kingSq_b], BLACK);
        refreshTable->initialized[BLACK][kingSq_b] = 1;
    }
    else updateBoardAccumulator(board, refreshTable->boards[BLACK][kingSq_b], &refreshTable->accumulators[BLACK][kingSq_b], BLACK);
    
    memcpy(&acc->inputNodes, &refreshTable->accumulators[WHITE][kingSq_w].inputNodes, sizeof(uint64_t) * BITBOARDS_PER_INPUT_SIDE);
    memcpy(&acc->inputNodes[BITBOARDS_PER_INPUT_SIDE], &refreshTable->accumulators[BLACK][kingSq_b].inputNodes[BITBOARDS_PER_INPUT_SIDE], sizeof(uint64_t) * BITBOARDS_PER_INPUT_SIDE);

    memcpy(acc->rawAccumulator[WHITE], refreshTable->accumulators[WHITE][kingSq_w].rawAccumulator[WHITE], sizeof(int16_t) * ACCUMULATOR_NODES_PER_SIDE);
    memcpy(acc->accumulator[WHITE], refreshTable->accumulators[WHITE][kingSq_w].accumulator[WHITE], sizeof(uint8_t) * ACCUMULATOR_NODES_PER_SIDE);

    memcpy(acc->rawAccumulator[BLACK], refreshTable->accumulators[BLACK][kingSq_b].rawAccumulator[BLACK], sizeof(int16_t) * ACCUMULATOR_NODES_PER_SIDE);
    memcpy(acc->accumulator[BLACK], refreshTable->accumulators[BLACK][kingSq_b].accumulator[BLACK], sizeof(uint8_t) * ACCUMULATOR_NODES_PER_SIDE);
}

accumulatorRefreshTable* createRefreshTable()
{
    accumulatorRefreshTable* table = calloc(1, sizeof(accumulatorRefreshTable));
    for(int color = 0; color < 2; color++)
    {
        for(int newKSq = 0; newKSq < 64; newKSq++)
        {
            table->boards[color][newKSq] = create_board(NULL);
        }
    }
    return table;
}

void destroyRefreshTable(accumulatorRefreshTable* refreshTable)
{
    if(!refreshTable) return;

    for(int i = 0; i < 2; i++)
    {
        for(int j = 0; j < 64; j++)
        {
            free(refreshTable->boards[i][j]);
        }
    }
    free(refreshTable);

    refreshTable = NULL;
}