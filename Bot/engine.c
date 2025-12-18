#include "engine.h"
#include "moves.h"
#include "bitboard.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

void initPieceWeights(weights* w, int useExisting)
{
    for(int i = 0; i < 64; i++)
    {
        w->pawnPieceWeights[i] = i;
        w->rookPieceWeights[i] = 1 ;
        w->knightPieceWeights[i] = 1;
        w->bishopPieceWeights[i] = 1;
        w->queenPieceWeights[i] = 1;
        w->kingPieceWeights[i] = 1;
    }
    

    w->pawnWeight = 1;
    w->rookWeight = 5;
    w->knightWeight = 3;
    w->bishopWeight = 3;
    w->queenWeight = 9;
    w->kingWeight = 25;
}


double evaluate(bitboard* board, weights* w)
{
    double score_w, score_b = 0;

    for(int i = 0; i < 64; i++)
    {
        uint64_t mask = 1ull << i;

        if(board->pawn_b&mask) score_b+=(w->pawnWeight*w->pawnPieceWeights[i]);
        if(board->knight_b&mask) score_b+=(w->knightWeight*w->knightPieceWeights[i]);
        if(board->bishop_b&mask) score_b+=(w->bishopWeight*w->bishopPieceWeights[i]);
        if(board->rook_b&mask) score_b+=(w->rookWeight*w->rookPieceWeights[i]);
        if(board->queen_b&mask) score_b+=(w->queenWeight*w->queenPieceWeights[i]);
        if(board->king_b&mask) score_b+=(w->kingWeight*w->kingPieceWeights[i]);

        if(board->pawn_w&mask) score_w+=(w->pawnWeight*w->pawnPieceWeights[i]);
        if(board->knight_w&mask) score_w+=(w->knightWeight*w->knightPieceWeights[i]);
        if(board->bishop_w&mask) score_w+=(w->bishopWeight*w->bishopPieceWeights[i]);
        if(board->rook_w&mask) score_w+=(w->rookWeight*w->rookPieceWeights[i]);
        if(board->queen_w&mask) score_w+=(w->queenWeight*w->queenPieceWeights[i]);
        if(board->king_w&mask) score_w+=(w->kingWeight*w->kingPieceWeights[i]);

    }

    return score_w - score_b;
}

double alpha_beta(bitboard board, weights* weights, double alpha, double beta, int depth)
{
    //Move lists ends with a terminating move with startSquare == -1 and endSquare == moveCount.
    move* childMoves = generateMoveList(&board);
    if(!childMoves || depth <= 0)
    {
        free(childMoves);
        return evaluate(&board, weights);
    }
    else if(board.turn == WHITE)
    {
        printf("BOT PLAYED WHITE AT DEPTH=%d\n", depth);
        int index = 0;
        while(childMoves[index].startSquare != -1)
        {
            bitboard childBoard;
            memcpy(&childBoard, &board, sizeof(bitboard));
            moveFromStruct(&childBoard, childMoves[index]);
            double currentEvaluation = alpha_beta(childBoard, weights, alpha, beta, depth - 1);
            alpha = fmax(alpha, currentEvaluation);
            if(alpha >= beta) 
            {
                free(childMoves);
                return beta;
            }
        }
        free(childMoves);
        return alpha;
    }
    else
    {
        printf("BOT PLAYED BLACK AT DEPTH=%d\n", depth);
        int index = 0;
        while(childMoves[index].startSquare != -1)
        {
            bitboard childBoard;
            memcpy(&childBoard, &board, sizeof(bitboard));
            moveFromStruct(&childBoard, childMoves[index]);
            double currentEvaluation = alpha_beta(childBoard, weights, alpha, beta, depth - 1);
            beta = fmin(beta, currentEvaluation);
            if(alpha >= beta) 
            {
                free(childMoves);
                return alpha;
            }
        }
        free(childMoves);
        return beta;
    }
}

move calculateBestMove(bitboard* board, weights* weights, int depth)
{
    move* childMoves = generateMoveList(board);

    int moveCount = 0;
    while(childMoves[moveCount].startSquare != -1) moveCount++;

    bitboard childBoard;
    double* childScores = calloc(moveCount, sizeof(double));
    for(int i = 0; i < moveCount; i++)
    {
        printf("Start of child %d evaluation:\n", i);
        memcpy(&childBoard, board, sizeof(bitboard));
        movePiece(&childBoard, childMoves[i].startSquare, childMoves[i].endSquare, childMoves[i].piece, childMoves[i].promoteTo);
        childScores[i] = alpha_beta(childBoard, weights, DBL_MIN, DBL_MAX, depth);
        printf("End of child %d evaluation:\n", i);
    }

    double maxScore = childScores[0];
    move bestMove = childMoves[0];
    for(int i = 1; i < moveCount; i++)
    {
        if(childScores[i] > maxScore)
        {
            maxScore = childScores[i];
            bestMove = childMoves[i];
        }
    }

    free(childScores);
    free(childMoves);

    return bestMove;
}