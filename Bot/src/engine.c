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
        if(i > 7 && i < 56) w->pawnPieceWeights[i] = 1;
        else w->pawnPieceWeights[i] = 0;
        w->knightPieceWeights[i] = 3;
        w->bishopPieceWeights[i] = 3;
        w->rookPieceWeights[i] = 5;
        w->queenPieceWeights[i] = 9;
        w->kingPieceWeights[i] = 25;
    }
    

    w->pawnWeight = 1;
    w->knightWeight = 3;
    w->bishopWeight = 3;
    w->rookWeight = 5;
    w->queenWeight = 9;
    w->kingWeight = 25;
}

double evaluate(bitboard* board, engine* w)
{
    if(board->victor == WHITE) 
    {
        if(board->turn == WHITE) return DBL_MAX;
        else return -DBL_MAX;
    }
    if(board->victor == BLACK) 
    {
        if(board->turn == BLACK) return DBL_MAX;
        else return -DBL_MAX;
    }
    if(board->victor == (WHITE|BLACK)) return CONTEMPT_FACTOR;

    double score_w, score_b = 0;

    for(int i = 0; i < 64; i++)
    {
        uint64_t mask = 1ull << i;

        if(board->pawn_b&mask) score_b+=(w->pawnWeight*w->pawnPieceWeights[i]);
        else if(board->knight_b&mask) score_b+=(w->knightWeight*w->knightPieceWeights[i]);
        else if(board->bishop_b&mask) score_b+=(w->bishopWeight*w->bishopPieceWeights[i]);
        else if(board->rook_b&mask) score_b+=(w->rookWeight*w->rookPieceWeights[i]);
        else if(board->queen_b&mask) score_b+=(w->queenWeight*w->queenPieceWeights[i]);
        else if(board->king_b&mask) score_b+=(w->kingWeight*w->kingPieceWeights[i]);

        else if(board->pawn_w&mask) score_w+=(w->pawnWeight*w->pawnPieceWeights[i]);
        else if(board->knight_w&mask) score_w+=(w->knightWeight*w->knightPieceWeights[i]);
        else if(board->bishop_w&mask) score_w+=(w->bishopWeight*w->bishopPieceWeights[i]);
        else if(board->rook_w&mask) score_w+=(w->rookWeight*w->rookPieceWeights[i]);
        else if(board->queen_w&mask) score_w+=(w->queenWeight*w->queenPieceWeights[i]);
        else if(board->king_w&mask) score_w+=(w->kingWeight*w->kingPieceWeights[i]);

    }

    //Evaluate in response to whoever's turn it is.
    if(ISWHITE(board->turn)) return score_w - score_b;
    else return score_b - score_w;
}

double quiesce(bitboard* board, engine* engine, double alpha, double beta, int depth)
{
    
    if(board->victor == WHITE) 
    {
        if(board->turn == WHITE) return DBL_MAX;
        else return -DBL_MAX;
    }
    if(board->victor == BLACK) 
    {
        if(board->turn == BLACK) return DBL_MAX;
        else return -DBL_MAX;
    }
    if(board->victor == (WHITE|BLACK)) return CONTEMPT_FACTOR;

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
    if(depth == 0 || clock() > timeLimit || board->victor) return quiesce(board, engine, alpha, beta, 3);
    
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
                    if (score > alpha && score < beta) score = -principalVariationSearch(board, engine, -beta, -alpha, maxDepth, depth - 1, pv, pvIndex + depth, timeLimit);
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
                    pv[pvIndex] = *moveList[index];
                    copyNMoves(&pv[pvIndex + 1], &pv[pvIndex + depth], depth - 1);
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
    engine->pv = CALLOC(maxDepth, sizeof(move));
    move* tempPVTable = NULL;

    clock_t endTime = clock() + CLOCKS_PER_SEC*maxTimeSeconds;

    for(int currentDepth = 1; currentDepth <= maxDepth; currentDepth++)
    {
        if(clock() > endTime) break;

        tempPVTable = CALLOC(0.5*currentDepth*(currentDepth + 1), sizeof(move));
        copyNMoves(tempPVTable, engine->pv, currentDepth);

        //DBL_MIN is the smallest POSITIVE double. -DBL_MAX must be used instead.
        //I'm angry at how long this slipped past me.
        principalVariationSearch(board, engine, -DBL_MAX, DBL_MAX, currentDepth, currentDepth, tempPVTable, 0, endTime);

        copyNMoves(engine->pv, tempPVTable, currentDepth);
        FREE(tempPVTable);
        tempPVTable = NULL;
    }

    if(tempPVTable) FREE(tempPVTable);
    destroy_hashTable(engine->transpositionTable);
    engine->transpositionTable = NULL;

    move* bestMove = CALLOC(1, sizeof(move));
    memcpy(bestMove, &engine->pv[0], sizeof(move));

    if(bestMove->startSquare == bestMove->endSquare && bestMove->startSquare == 0)
    {
        DEBUG("Engine returned empty move.");
        if(printDebugMessages)
        {
            for(int i = 0; i < maxDepth; i++) printf("\tpv[%d] = %d->%d\n", i, engine->pv[i].startSquare, engine->pv[i].endSquare);
        }
    }

    FREE(engine->pv);
    engine->pv = NULL;
    return bestMove;
}