#include "analyze/nnue/accumulator.h"
#include "analyze/nnue/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/search.h"
#include <math.h>
#include <immintrin.h>
#include <string.h>

/**
 * - Index X contains the king bucket index at square X.
 * - Created for white's perspective; must be flipped for black.
 * - Mirrored accross middle of board, split between d/e files.
 *      - When king column > d, the squares & masks of all pieces get mirrored.
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
void calculateAccumulator(uint64_t* inputNodes, int16_t* outputValues, int kingBucketOffset)
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

void loadInputAccumulator(bitboard* board, accumulator* acc)
{
    assert(board);
    assert(acc);
    
    uint64_t* inputs = acc->inputNodes;

    memset(inputs, 0, 2 * BITBOARDS_PER_INPUT_SIDE * sizeof(uint64_t));
    
    int whiteBucket = kingBuckets[board->kingSquare[WHITE]];
    int blackBucket = kingBuckets[FLIP_SQUARE(board->kingSquare[BLACK])];

    int baseIndex_w = PIECE_COUNT * whiteBucket;
    int baseIndex_b = BITBOARDS_PER_INPUT_SIDE + PIECE_COUNT * blackBucket;

    for(int i = 0; i < PIECE_TYPE_COUNT; i++)
    {
        //White's perspective
        inputs[baseIndex_w + i] = board->pieces[2 * i];
        inputs[baseIndex_w + PIECE_TYPE_COUNT + i] = board->pieces[2 * i + 1];

        //Black's perspective
        inputs[baseIndex_b + i] = FLIP_MASK(board->pieces[2 * i + 1]);
        inputs[baseIndex_b + PIECE_TYPE_COUNT + i] = FLIP_MASK(board->pieces[2 * i]);
    }
    
    if(getColumn(board->kingSquare[WHITE]) > 3)
        for(int p = 0; p < PIECE_COUNT; p++) 
            inputs[baseIndex_w + p] = mirrorBoard(inputs[baseIndex_w + p]);
    if(getColumn(board->kingSquare[BLACK]) > 3)
        for(int p = 0; p < PIECE_COUNT; p++) 
            inputs[baseIndex_b + p] = mirrorBoard(inputs[baseIndex_b + p]);

    calculateAccumulator(&inputs[baseIndex_w], acc->rawAccumulator[WHITE], BITS_PER_KING_BUCKET * whiteBucket);
    calculateAccumulator(&inputs[baseIndex_b], acc->rawAccumulator[BLACK], BITS_PER_KING_BUCKET * blackBucket);
}

void updateMoveAccumulator(bitboard* board, move_d lastMove, int shouldUndoMove, accumulator* acc, accumulatorRefreshTable* refreshTable)
{
    assert(board);
    assert(acc);

    move_c compactMove = (move_c)lastMove.compactMove;
    assert(IS_VALID_MOVE(compactMove));

    if(ISKING(lastMove.piece) &&
        (((getColumn(compactMove.startSquare) > 3) != (getColumn(compactMove.endSquare) > 3)) ||
         (ISWHITE(lastMove.piece) && kingBuckets[compactMove.startSquare] != kingBuckets[compactMove.endSquare]) ||
         (ISBLACK(lastMove.piece) && kingBuckets[FLIP_SQUARE(compactMove.startSquare)] != kingBuckets[FLIP_SQUARE(compactMove.endSquare)])))
         {
            updateAccumulatorFromTable(board, acc, refreshTable);
            return;
         }


    for(int side = WHITE; side <= BLACK; side++)
    {
        int ksq = (side == WHITE) ? board->kingSquare[WHITE] : FLIP_SQUARE(board->kingSquare[BLACK]);

        /** Handle moving piece **/
        int fromSq, toSq;
        if(shouldUndoMove)
        {
            toSq = compactMove.startSquare;
            fromSq = compactMove.endSquare;
        }
        else
        {
            fromSq = compactMove.startSquare;
            toSq = compactMove.endSquare;
        }
        int pieceOffset = lastMove.piece;
        
        if(side == BLACK)
        {
            toSq = FLIP_SQUARE(toSq);
            fromSq = FLIP_SQUARE(fromSq);
            pieceOffset = FLIP_COLOR(pieceOffset);
        }
        
        //Mirroring
        if(getColumn(ksq) > 3) { fromSq = MIRROR_SQUARE(fromSq); toSq = MIRROR_SQUARE(toSq); }

        pieceOffset = ISWHITE(pieceOffset) ? PIECE(pieceOffset) / 2 :  (PIECE_TYPE_COUNT) + PIECE(pieceOffset) / 2;

        int inputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * side) + (PIECE_COUNT * kingBuckets[ksq]) + pieceOffset;
        
        int fromIdx, toIdx;
        if(compactMove.promoteTo) 
        {
            int relativeColor = (side == WHITE) ? COLOR(lastMove.piece) : FLIP_COLOR(lastMove.piece);
            int promotePieceOffset = ISWHITE(relativeColor) ? PIECE(compactMove.promoteTo) / 2 :  (PIECE_TYPE_COUNT) + PIECE(compactMove.promoteTo) / 2;
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
            __m256i v_to    = _mm256_loadu_si256((__m256i const*)&weights->weights1[toIdx][j]);
            __m256i v_from  = _mm256_loadu_si256((__m256i const*)&weights->weights1[fromIdx][j]);
            
            v_acc = _mm256_adds_epi16(v_acc, v_to);
            v_acc = _mm256_subs_epi16(v_acc, v_from);
            
            _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[side][j], v_acc);
        }

        /** Handle captured piece **/
        if(lastMove.capturedPiece != EMPTY_PIECE)
        {
            int capturedPieceOffset = lastMove.capturedPiece;
            int capturedPieceSquare = compactMove.endSquare;

            if(ISPAWN(lastMove.piece) && ((!shouldUndoMove && compactMove.endSquare == lastMove.prevEnPassantSquare) || (shouldUndoMove && compactMove.endSquare == board->enPassantSquare)))
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
                    __m256i v_cap = _mm256_loadu_si256((__m256i const*)&weights->weights1[capIdx][j]);
                    
                    v_acc = _mm256_adds_epi16(v_acc, v_cap);
                    
                    _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[side][j], v_acc);
                }
            }
            else
            {
                for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=16)
                {
                    __m256i v_acc = _mm256_loadu_si256((__m256i const*)&acc->rawAccumulator[side][j]);
                    __m256i v_cap = _mm256_loadu_si256((__m256i const*)&weights->weights1[capIdx][j]);
                    
                    v_acc = _mm256_subs_epi16(v_acc, v_cap);
                    
                    _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[side][j], v_acc);
                }
            }
        }

        /** Handle castled rook (Doesn't support Chess960) **/
        if(ISKING(lastMove.piece) && abs(compactMove.startSquare - compactMove.endSquare) == 2)
        {
            int castledRookFrom, castledRookTo;
            int castledRookOffset = ROOK | COLOR(lastMove.piece);
            if(getColumn(compactMove.endSquare) == 6)
            {
                //Kingside Castle
                castledRookFrom = compactMove.startSquare + 3;
                castledRookTo = compactMove.startSquare + 1;
            }
            else
            {
                //Queenside Castle
                castledRookFrom = compactMove.startSquare - 4;
                castledRookTo = compactMove.startSquare - 1;
            }
            
            if(side == BLACK)
            {
                castledRookOffset = FLIP_COLOR(castledRookOffset);
                castledRookFrom = FLIP_SQUARE(castledRookFrom);
                castledRookTo = FLIP_SQUARE(castledRookTo);
            }

            if(getColumn(ksq) > 3) 
            { 
                castledRookFrom = MIRROR_SQUARE(castledRookFrom); 
                castledRookTo = MIRROR_SQUARE(castledRookTo); 
            }

            if(shouldUndoMove)
            {
                int temp = castledRookFrom;
                castledRookFrom = castledRookTo;
                castledRookTo = temp;
            }
            
            castledRookOffset = ISWHITE(castledRookOffset) ? PIECE(castledRookOffset) / 2 :  (PIECE_TYPE_COUNT) + PIECE(castledRookOffset) / 2;

            int inputNodeIndex = (BITBOARDS_PER_INPUT_SIDE * side) + (PIECE_COUNT * kingBuckets[ksq]) + castledRookOffset;
            
            uint64_t xorMask = singleBitMask(castledRookFrom) | singleBitMask(castledRookTo);

            acc->inputNodes[inputNodeIndex]^=xorMask;

            int castledRookFromIdx = (64 * ((PIECE_COUNT * kingBuckets[ksq]) + castledRookOffset)) + castledRookFrom;
            int castledRookToIdx = (64 * ((PIECE_COUNT * kingBuckets[ksq]) + castledRookOffset)) + castledRookTo;

            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=16)
            {
                __m256i v_acc   = _mm256_loadu_si256((__m256i const*)&acc->rawAccumulator[side][j]);
                __m256i v_to    = _mm256_loadu_si256((__m256i const*)&weights->weights1[castledRookToIdx][j]);
                __m256i v_from  = _mm256_loadu_si256((__m256i const*)&weights->weights1[castledRookFromIdx][j]);
                
                v_acc = _mm256_adds_epi16(v_acc, v_to);
                v_acc = _mm256_subs_epi16(v_acc, v_from);
                
                _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[side][j], v_acc);
            }
        }
    }

    //Compare the efficient updates against a full refresh.
    //Commented out unless its being explicitly tested.
    /*
    #ifndef NDEBUG
        accumulator realAccumValues = {0};
        loadInputAccumulator(board, &realAccumValues);
        assert(memcmp(&realAccumValues.inputNodes, &acc->inputNodes, sizeof(acc->inputNodes)) == 0);
        assert(memcmp(&realAccumValues.rawAccumulator, &acc->rawAccumulator, sizeof(acc->rawAccumulator)) == 0);
    #endif
    */
}

void updateBoardAccumulator(bitboard* currentBoard, bitboard* accumulatorBoard, accumulator* acc, int color)
{
    assert(currentBoard);
    assert(accumulatorBoard);
    assert(acc);
    assert((ISWHITE(color) && kingBuckets[currentBoard->kingSquare[color]] == kingBuckets[accumulatorBoard->kingSquare[color]]) ||
           (ISBLACK(color) && kingBuckets[FLIP_SQUARE(currentBoard->kingSquare[color])] == kingBuckets[FLIP_SQUARE(accumulatorBoard->kingSquare[color])]));
    assert((getColumn(currentBoard->kingSquare[color]) > 3) == (getColumn(accumulatorBoard->kingSquare[color]) > 3));

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
            int16_t* targetWeights = weights->weights1[featureIndex];

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
                    
                    v_acc = _mm256_subs_epi16(v_acc, v_w);
                    
                    _mm256_storeu_si256((__m256i*)&targetAcc[i], v_acc);
                }
            }

            difference&=(difference - 1);
        }
    }

    //We don't need the larger game rule based bitboard arrays.
    //Just copy in what will be used.
    memcpy(accumulatorBoard->pieces, currentBoard->pieces, sizeof(currentBoard->pieces));
    memcpy(accumulatorBoard->kingSquare, currentBoard->kingSquare, sizeof(currentBoard->kingSquare));
}

void updateAccumulatorFromTable(bitboard* board, accumulator* acc,  accumulatorRefreshTable* refreshTable)
{
    assert(board);
    assert(acc);
    assert(refreshTable);

    //Black square is flipped for weight index tracking, but not here in refresh table indices.
    int kingSq_w = board->kingSquare[WHITE];
    int kingSq_b = FLIP_SQUARE(board->kingSquare[BLACK]);

    int bucketIndex_w = kingBuckets[kingSq_w];
    int bucketIndex_b = kingBuckets[kingSq_b];

    if(getColumn(kingSq_w) > 3)
        bucketIndex_w += KING_BUCKETS;
    if(getColumn(kingSq_b) > 3)
        bucketIndex_b += KING_BUCKETS;

    //Update the accumulator & board in table to match given board.
    if(!refreshTable->initialized[WHITE][bucketIndex_w])
    {
        memcpy(refreshTable->boards[WHITE][bucketIndex_w]->pieces, board->pieces, sizeof(board->pieces));
        memcpy(refreshTable->boards[WHITE][bucketIndex_w]->kingSquare, board->kingSquare, sizeof(board->kingSquare));
        loadInputAccumulator(refreshTable->boards[WHITE][bucketIndex_w], &refreshTable->accumulators[WHITE][bucketIndex_w]);
        refreshTable->initialized[WHITE][bucketIndex_w] = 1;
    }
    else updateBoardAccumulator(board, refreshTable->boards[WHITE][bucketIndex_w], &refreshTable->accumulators[WHITE][bucketIndex_w], WHITE);

    if(!refreshTable->initialized[BLACK][bucketIndex_b])
    {
        memcpy(refreshTable->boards[BLACK][bucketIndex_b]->pieces, board->pieces, sizeof(board->pieces));
        memcpy(refreshTable->boards[BLACK][bucketIndex_b]->kingSquare, board->kingSquare, sizeof(board->kingSquare));
        loadInputAccumulator(refreshTable->boards[BLACK][bucketIndex_b], &refreshTable->accumulators[BLACK][bucketIndex_b]);
        refreshTable->initialized[BLACK][bucketIndex_b] = 1;
    }
    else updateBoardAccumulator(board, refreshTable->boards[BLACK][bucketIndex_b], &refreshTable->accumulators[BLACK][bucketIndex_b], BLACK);
    
    //Copy updated table values back to board.

    memcpy(&acc->inputNodes, &refreshTable->accumulators[WHITE][bucketIndex_w].inputNodes, sizeof(uint64_t) * BITBOARDS_PER_INPUT_SIDE);
    memcpy(&acc->inputNodes[BITBOARDS_PER_INPUT_SIDE], &refreshTable->accumulators[BLACK][bucketIndex_b].inputNodes[BITBOARDS_PER_INPUT_SIDE], sizeof(uint64_t) * BITBOARDS_PER_INPUT_SIDE);

    memcpy(acc->rawAccumulator[WHITE], refreshTable->accumulators[WHITE][bucketIndex_w].rawAccumulator[WHITE], sizeof(int16_t) * ACCUMULATOR_NODES_PER_SIDE);
    memcpy(acc->rawAccumulator[BLACK], refreshTable->accumulators[BLACK][bucketIndex_b].rawAccumulator[BLACK], sizeof(int16_t) * ACCUMULATOR_NODES_PER_SIDE);
}

accumulatorRefreshTable* createRefreshTable()
{
    accumulatorRefreshTable* table = calloc(1, sizeof(accumulatorRefreshTable));
    for(int color = 0; color < 2; color++)
        for(int newKSq = 0; newKSq < 2 * KING_BUCKETS; newKSq++)
            table->boards[color][newKSq] = create_board(NULL);
    return table;
}

void destroyRefreshTable(accumulatorRefreshTable* refreshTable)
{
    if(!refreshTable) return;

    for(int i = 0; i < 2; i++)
        for(int j = 0; j < 2 * KING_BUCKETS; j++)
            free(refreshTable->boards[i][j]);
    free(refreshTable);
}