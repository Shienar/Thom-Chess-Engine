#include "../include/neuralnet.h"
#include "../include/debug.h"
#include "../include/bitboard.h"
#include "../include/moves.h"
#include "../include/engine.h"
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

void loadInputAccumulator(bitboard* board)
{
    if(!board)
    {
        DEBUG("Cannot load null board into accumulator.");
        return;
    }
    else if(!trainingNNUE && !playerNNUE)
    {
        DEBUG("Cannot load null training weights.");
        return;
    }

    uint64_t* inputs = NULL;
    if(trainingNNUE) inputs = trainingNNUE->inputNodes;
    else inputs = playerNNUE->inputNodes;
    memset(inputs, 0, 1280*sizeof(uint64_t));

    int baseIndex_w = 10*board->kingSquare_w;
    int baseIndex_b = 640 + 10*board->kingSquare_b;

/**
 * To find the index given COLOR's KINGSQUARE and 
 * PIECE's PIECE_SQUARE:
 * 
 * index = (40960 * ISBLACK(COLOR)) + (KINGSQUARE * 640) + 64 * PIECE + PIECE_SQUARE
 * 
 * Black pieces are flipped to be viewed from white's perspective.
 */
    int extendedBaseIndex_w = (board->kingSquare_w * 640);
    int extendedBaseIndex_b = (board->kingSquare_b * 640);

    inputs[baseIndex_w + 0] = board->pawn_w;
    inputs[baseIndex_w + 1] = board->knight_w;
    inputs[baseIndex_w + 2] = board->bishop_w;
    inputs[baseIndex_w + 3] = board->rook_w;
    inputs[baseIndex_w + 4] = board->queen_w;
    inputs[baseIndex_w + 5] = __builtin_bswap64(board->pawn_b);
    inputs[baseIndex_w + 6] = __builtin_bswap64(board->knight_b);
    inputs[baseIndex_w + 7] = __builtin_bswap64(board->bishop_b);
    inputs[baseIndex_w + 8] = __builtin_bswap64(board->rook_b);
    inputs[baseIndex_w + 9] = __builtin_bswap64(board->queen_b);

    inputs[baseIndex_b + 0] = __builtin_bswap64(board->pawn_b);
    inputs[baseIndex_b + 1] = __builtin_bswap64(board->knight_b);
    inputs[baseIndex_b + 2] = __builtin_bswap64(board->bishop_b);
    inputs[baseIndex_b + 3] = __builtin_bswap64(board->rook_b);
    inputs[baseIndex_b + 4] = __builtin_bswap64(board->queen_b);
    inputs[baseIndex_b + 5] = board->pawn_w;
    inputs[baseIndex_b + 6] = board->knight_w;
    inputs[baseIndex_b + 7] = board->bishop_w;
    inputs[baseIndex_b + 8] = board->rook_w;
    inputs[baseIndex_b + 9] = board->queen_w;

    if(trainingNNUE)
    {
        float inputArray[1280];
        extractInputLayerToArray(&inputs[baseIndex_w], &inputs[baseIndex_b], inputArray, NULL);

        calculateLayer_Floats(inputArray, trainingNNUE->accumulator[0], 640, ACCUMULATOR_NODES_PER_SIDE, &trainingNNUE->weights1[extendedBaseIndex_w], trainingNNUE->weights1_bias, 0);
        calculateLayer_Floats(&inputArray[640], trainingNNUE->accumulator[1], 640, ACCUMULATOR_NODES_PER_SIDE, &trainingNNUE->weights1[extendedBaseIndex_b], trainingNNUE->weights1_bias, 0);
    }
    else
    {
        int8_t inputArray[1280];
        extractInputLayerToArray(&inputs[baseIndex_w], &inputs[baseIndex_b], NULL, inputArray);

        calculateLayer_IntBytes(inputArray, playerNNUE->accumulator[0], 640, ACCUMULATOR_NODES_PER_SIDE, &playerNNUE->weights1[extendedBaseIndex_w], playerNNUE->weights1_bias, 0);
        calculateLayer_IntBytes(&inputArray[640], playerNNUE->accumulator[1], 640, ACCUMULATOR_NODES_PER_SIDE, &playerNNUE->weights1[extendedBaseIndex_b], playerNNUE->weights1_bias, 0);
    }
}

void updateMoveAccumulator(bitboard* board, move* lastMove, int shouldUndoMove)
{
    if(!lastMove)
    {
        DEBUG("Cannot load null move.");
        return;
    }
    else if(!trainingNNUE && !playerNNUE)
    {
        DEBUG("Cannot load null training weights.");
        return;
    }

    if(ISKING(lastMove->piece))
    {
        loadInputAccumulator(board);
    }
    else
    {
        //Update input nodes.
        uint64_t xorMask = (1ull<<lastMove->startSquare)|(1ull<<lastMove->endSquare);

        int pieceOffset = lastMove->piece;
        int kingSquare = 0;
        if(ISBLACK(pieceOffset)) 
        {
            pieceOffset = (lastMove->piece&0xF) + 4;
            kingSquare = board->kingSquare_b;
        }
        else 
        {
            pieceOffset = (lastMove->piece&0xF) - 1;
            kingSquare = board->kingSquare_w;
        }

        int inputNodeIndex = (640 * ISBLACK(lastMove->piece)) + (10 * kingSquare) + pieceOffset;

        if(trainingNNUE) trainingNNUE->inputNodes[inputNodeIndex]^=xorMask;
        else playerNNUE->inputNodes[inputNodeIndex]^=xorMask;

        //Doesn't care about 40,960 offset for black inputs.
        int fromSquareIndex, toSquareIndex;
        if(shouldUndoMove)
        {
            fromSquareIndex = lastMove->endSquare;
            toSquareIndex = lastMove->startSquare;
        }
        else
        {
            fromSquareIndex = lastMove->startSquare;
            toSquareIndex = lastMove->endSquare;
        }

        if(trainingNNUE)
        {
            float* accumulator;
            if(ISBLACK(lastMove->piece)) accumulator = trainingNNUE->accumulator[1];
            else accumulator = trainingNNUE->accumulator[0];

            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
            {
                accumulator[i] = accumulator[i] + trainingNNUE->weights1[toSquareIndex][i] -  trainingNNUE->weights1[fromSquareIndex][i];
            }
        }
        else
        {
            int8_t* accumulator;
            if(ISBLACK(lastMove->piece)) accumulator = playerNNUE->accumulator[1];
            else accumulator = playerNNUE->accumulator[0];

            for(int i = 0; i < ACCUMULATOR_NODES_PER_SIDE; i++)
            {
                accumulator[i] = accumulator[i] + playerNNUE->weights1[toSquareIndex][i] -  playerNNUE->weights1[fromSquareIndex][i];
            }
        }
    }
}