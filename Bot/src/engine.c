#include "../include/engine.h"
#include "../include/moves.h"
#include "../include/bitboard.h"
#include "../include/debug.h"
#include "../include/evolve.h"
#include <string.h>
#include <float.h>
#include <math.h>

void initEnginePieceWeights(engine* w, int useExisting)
{
    if(useExisting && !loadEngineWeights(w)) return;
    
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

double evaluate(bitboard* board, engine* w)
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

double quiesce(bitboard* board, engine* engine, double alpha, double beta, int depth)
{
    double best = transposition_table_evaluate(board, engine);

    if(depth == 0 || best >= beta) return best;
    if(best > alpha) alpha = best;

    move** captureMoves = generateMoveList(board, 1);
    if(captureMoves != NULL)
    {
        int index = 0;
        while(captureMoves[index])
        {
            move* currentMove = captureMoves[index];
            if(!moveFromStruct(board, currentMove))
            {
                double score = -quiesce(board, engine, alpha, beta, depth - 1);
                unmove(board);
                if(score >= beta)
                {
                    freeMoveList(captureMoves);
                    return score;
                }
                else if(score > best) best = score;
                else if(score > alpha) alpha = score;
            }
            index++;
        }
        freeMoveList(captureMoves);
    }
    return best;
}

void copyNMoves(move* dest, move* source, int count)  {while(count--) *dest++ = *source++;}

double principalVariationSearch(bitboard* board, engine* engine, double alpha, double beta, int depth, int pvIndex, clock_t timeLimit)
{
    if(depth == 0 || clock() > timeLimit) return quiesce(board, engine, alpha, beta, 3);

    move* principalMove = &engine->pvTable[pvIndex];
    double score = 0;

    //Is there an initialized principalmove?
    if(principalMove->startSquare != 0 || principalMove->endSquare != 0)
    {
        //Check the principal move.
        if(!moveFromStruct(board, principalMove))
        {
            score = -principalVariationSearch(board, engine, -alpha, -beta, depth - 1, pvIndex + depth, timeLimit);
            unmove(board);

            if(score >= beta) return beta;
            else if(score > alpha) 
            {
                alpha = score;
                copyNMoves(&engine->pvTable[pvIndex + 1], &engine->pvTable[pvIndex + depth], depth - 1);
            }
        }
    }

    move** moveList = generateMoveList(board, 0);
    if(moveList)
    {
        int index = 0;
        while(moveList[index])
        {
            if(!moveFromStruct(board, moveList[index]))
            {
                score = -principalVariationSearch(board, engine, -alpha, -beta, depth - 1, pvIndex + depth, timeLimit);
                //Re-search PV node
                if (score > alpha && beta - alpha > 0) score = -principalVariationSearch(board, engine, -alpha, -beta, depth - 1, pvIndex + depth, timeLimit);
                
                unmove(board);

                if(score >= beta)
                {
                    freeMoveList(moveList);
                    return beta;
                }
                else if(score > alpha) 
                {
                    alpha = score;
                    engine->pvTable[pvIndex] = *moveList[index];
                    copyNMoves(&engine->pvTable[pvIndex + 1], &engine->pvTable[pvIndex + depth], depth - 1);
                }
            }
            index++;
        }
        freeMoveList(moveList);
    }
    return alpha;
}

move* calculateBestMove(bitboard* board, engine* engine, int maxDepth, int maxTimeSeconds)
{
    destroy_hashTable(engine->transpositionTable);
    engine->transpositionTable = create_hashTable();
    engine->pvTable = CALLOC(0.5*maxDepth*(maxDepth + 1), sizeof(move));

    clock_t endTime = clock() + CLOCKS_PER_SEC*maxTimeSeconds;

    for(int currentDepth = 1; currentDepth <= maxDepth; currentDepth++)
    {
        if(clock() > endTime) break;

        principalVariationSearch(board, engine, DBL_MIN, DBL_MAX, currentDepth, 0, endTime);
    }

    destroy_hashTable(engine->transpositionTable);
    engine->transpositionTable = NULL;

    move* bestMove = CALLOC(1, sizeof(move));
    memcpy(bestMove, &engine->pvTable[0], sizeof(move));
    FREE(engine->pvTable);
    engine->pvTable = NULL;
    return bestMove;
}