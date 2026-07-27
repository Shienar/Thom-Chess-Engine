#include "analyze/nnue/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/search.h"
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

int neighboringKingBuckets[KING_BUCKETS][5] = {
    {1, 4, 1, 4, 1},
    {0, 2, 4, 5, 2},
    {1, 3, 4, 5, 3},
    {2, 5, 2, 5, 1},
    {0, 1, 2, 5, 6},
    {1, 2, 3, 4, 6},
    {4, 5, 7, 4, 5},
    {6, 8, 6, 8, 6},
    {7, 9, 7, 9, 7},
    {8, 8, 8, 8, 8}
};

//full refresh of raw values.
void calculateAccumulator(uint64_t* inputNodes, int16_t* outputValues, quantized_weights* weights, int kingBucketOffset)
{
    for(int outputIndex = 0; outputIndex < ACCUMULATOR_NODES_PER_SIDE; outputIndex+=16) 
    {
        __m256i v_output = _mm256_loadu_si256((const __m256i*)&weights->weights1_bias[outputIndex]);

        for(int piece = 0; piece < PIECE_COUNT; piece++)
        {
            uint64_t pieceMask = inputNodes[piece];
            int baseIndex = kingBucketOffset + piece * 64;
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

    for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 32) 
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
        v_low = _mm256_srli_epi32(v_low, QA_RSHIFT);
        v_high = _mm256_srli_epi32(v_high, QA_RSHIFT);

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
        v_low = _mm256_srli_epi32(v_low, QA_RSHIFT);
        v_high = _mm256_srli_epi32(v_high, QA_RSHIFT);

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
    
    int baseIndex_w = PIECE_COUNT*kingBuckets[board->kingSquare[WHITE]];
    int baseIndex_b = BITBOARDS_PER_INPUT_SIDE + PIECE_COUNT*kingBuckets[FLIP_SQUARE(board->kingSquare[BLACK])];
    int trackedPiecesPerColor = PIECE_TYPE_COUNT;
    for(int i = 0; i < PIECE_TYPE_COUNT; i++)
    {
        //White's perspective
        inputs[baseIndex_w + i] = board->pieces[2 * i];
        inputs[baseIndex_w + trackedPiecesPerColor + i] = board->pieces[2 * i + 1];

        //Black's perspective
        inputs[baseIndex_b + i] = FLIP_MASK(board->pieces[2 * i + 1]);
        inputs[baseIndex_b + trackedPiecesPerColor + i] = FLIP_MASK(board->pieces[2 * i]);
    }

    //Mirroring
    if(getColumn(board->kingSquare[WHITE]) > 3)
        for(int p = 0; p < PIECE_COUNT; p++) inputs[baseIndex_w + p] = mirrorBoard(inputs[baseIndex_w + p]);
    if(getColumn(board->kingSquare[BLACK]) > 3)
        for(int p = 0; p < PIECE_COUNT; p++) inputs[baseIndex_b + p] = mirrorBoard(inputs[baseIndex_b + p]);

    if(ISWHITE(color)) 
    {
        calculateAccumulator(&inputs[baseIndex_w], acc->rawAccumulator[WHITE],  int_weights, BITS_PER_KING_BUCKET * kingBuckets[board->kingSquare[WHITE]]);
        activateAccumulator(acc->rawAccumulator[WHITE], acc->accumulator[WHITE]);
    }
    if(ISBLACK(color)) 
    {
        calculateAccumulator(&inputs[baseIndex_b], acc->rawAccumulator[BLACK], int_weights, BITS_PER_KING_BUCKET * kingBuckets[FLIP_SQUARE(board->kingSquare[BLACK])]);
        activateAccumulator(acc->rawAccumulator[BLACK], acc->accumulator[BLACK]);
    }
}

void updateMoveAccumulator(bitboard* board, move_d lastMove, int shouldUndoMove, accumulator* acc, accumulatorRefreshTable* refreshTable)
{
    assert(board);
    assert(acc);
    assert(IS_VALID_MOVE(lastMove));

    if(ISKING(lastMove.piece)) 
    {
        updateAccumulatorFromTable(board, acc, refreshTable);
        return;
    }

    for(int side = WHITE; side <= BLACK; side++)
    {
        /** Handle moving piece **/
        int fromSq, toSq;
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
        int pieceOffset = lastMove.piece;
        
        if(side == BLACK)
        {
            toSq = FLIP_SQUARE(toSq);
            fromSq = FLIP_SQUARE(fromSq);
            pieceOffset = FLIP_COLOR(pieceOffset);
        }
        int ksq = (side == WHITE) ? board->kingSquare[WHITE] : FLIP_SQUARE(board->kingSquare[BLACK]);

        //Mirroring
        if(getColumn(ksq) > 3) { fromSq = MIRROR_SQUARE(fromSq); toSq = MIRROR_SQUARE(toSq); }

        pieceOffset = ISWHITE(pieceOffset) ? PIECE(pieceOffset) / 2 :  (PIECE_TYPE_COUNT) + PIECE(pieceOffset) / 2;

        int inputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * side) + (PIECE_COUNT * kingBuckets[ksq]) + pieceOffset;
        
        int fromIdx, toIdx;
        if(lastMove.promoteTo) 
        {
            int relativeColor = (side == WHITE) ? COLOR(lastMove.piece) : FLIP_COLOR(lastMove.piece);
            int promotePieceOffset = ISWHITE(relativeColor) ? PIECE(lastMove.promoteTo) / 2 :  (PIECE_TYPE_COUNT) + PIECE(lastMove.promoteTo) / 2;
            int promoteInputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * side) + (PIECE_COUNT * kingBuckets[ksq]) + promotePieceOffset;
            
            if(shouldUndoMove)
            {
                uint64_t xorMask_promote = singleBitMask(fromSq);
                uint64_t xorMask_pawn = singleBitMask(toSq);

                acc->inputNodes[inputNodeIndex] ^= xorMask_pawn;
                acc->inputNodes[promoteInputNodeIndex] ^= xorMask_promote;
                
                fromIdx = (64 * ((PIECE_COUNT * kingBuckets[ksq]) + promotePieceOffset)) + fromSq;
                toIdx = (64 * ((PIECE_COUNT * kingBuckets[ksq]) + pieceOffset)) + toSq;
            }
            else
            {
                uint64_t xorMask_promote = singleBitMask(toSq);
                uint64_t xorMask_pawn = singleBitMask(fromSq);

                acc->inputNodes[inputNodeIndex] ^= xorMask_pawn;
                acc->inputNodes[promoteInputNodeIndex] ^= xorMask_promote;
                
                fromIdx = (64 * ((PIECE_COUNT * kingBuckets[ksq]) + pieceOffset)) + fromSq;
                toIdx = (64 * ((PIECE_COUNT * kingBuckets[ksq]) + promotePieceOffset)) + toSq;
            }
        }
        else
        {
            
            uint64_t xorMask = singleBitMask(fromSq) | singleBitMask(toSq);

            acc->inputNodes[inputNodeIndex]^=xorMask;

            fromIdx = (64 * ((PIECE_COUNT * kingBuckets[ksq]) + pieceOffset)) + fromSq;
            toIdx = (64 * ((PIECE_COUNT * kingBuckets[ksq]) + pieceOffset)) + toSq;

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

        /** Handle captured piece **/
        if(lastMove.capturedPiece != EMPTY_PIECE)
        {
            int capturedPieceOffset = lastMove.capturedPiece;
            int capturedPieceSquare = lastMove.endSquare;

            if(ISPAWN(lastMove.piece) && ((!shouldUndoMove && lastMove.endSquare == lastMove.prevEnPassantSquare) || (shouldUndoMove && lastMove.endSquare == board->enPassantSquare)))
            {
                if(ISWHITE(lastMove.piece)) capturedPieceSquare -=8;
                else capturedPieceSquare +=8;
            }

            if(side == BLACK)
            {
                capturedPieceOffset = FLIP_COLOR(capturedPieceOffset);
                capturedPieceSquare = FLIP_SQUARE(capturedPieceSquare);
            }
            
            if(getColumn(ksq) > 3) capturedPieceSquare = MIRROR_SQUARE(capturedPieceSquare);

            capturedPieceOffset = ISWHITE(capturedPieceOffset) ? PIECE(capturedPieceOffset) / 2 :  (PIECE_TYPE_COUNT) + PIECE(capturedPieceOffset) / 2;
            int capturedInputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * side) + (PIECE_COUNT * kingBuckets[ksq]) + capturedPieceOffset;

            acc->inputNodes[capturedInputNodeIndex]^=singleBitMask(capturedPieceSquare);

            int capIdx = (64 * ((PIECE_COUNT * kingBuckets[ksq]) + capturedPieceOffset)) + capturedPieceSquare;
            
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


        //Activate
        activateAccumulator(acc->rawAccumulator[side], acc->accumulator[side]);
    }

    //Keeping this commented out most of the time.
    //It makes the engine slower but it is necessary for debugging.
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
    assert(currentBoard->kingSquare[color] == accumulatorBoard->kingSquare[color]);

    uint64_t curBoard[PIECE_COUNT] = {0};
    uint64_t accumBoard[PIECE_COUNT] = {0};

    int ksq; 
    int trackedPiecesPerColor = PIECE_TYPE_COUNT;
    if(ISWHITE(color))
    {
        ksq = accumulatorBoard->kingSquare[WHITE];
        for(int i = 0; i < PIECE_TYPE_COUNT; i++)
        {
            curBoard[i] = currentBoard->pieces[2 * i];
            curBoard[i + trackedPiecesPerColor] = currentBoard->pieces[2 * i + 1];

            accumBoard[i] = accumulatorBoard->pieces[2 * i];
            accumBoard[i + trackedPiecesPerColor] = accumulatorBoard->pieces[2 * i + 1];
        }
    }
    else
    {
        ksq = FLIP_SQUARE(accumulatorBoard->kingSquare[BLACK]);
        for(int i = 0; i < PIECE_TYPE_COUNT; i++)
        {
            curBoard[i] = FLIP_MASK(currentBoard->pieces[2 * i + 1]);
            curBoard[trackedPiecesPerColor + i] = FLIP_MASK(currentBoard->pieces[2 * i]);
            
            accumBoard[i] = FLIP_MASK(accumulatorBoard->pieces[2 * i + 1]);
            accumBoard[trackedPiecesPerColor + i] = FLIP_MASK(accumulatorBoard->pieces[2 * i]);
        }
    }

    //Mirroring
    if(getColumn(ksq) > 3)
    {
        for(int i = 0; i < PIECE_COUNT; i++)
        {
            curBoard[i] = mirrorBoard(curBoard[i]);
            accumBoard[i] = mirrorBoard(accumBoard[i]);
        }
    }

    for(int piece = 0; piece < PIECE_COUNT; piece++)
    {
        uint64_t difference = curBoard[piece]^accumBoard[piece];

        //Input node updates
        int inputNodeIndex = (PIECE_COUNT * kingBuckets[ksq]) + piece;
        if(ISBLACK(color)) inputNodeIndex+=BITBOARDS_PER_INPUT_SIDE;

        acc->inputNodes[inputNodeIndex]^=difference;

        while(difference)
        {
            int square = __builtin_ctzll(difference);

            int featureIndex = (64 * (PIECE_COUNT * kingBuckets[ksq] + piece)) + square;

            char wasAdded = curBoard[piece]&singleBitMask(square) ? 1 : 0;

            int16_t* targetAcc = acc->rawAccumulator[color];
            int16_t* targetWeights = int_weights->weights1[featureIndex];

            if(wasAdded) 
            {
                for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 16) 
                {
                    __m256i v_acc = _mm256_loadu_si256((__m256i const*)&targetAcc[i]);
                    __m256i v_weight = _mm256_loadu_si256((__m256i const*)&targetWeights[i]);
                    
                    v_acc = _mm256_adds_epi16(v_acc, v_weight);
                    
                    _mm256_storeu_si256((__m256i*)&targetAcc[i], v_acc);
                }
            } 
            else 
            {
                for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i += 16) 
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
    int kingSq_w = board->kingSquare[WHITE];
    int kingSq_b = board->kingSquare[BLACK];

    //Update the accumulator & board in table to match given board.
    if(!refreshTable->initialized[WHITE][kingSq_w])
    {
        memcpy(refreshTable->boards[WHITE][kingSq_w], board, sizeof(bitboard));
        loadInputAccumulator(refreshTable->boards[WHITE][kingSq_w], &refreshTable->accumulators[WHITE][kingSq_w], WHITE);
        loadInputAccumulator(refreshTable->boards[WHITE][kingSq_w], &refreshTable->accumulators[WHITE][kingSq_w], BLACK);
        refreshTable->initialized[WHITE][kingSq_w] = 1;
    }
    else updateBoardAccumulator(board, refreshTable->boards[WHITE][kingSq_w], &refreshTable->accumulators[WHITE][kingSq_w], WHITE);

    if(!refreshTable->initialized[BLACK][kingSq_b])
    {
        memcpy(refreshTable->boards[BLACK][kingSq_b], board, sizeof(bitboard));
        loadInputAccumulator(refreshTable->boards[BLACK][kingSq_b], &refreshTable->accumulators[BLACK][kingSq_b], WHITE);
        loadInputAccumulator(refreshTable->boards[BLACK][kingSq_b], &refreshTable->accumulators[BLACK][kingSq_b], BLACK);
        refreshTable->initialized[BLACK][kingSq_b] = 1;
    }
    else updateBoardAccumulator(board, refreshTable->boards[BLACK][kingSq_b], &refreshTable->accumulators[BLACK][kingSq_b], BLACK);
    
    //Copy updated table values back to board.

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