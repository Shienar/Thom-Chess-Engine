#include "neuralnet.h"
#include "../debug.h"
#include "../board/bitboard.h"
#include "../board/moves.h"
#include "engine.h"
#include <math.h>
#include <float.h>

void extractInputLayerToArray(uint64_t* inputLayerCompact_w, uint64_t* inputLayerCompact_b, float* inputLayerFloats, int8_t* inputLayerBytes)
{
    if(!inputLayerCompact_w || !inputLayerCompact_b)
    {
        DEBUG("Cannot extract null input layer.");
        return;
    }
    else if(inputLayerFloats)
    {
        for(int i = 0; i < 640; i++)
        {
            if((inputLayerCompact_w[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerFloats[i] = 1.0;
            else inputLayerFloats[i] = 0.0;

            if((inputLayerCompact_b[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerFloats[640 + i] = 1.0;
            else inputLayerFloats[640 + i] = 0.0;
        }
    }
    else if(inputLayerBytes)
    {
        for(int i = 0; i < 640; i++)
        {
            if((inputLayerCompact_w[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerBytes[i] = 1;
            else inputLayerBytes[i] = 0;

            if((inputLayerCompact_b[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerBytes[640 + i] = 1;
            else inputLayerBytes[640 + i] = 0;
        }
    }
    else
    {
        DEBUG("No provided destination for extraction.");
    }
}

void loadInputAccumulator(bitboard* board, accumulator_playing* byteAccumulator, accumulator_training* floatAccumulator)
{
    if(!board)
    {
        DEBUG("Cannot load null board into accumulator.");
        return;
    }
    else if(!byteAccumulator && !floatAccumulator)
    {
        DEBUG("Cannot load null accumulator.");
        return;
    }
    else if(byteAccumulator && !playerNNUE) load_playingWeights();
    else if(floatAccumulator && !trainingNNUE) load_trainingWeights();
    

    uint64_t* inputs = NULL;
    if(byteAccumulator) inputs = byteAccumulator->inputNodes;
    else inputs = floatAccumulator->inputNodes;
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
        extractInputLayerToArray(&inputs[baseIndex_w], &inputs[baseIndex_b], NULL, inputArray);

        calculateLayer_IntBytes(inputArray, byteAccumulator->accumulator[0], 640, ACCUMULATOR_NODES_PER_SIDE, &playerNNUE->weights1[extendedBaseIndex_w], playerNNUE->weights1_bias, 0);
        calculateLayer_IntBytes(&inputArray[640], byteAccumulator->accumulator[1], 640, ACCUMULATOR_NODES_PER_SIDE, &playerNNUE->weights1[extendedBaseIndex_b], playerNNUE->weights1_bias, 0);
    }
    else
    {
        float inputArray[1280];
        extractInputLayerToArray(&inputs[baseIndex_w], &inputs[baseIndex_b], inputArray, NULL);

        calculateLayer_Floats(inputArray, floatAccumulator->accumulator[0], 640, ACCUMULATOR_NODES_PER_SIDE, &trainingNNUE->weights1[extendedBaseIndex_w], trainingNNUE->weights1_bias, 0);
        calculateLayer_Floats(&inputArray[640], floatAccumulator->accumulator[1], 640, ACCUMULATOR_NODES_PER_SIDE, &trainingNNUE->weights1[extendedBaseIndex_b], trainingNNUE->weights1_bias, 0);
    }
}

void updateMoveAccumulator(bitboard* board, move* lastMove, int shouldUndoMove, accumulator_playing* byteAccumulator, accumulator_training* floatAccumulator)
{
    if(!lastMove)
    {
        DEBUG("Cannot load null move.");
        return;
    }
    else if(!byteAccumulator && !floatAccumulator)
    {
        DEBUG("Cannot load null accumulator.");
        return;
    }
    else if(byteAccumulator && !playerNNUE) load_playingWeights();
    else if(floatAccumulator && !trainingNNUE) load_trainingWeights();

    if(ISKING(lastMove->piece))
    {
        loadInputAccumulator(board, byteAccumulator, floatAccumulator);
        return;
    }

    for(int i = 0; i < 2; i++)
    {
        int ksq, fromSq, toSq, pieceOffset = 0;
        if(i == 0)
        {
            ksq = board->kingSquare_w;
            if(shouldUndoMove)
            {
                toSq = lastMove->startSquare;
                fromSq = lastMove->endSquare;
            }
            else
            {
                fromSq = lastMove->startSquare;
                toSq = lastMove->endSquare;
            }
            pieceOffset = lastMove->piece;
        }
        else
        {
            ksq = FLIP_SQUARE(board->kingSquare_b);
            if(shouldUndoMove)
            {
                toSq = FLIP_SQUARE(lastMove->startSquare);
                fromSq = FLIP_SQUARE(lastMove->endSquare);
            }
            else
            {
                fromSq = FLIP_SQUARE(lastMove->startSquare);
                toSq = FLIP_SQUARE(lastMove->endSquare);
            }
            pieceOffset = FLIP_COLOR(lastMove->piece);
        }
        
        pieceOffset = ISBLACK(pieceOffset) ? ((pieceOffset&0xF) + 4) : ((pieceOffset&0xF) - 1); 
        
        int inputNodeIndex = (640 * i) + (10 * ksq) + pieceOffset;
        uint64_t xorMask = (1ull << fromSq) | (1ull << toSq);

        if(byteAccumulator) byteAccumulator->inputNodes[inputNodeIndex]^=xorMask;
        else floatAccumulator->inputNodes[inputNodeIndex]^=xorMask;

        int fromIdx = (64 * ((10 * ksq) + pieceOffset)) + fromSq;
        int toIdx   = (64 * ((10 * ksq) + pieceOffset)) + toSq;

        int capIdx = -1;
        if(lastMove->capturedPiece)
        {
            int capturedPieceOffset, capturedPieceSquare;
            if(i == 0)
            {
                capturedPieceOffset = lastMove->capturedPiece;
                capturedPieceSquare = lastMove->capturedPieceSquare;
            }
            else
            {
                capturedPieceOffset = FLIP_COLOR(lastMove->capturedPiece);
                capturedPieceSquare = FLIP_SQUARE(lastMove->capturedPieceSquare);
            }

            capturedPieceOffset = ISBLACK(capturedPieceOffset) ? ((capturedPieceOffset&0xF) + 4) : ((capturedPieceOffset&0xF) - 1); 
            int capturedInputNodeIndex = (640 * i) + (10 * ksq) + capturedPieceOffset;

            if(byteAccumulator) byteAccumulator->inputNodes[capturedInputNodeIndex]^=(1ull<<capturedPieceSquare);
            else floatAccumulator->inputNodes[capturedInputNodeIndex]^=(1ull<<capturedPieceSquare);

            capIdx = (64 * ((10 * ksq) + capturedPieceOffset)) + capturedPieceSquare;
        }

        int isCapture = shouldUndoMove ? 1 : -1;

        if(byteAccumulator)
        {
            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            {
                byteAccumulator->accumulator[i][j]+= playerNNUE->weights1[toIdx][j] - playerNNUE->weights1[fromIdx][j];
                if(capIdx != -1) byteAccumulator->accumulator[i][j]+= (isCapture * playerNNUE->weights1[capIdx][j]);
            }
        }
        else
        {
            for(int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++)
            {
                floatAccumulator->accumulator[i][j]+= trainingNNUE->weights1[toIdx][j] - trainingNNUE->weights1[fromIdx][j];
                if(capIdx != -1) floatAccumulator->accumulator[i][j]+= (isCapture * trainingNNUE->weights1[capIdx][j]);
            }
        }
        
    }
}