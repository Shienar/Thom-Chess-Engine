#include "../include/neuralnet.h"
#include "../include/debug.h"
#include "../include/bitboard.h"
#include "../include/moves.h"
#include "../include/engine.h"
#include <math.h>
#include <float.h>

void extractInputLayerToArray(uint64_t* inputLayerCompact, float* inputLayerFloats, int8_t* inputLayerBytes)
{
    if(!inputLayerCompact)
    {
        DEBUG("Cannot extract null input layer.");
        return;
    }
    else if(inputLayerFloats)
    {
        for(int i = 0; i < INPUT_BITS; i++)
        {
            if((inputLayerCompact[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerFloats[i] = 1.0;
            else inputLayerFloats[i] = 0.0;
        }
    }
    else if(inputLayerBytes)
    {
        for(int i = 0; i < INPUT_BITS; i++)
        {
            if((inputLayerCompact[ (int) i / 64 ] >> ( i % 64 )) & 1) inputLayerBytes[i] = 1;
            else inputLayerBytes[i] = 0;
        }
    }
    else
    {
        DEBUG("No provided destination for extraction.");
    }
}

void loadInputAccumulator(bitboard* board, int networkType)
{
    if(!board)
    {
        DEBUG("Cannot load null board into accumulator.");
        return;
    }
    else if(networkType == TRAINING_NNUE && !trainingNNUE)
    {
        DEBUG("Cannot load null training weights.");
        return;
    }
    else if(networkType == PLAYER_NNUE && !playerNNUE)
    {
        DEBUG("Cannot load null playing weights.");
        return;
    }

    
    uint64_t* inputs = NULL;
    if(networkType == TRAINING_NNUE) inputs = trainingNNUE->inputNodes;
    else inputs = playerNNUE->inputNodes;
    memset(inputs, 0, 1280*sizeof(uint64_t));

    int baseIndex_w = 10*board->kingSquare_w;
    int baseIndex_b = 640 + 10*board->kingSquare_b;

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

    inputs[baseIndex_b + 0] = board->pawn_b;
    inputs[baseIndex_b + 1] = board->knight_b;
    inputs[baseIndex_b + 2] = board->bishop_b;
    inputs[baseIndex_b + 3] = board->rook_b;
    inputs[baseIndex_b + 4] = board->queen_b;
    inputs[baseIndex_b + 5] = board->pawn_w;
    inputs[baseIndex_b + 6] = board->knight_w;
    inputs[baseIndex_b + 7] = board->bishop_w;
    inputs[baseIndex_b + 8] = board->rook_w;
    inputs[baseIndex_b + 9] = board->queen_w;

    if(networkType == TRAINING_NNUE)
    {
        float inputArray[81920];
        extractInputLayerToArray(inputs, inputArray, NULL);

        calculateLayer_Floats(inputArray, trainingNNUE->accumulator[0], HALF_INPUT_BITS, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights1, trainingNNUE->weights1_bias, 0);
        calculateLayer_Floats(&inputArray[40960], trainingNNUE->accumulator[1], HALF_INPUT_BITS, ACCUMULATOR_NODES_PER_SIDE, trainingNNUE->weights1, trainingNNUE->weights1_bias, 0);
    }
    else
    {
        int8_t inputArray[81920];
        extractInputLayerToArray(inputs, NULL, inputArray);

        calculateLayer_IntBytes(inputArray, playerNNUE->accumulator[0], HALF_INPUT_BITS, ACCUMULATOR_NODES_PER_SIDE, playerNNUE->weights1, playerNNUE->weights1_bias, 0);
        calculateLayer_IntBytes(&inputArray[40960], playerNNUE->accumulator[1], HALF_INPUT_BITS, ACCUMULATOR_NODES_PER_SIDE, playerNNUE->weights1, playerNNUE->weights1_bias, 0);
    }
}

void updateMoveAccumulator(bitboard* board, move* lastMove, int networkType, int shouldUndoMove)
{
    if(!lastMove)
    {
        DEBUG("Cannot load null move.");
        return;
    }

    if(ISKING(lastMove->piece))
    {
        loadInputAccumulator(board, networkType);
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

        if(networkType == TRAINING_NNUE) trainingNNUE->inputNodes[inputNodeIndex]^=xorMask;
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

        if(networkType == TRAINING_NNUE)
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