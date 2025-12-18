#include "engine.h"
#include "moves.h"
#include "bitboard.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <windows.h>

void initPieceWeights(weights* w, int useExisting)
{
    for(int i = 0; i < 64; i++)
    {
        w->pawnPieceWeights[i] = rand()%3;
        w->rookPieceWeights[i] = rand()%5;
        w->knightPieceWeights[i] = rand()%7;
        w->bishopPieceWeights[i] = rand()%11;
        w->queenPieceWeights[i] = rand()%13;
        w->kingPieceWeights[i] = rand()%17;
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
    if(board->victor == WHITE) return DBL_MAX;
    if(board->victor == BLACK) return DBL_MIN;
    if(board->victor == (WHITE|BLACK)) return 0;

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

double alpha_beta(bitboard* board, weights* weights, double alpha, double beta, int depth)
{
    if(depth <= 0) return evaluate(board, weights);

    //Move lists ends with a terminating move with startSquare == -1 and endSquare == moveCount.
    move** childMoves = generateMoveList(board);
    if(!childMoves) return evaluate(board, weights);

    int moveCount = 0;
    while(childMoves[moveCount]->startSquare != -1) moveCount++;
    if(board->turn == WHITE)
    {
        for(int i = 0; i < moveCount; i++)
        {
            if(!moveFromStruct(board, childMoves[i]))
            {
                double currentEvaluation = alpha_beta(board, weights, alpha, beta, depth - 1);
                unmove(board);
                alpha = fmax(alpha, currentEvaluation);
                if(alpha >= beta) 
                {
                    for(int j = 0; j < moveCount; j++) free(childMoves[j]);
                    free(childMoves);
                    return beta;
                }
            }
        }
        for(int j = 0; j < moveCount; j++) free(childMoves[j]);
        free(childMoves);
        return alpha;
    }
    else
    {
        for(int i = 0; i < moveCount; i++)
        {
            if(!moveFromStruct(board, childMoves[i]))
            {
                double currentEvaluation = alpha_beta(board, weights, alpha, beta, depth - 1);
                unmove(board);
                beta = fmin(beta, currentEvaluation);
                if(alpha >= beta) 
                {
                    for(int j = 0; j < moveCount; j++) free(childMoves[j]);
                    free(childMoves);
                    return alpha;
                }
            }
        }
        for(int j = 0; j < moveCount; j++) free(childMoves[j]);
        free(childMoves);
        return beta;
    }
}

void CALLBACK alpha_beta_thread_func(PTP_CALLBACK_INSTANCE Instance, PVOID voidParam, PTP_WORK Work)
{
    printf("Thread start\n");
    threadParam* params = (threadParam*) voidParam;
    *(params->returnValue) = alpha_beta(params->board, params->weights, DBL_MIN, DBL_MAX, params->depth -1);
    ReleaseSemaphore(params->parentWaitSemaphore, 1, NULL);
    ReleaseSemaphore(params->threadCountSemaphore, 1, NULL);
    printf("Thread end\n");
}

move* calculateBestMove(bitboard* board, weights* weights, int depth, int maxThreads)
{
    move** childMoves = generateMoveList(board);
    if(!childMoves) return NULL;

    int moveCount = 0;
    while(childMoves[moveCount]->startSquare != -1) moveCount++;

    bitboard* childBoards = calloc(moveCount, sizeof(bitboard));
    double* childScores = calloc(moveCount, sizeof(double));
    threadParam* threadParamArray = calloc(moveCount, sizeof(threadParam));

    //Forces parent to wait for all children
    HANDLE sharedSemaphore = CreateSemaphoreW(NULL, 0, moveCount, NULL);

    //Limits the maximum number of searching threads.
    HANDLE threadCountSemaphore = CreateSemaphoreW(NULL, maxThreads, moveCount, NULL);

    for(int i = 0; i < moveCount; i++)
    {
        memcpy(&childBoards[i], board, sizeof(bitboard));
        moveFromStruct(&childBoards[i], childMoves[i]);

        threadParamArray[i].parentWaitSemaphore = sharedSemaphore;
        threadParamArray[i].threadCountSemaphore = threadCountSemaphore;
        threadParamArray[i].board = &childBoards[i];
        threadParamArray[i].weights = weights;
        threadParamArray[i].depth = depth;
        threadParamArray[i].returnValue = &childScores[i];

        WaitForSingleObject(threadCountSemaphore, INFINITE);

        PTP_WORK newWork = CreateThreadpoolWork(alpha_beta_thread_func, &threadParamArray[i], NULL);
        if(newWork == NULL)
        {
            DEBUG("Failed to create threadpool work")
            continue;
        }
        SubmitThreadpoolWork(newWork);
    }

    //I wish I could just initialize the semaphore to a negative value.
    for(int i = 0; i < moveCount; i++) WaitForSingleObject(sharedSemaphore, INFINITE);
    CloseHandle(sharedSemaphore);
    CloseHandle(threadCountSemaphore);
    free(childBoards);
    free(threadParamArray);

    double maxScore = childScores[0];
    move* bestMove = childMoves[0];
    for(int i = 1; i < moveCount; i++)
    {
        if(childScores[i] > maxScore)
        {
            maxScore = childScores[i];
            free(bestMove);
            bestMove = childMoves[i];
        }
        else
        {
            free(childMoves[i]);
        }
    }

    move* pBestMove = calloc(1, sizeof(move));
    memcpy(pBestMove, bestMove, sizeof(move));

    free(bestMove);
    free(childScores);
    free(childMoves);

    return pBestMove;
}