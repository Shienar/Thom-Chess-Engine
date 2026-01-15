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
    if(board->victor == WHITE) return DBL_MAX;
    if(board->victor == BLACK) return DBL_MIN;
    if(board->victor == (WHITE|BLACK)) return 0;

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

//For qsort_s
int sortMoves(void* c, const void* a, const void* b)
{
    move* move_a = *(move**)a;
    move* move_b = *(move**)b;

    if(c)
    {
        move* best_move = (move*)c;
        if(move_a->startSquare == best_move->startSquare && move_a->endSquare == best_move->endSquare) return -1;
        else if(move_b->startSquare == best_move->startSquare && move_b->endSquare == best_move->endSquare) return 1;
    }
    
    return 0;
}

void copyNMoves(move* dest, move* source, int count)  {while(count--) *dest++ = *source++;}

double principalVariationSearch(bitboard* board, engine* engine, double alpha, double beta, int maxDepth, int depth, move* pv, int pvIndex, clock_t timeLimit)
{
    if(board->victor == WHITE) return DBL_MAX;
    if(board->victor == BLACK) return DBL_MIN;
    if(board->victor == (WHITE|BLACK)) return 0;

    if(depth == 0 || clock() > timeLimit) return quiesce(board, engine, alpha, beta, 3);
    
    memset(&engine->pvTable[pvIndex], 0, sizeof(move));
    
    double score = 0;

    move** moveList = generateMoveList(board, 0);
    if(moveList)
    {
        int index = 0;
        while(moveList[index]) index++;

        //Sort movelist with qsort_s. Pass the expected best move at this depth as the context.
        move* context = NULL;
        if(pv) context = &pv[maxDepth - depth];

        qsort_s(moveList, index, sizeof(move*), sortMoves, context);
        
        index = 0;
        while(moveList[index])
        {
            if(!moveFromStruct(board, moveList[index]))
            {
                if(index == 0)
                {
                    score = -principalVariationSearch(board, engine, -beta, -alpha, maxDepth, depth - 1, pv, pvIndex + depth, timeLimit);
                }
                else
                {
                    score = -principalVariationSearch(board, engine, -alpha - 1, -alpha, maxDepth, depth - 1, pv, pvIndex + depth, timeLimit);
                    //Re-search PV node
                    if (score > alpha && beta - alpha > 0) score = -principalVariationSearch(board, engine, -beta, -alpha, maxDepth, depth - 1, pv, pvIndex + depth, timeLimit);
                }
                
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
    move* tempPVTable = NULL;

    clock_t endTime = clock() + CLOCKS_PER_SEC*maxTimeSeconds;

    for(int currentDepth = 1; currentDepth <= maxDepth; currentDepth++)
    {
        if(clock() > endTime) break;

        principalVariationSearch(board, engine, DBL_MIN, DBL_MAX, currentDepth, currentDepth, tempPVTable, 0, endTime);

        if(tempPVTable)
        {
            FREE(tempPVTable);
            tempPVTable = NULL;
        }
        tempPVTable = CALLOC(0.5*currentDepth*(currentDepth + 1), sizeof(move));
        copyNMoves(tempPVTable, engine->pvTable, currentDepth);
    }

    if(tempPVTable) FREE(tempPVTable);
    destroy_hashTable(engine->transpositionTable);
    engine->transpositionTable = NULL;

    move* bestMove = CALLOC(1, sizeof(move));
    memcpy(bestMove, &engine->pvTable[0], sizeof(move));
    FREE(engine->pvTable);
    engine->pvTable = NULL;
    return bestMove;
}