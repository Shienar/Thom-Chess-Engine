#include "analyze/nnue/accumulator.h"
#include "analyze/nnue/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/search.h"
#include <math.h>
#include <immintrin.h>
#include <string.h>

//full refresh of raw values.
void calculateAccumulator(uint64_t* inputNodes, int16_t* outputValues, nnue_weights* weights)
{
    for(int outputIndex = 0; outputIndex < ACCUMULATOR_NODES_PER_SIDE; outputIndex+=16) 
    {
        __m256i v_output = _mm256_loadu_si256((const __m256i*)&weights->weights1_bias[outputIndex]);

        for(int piece = 0; piece < PIECE_COUNT; piece++)
        {
            uint64_t pieceMask = inputNodes[piece];
            int baseIndex = piece * 64;
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

void loadInputAccumulator(bitboard* board, accumulator* acc, int color)
{
    assert(board);
    assert(acc);
    
    uint64_t* inputs = acc->inputNodes;

    memset(inputs, 0, 2 * PIECE_COUNT * sizeof(uint64_t));
    
    for(int i = 0; i < PIECE_TYPE_COUNT; i++)
    {
        //White's perspective
        inputs[i] = board->pieces[2 * i];
        inputs[PIECE_TYPE_COUNT + i] = board->pieces[2 * i + 1];

        //Black's perspective
        inputs[PIECE_COUNT + i] = FLIP_MASK(board->pieces[2 * i + 1]);
        inputs[PIECE_COUNT + PIECE_TYPE_COUNT + i] = FLIP_MASK(board->pieces[2 * i]);
    }

    if(ISWHITE(color)) 
        calculateAccumulator(&inputs[0], acc->rawAccumulator[WHITE],  weights);
    if(ISBLACK(color))
        calculateAccumulator(&inputs[PIECE_COUNT], acc->rawAccumulator[BLACK], weights);
}

void updateMoveAccumulator(bitboard* board, move_d lastMove, int shouldUndoMove, accumulator* acc)
{
    assert(board);
    assert(acc);

    move_c compactMove = (move_c)lastMove.compactMove;
    assert(IS_VALID_MOVE(compactMove));

    for(int side = WHITE; side <= BLACK; side++)
    {
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

        pieceOffset = ISWHITE(pieceOffset) ? PIECE(pieceOffset) / 2 :  (PIECE_TYPE_COUNT) + PIECE(pieceOffset) / 2;

        int inputNodeIndex = (PIECE_COUNT * side) + pieceOffset;
        
        int fromIdx, toIdx;
        if(compactMove.promoteTo) 
        {
            int relativeColor = (side == WHITE) ? COLOR(lastMove.piece) : FLIP_COLOR(lastMove.piece);
            int promotePieceOffset = ISWHITE(relativeColor) ? PIECE(compactMove.promoteTo) / 2 :  (PIECE_TYPE_COUNT) + PIECE(compactMove.promoteTo) / 2;
            int promoteInputNodeIndex = (PIECE_COUNT * side) + promotePieceOffset;
            
            if(shouldUndoMove)
            {
                uint64_t xorMask_promote = singleBitMask(fromSq);
                uint64_t xorMask_pawn = singleBitMask(toSq);

                acc->inputNodes[inputNodeIndex] ^= xorMask_pawn;
                acc->inputNodes[promoteInputNodeIndex] ^= xorMask_promote;
                
                fromIdx = (64 * promotePieceOffset) + fromSq;
                toIdx = (64 * pieceOffset) + toSq;
            }
            else
            {
                uint64_t xorMask_promote = singleBitMask(toSq);
                uint64_t xorMask_pawn = singleBitMask(fromSq);

                acc->inputNodes[inputNodeIndex] ^= xorMask_pawn;
                acc->inputNodes[promoteInputNodeIndex] ^= xorMask_promote;
                
                fromIdx = (64 * pieceOffset) + fromSq;
                toIdx = (64 * promotePieceOffset) + toSq;
            }
        }
        else
        {
            
            uint64_t xorMask = singleBitMask(fromSq) | singleBitMask(toSq);

            acc->inputNodes[inputNodeIndex]^=xorMask;

            fromIdx = (64 * pieceOffset) + fromSq;
            toIdx = (64 * pieceOffset) + toSq;

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

            capturedPieceOffset = ISWHITE(capturedPieceOffset) ? PIECE(capturedPieceOffset) / 2 :  (PIECE_TYPE_COUNT) + PIECE(capturedPieceOffset) / 2;
            int capturedInputNodeIndex = (PIECE_COUNT * side) + capturedPieceOffset;

            acc->inputNodes[capturedInputNodeIndex]^=singleBitMask(capturedPieceSquare);

            int capIdx = (64 * capturedPieceOffset) + capturedPieceSquare;
            
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
        if(ISKING(lastMove.piece) && abs(compactMove.startSquare - (compactMove.endSquare) == 2))
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

            if(shouldUndoMove)
            {
                int temp = castledRookFrom;
                castledRookFrom = castledRookTo;
                castledRookTo = temp;
            }
            
            castledRookOffset = ISWHITE(castledRookOffset) ? PIECE(castledRookOffset) / 2 :  (PIECE_TYPE_COUNT) + PIECE(castledRookOffset) / 2;

            int inputNodeIndex = (PIECE_COUNT * side) + pieceOffset;
            
            uint64_t xorMask = singleBitMask(fromSq) | singleBitMask(toSq);

            acc->inputNodes[inputNodeIndex]^=xorMask;

            int castledRookFromIdx = (64 * castledRookOffset) + castledRookFrom;
            int castledRookToIdx = (64 * castledRookOffset) + castledRookTo;

            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=16)
            {
                __m256i v_acc   = _mm256_loadu_si256((__m256i const*)&acc->rawAccumulator[side][j]);
                __m256i v_to    = _mm256_loadu_si256((__m256i const*)&weights->weights1[castledRookFromIdx][j]);
                __m256i v_from  = _mm256_loadu_si256((__m256i const*)&weights->weights1[castledRookToIdx][j]);
                
                v_acc = _mm256_adds_epi16(v_acc, v_to);
                v_acc = _mm256_subs_epi16(v_acc, v_from);
                
                _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[side][j], v_acc);
            }
        }
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
    #endif
    */
}