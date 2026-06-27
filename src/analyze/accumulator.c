#include "analyze/neuralnet.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "analyze/engine.h"
#include <math.h>
#include <immintrin.h>
#include <string.h>

accumulator* playerAccumulator = NULL;

//full refresh of raw values.
void calculateAccumulator(uint64_t* inputNodes, int16_t* outputValues, quantized_weights* weights)
{
    for (int outputIndex = 0; outputIndex < ACCUMULATOR_NODES_PER_SIDE; outputIndex+=16) 
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

    memset(inputs, 0, 2 * PIECE_COUNT * sizeof(uint64_t));

    int trackedPiecesPerColor = PIECE_COUNT / 2;
    for(int i = 0; i < PIECE_COUNT / 2; i++)
    {
        //White's perspective
        inputs[i] = board->pieces[2 * i];
        inputs[trackedPiecesPerColor + i] = board->pieces[2 * i + 1];

        //Black's perspective
        inputs[PIECE_COUNT + i] = FLIP_MASK(board->pieces[2 * i + 1]);
        inputs[PIECE_COUNT + trackedPiecesPerColor + i] = FLIP_MASK(board->pieces[2 * i]);
    }

    if(ISWHITE(color)) 
    {
        calculateAccumulator(inputs, acc->rawAccumulator[WHITE],  int_weights);
        activateAccumulator(acc->rawAccumulator[WHITE], acc->accumulator[WHITE]);
    }
    if(ISBLACK(color)) 
    {
        calculateAccumulator(&inputs[PIECE_COUNT], acc->rawAccumulator[BLACK], int_weights);
        activateAccumulator(acc->rawAccumulator[BLACK], acc->accumulator[BLACK]);
    }
}

void updateMoveAccumulator(bitboard* board, move_d lastMove, int shouldUndoMove, accumulator* acc)
{
    assert(board);
    assert(acc);
    assert(IS_VALID_MOVE(lastMove));

    for(int side = WHITE; side <= BLACK; side++)
    {
        /** Handle moving piece **/
        int fromSq, toSq, pieceOffset;
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

        if(side == BLACK)
        {
            toSq = FLIP_SQUARE(toSq);
            fromSq = FLIP_SQUARE(fromSq);
            pieceOffset = FLIP_COLOR(pieceOffset);
        }

        pieceOffset = ISWHITE(pieceOffset) ? PIECE(pieceOffset) / 2 :  (PIECE_COUNT / 2) + PIECE(pieceOffset) / 2;

        int inputNodeIndex = (PIECE_COUNT * side) + pieceOffset;
        
        int fromIdx, toIdx;
        if(lastMove.promoteTo) 
        {
            int relativeColor = (side == WHITE) ? COLOR(lastMove.piece) : FLIP_COLOR(lastMove.piece);
            int promotePieceOffset = ISWHITE(relativeColor) ? PIECE(lastMove.promoteTo) / 2 :  (PIECE_COUNT / 2) + PIECE(lastMove.promoteTo) / 2;
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

            capturedPieceOffset = ISWHITE(capturedPieceOffset) ? PIECE(capturedPieceOffset) / 2 :  (PIECE_COUNT / 2) + PIECE(capturedPieceOffset) / 2;
            int capturedInputNodeIndex = (PIECE_COUNT * side) + capturedPieceOffset;

            acc->inputNodes[capturedInputNodeIndex]^=(1ull<<capturedPieceSquare);

            int capIdx = (64 * capturedPieceOffset) + capturedPieceSquare;
            
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

        /** Handle castled rook **/
        int moveDistance = lastMove.endSquare - lastMove.startSquare;
        if(ISKING(lastMove.piece) && abs(moveDistance) == 2)
        {
            int castledRookFrom, castledRookTo;
            if(moveDistance == 2)
            {
                castledRookTo = lastMove.startSquare + 1;
                castledRookFrom = lastMove.startSquare + 3;
            }
            else
            {
                castledRookTo = lastMove.startSquare - 1;
                castledRookFrom = lastMove.startSquare - 4;
            }

            int castledRookOffset = ROOK | COLOR(lastMove.piece);
            if(side == BLACK)
            {
                castledRookOffset = FLIP_COLOR(castledRookOffset);
                castledRookFrom = FLIP_SQUARE(castledRookFrom);
                castledRookTo = FLIP_SQUARE(castledRookTo);
            }

            if(shouldUndoMove)
            {
                int temp = castledRookTo;
                castledRookTo = castledRookFrom;
                castledRookFrom = temp;
            }

            castledRookOffset = ISWHITE(castledRookOffset) ? PIECE(castledRookOffset) / 2 :  (PIECE_COUNT / 2) + PIECE(castledRookOffset) / 2;
            int castleInputNodeIndex = (PIECE_COUNT * side) + castledRookOffset;

            uint64_t xorMask = singleBitMask(castledRookFrom) | singleBitMask(castledRookTo);
            acc->inputNodes[castleInputNodeIndex]^=xorMask;

            int castledRookFromIdx = (64 * castledRookOffset) + castledRookFrom;
            int castledRoomToIdx = (64 * castledRookOffset) + castledRookTo;

            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j+=16)
            {
                __m256i v_acc   = _mm256_loadu_si256((__m256i const*)&acc->rawAccumulator[side][j]);
                __m256i v_to    = _mm256_loadu_si256((__m256i const*)&int_weights->weights1[castledRoomToIdx][j]);
                __m256i v_from  = _mm256_loadu_si256((__m256i const*)&int_weights->weights1[castledRookFromIdx][j]);
                
                v_acc = _mm256_adds_epi16(v_acc, v_to);
                v_acc = _mm256_subs_epi16(v_acc, v_from);
                
                _mm256_storeu_si256((__m256i*)&acc->rawAccumulator[side][j], v_acc);
            }
        }

        //Activate
        activateAccumulator(acc->rawAccumulator[side], acc->accumulator[side]);
    }

    //Keeping this commented out most of the time. 
    //It makes the engine slower but it is necessary for debugging.
    #ifndef NDEBUG
        accumulator realAccumValues = {0};
        loadInputAccumulator(board, &realAccumValues, WHITE);
        loadInputAccumulator(board, &realAccumValues, BLACK);
        assert(memcmp(&realAccumValues.inputNodes, &acc->inputNodes, sizeof(acc->inputNodes)) == 0);
        assert(memcmp(&realAccumValues.rawAccumulator, &acc->rawAccumulator, sizeof(acc->rawAccumulator)) == 0);
        assert(memcmp(&realAccumValues.accumulator, &acc->accumulator, sizeof(acc->accumulator)) == 0);
    #endif
}