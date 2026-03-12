#include "../include/engine.h"
#include "../include/moves.h"
#include "../include/bitboard.h"
#include "../include/debug.h"
#include "../include/book.h"
#include "../include/neuralnet.h"
#include <string.h>
#include <float.h>
#include <math.h>

double evaluate(bitboard* board)
{
    if(ISDRAW(board->victor)) 
    {
        //Draws are less desirable in the early/middlegame and more desirable in the lategame.
        int scale;
        if(board->halfMoveCount < MIDDLEGAME_START_HALFMOVES) scale = CONTEMPT_FACTOR_SCALE_EARLYGAME;
        else if(board->halfMoveCount < MIDDLEGAME_END_HALFMOVES) scale = CONTEMPT_FACTOR_SCALE_MIDDLEGAME;
        else scale = CONTEMPT_FACTOR_SCALE_ENDGAME;

        if(board->victor&STALEMATED_WHITE || board->victor&STALEMATED_BLACK) return scale*CONTEMPT_FACTOR_STALEMATE;
        if(board->victor&THREEFOLD) return scale*CONTEMPT_FACTOR_THREEFOLD;
        if(board->victor&FIFTYMOVERULE) return scale*CONTEMPT_FACTOR_FIFTYMOVERULE;
        if(board->victor&INSUFFICIENT_MATERIAL) return scale*CONTEMPT_FACTOR_INSUFFICIENT_MATERIAL;

        DEBUG("Unhandled draw condition.")
        return 0.0;
    }
    else if(board->victor == WHITE) 
    {
        if(board->turn == WHITE) return DBL_MAX;
        else return -DBL_MAX;
    }
    else if(board->victor == BLACK) 
    {
        if(board->turn == BLACK) return DBL_MAX;
        else return -DBL_MAX;
    }

    return (double) forwardPropagate_Int();
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

    if(move_a->capturedPiece && move_b->capturedPiece) return ((move_b->capturedPiece&0xF) - (move_b->piece&0xF)) - ((move_a->capturedPiece&0xF) - (move_a->piece&0xF));
    else if(move_a->capturedPiece) return -1;
    else if(move_b->capturedPiece) return 1;
    else if(ISKING(move_a->piece)) return 1; 
    else if(ISKING(move_b->piece)) return -1;
    else return move_b->piece - move_a->piece;
    
}

double quiesce(bitboard* board, double alpha, double beta, int depth)
{
    if(board->victor == WHITE) 
    {
        if(board->turn == WHITE) return INT64_MAX;
        else return INT64_MIN;
    }
    if(board->victor == BLACK) 
    {
        if(board->turn == BLACK) return INT64_MAX;
        else return INT64_MIN;
    }
    if(ISDRAW(board->victor)) 
    {
        //Draws are less desirable in the early/middlegame and more desirable in the lategame.
        int scale;
        if(board->halfMoveCount < MIDDLEGAME_START_HALFMOVES) scale = CONTEMPT_FACTOR_SCALE_EARLYGAME;
        else if(board->halfMoveCount < MIDDLEGAME_END_HALFMOVES) scale = CONTEMPT_FACTOR_SCALE_MIDDLEGAME;
        else scale = CONTEMPT_FACTOR_SCALE_ENDGAME;

        if(board->victor&STALEMATED_WHITE || board->victor&STALEMATED_BLACK) return scale*CONTEMPT_FACTOR_STALEMATE;
        if(board->victor&THREEFOLD) return scale*CONTEMPT_FACTOR_THREEFOLD;
        if(board->victor&FIFTYMOVERULE) return scale*CONTEMPT_FACTOR_FIFTYMOVERULE;
        if(board->victor&INSUFFICIENT_MATERIAL) return scale*CONTEMPT_FACTOR_INSUFFICIENT_MATERIAL;
    }

    double best;
    table_entry_tt* entry = transposition_table_get(board, transpositionTable);
    if(entry &&
        (entry->nodeType == NODE_TYPE_PV ||
        (entry->nodeType == NODE_TYPE_ALL && entry->evaluation <= alpha) ||
        (entry->nodeType == NODE_TYPE_CUT && entry->evaluation >= beta))) 
    {
        best = entry->evaluation;
    }
    else best = evaluate(board);

    if(depth == 0 || best >= beta) return best;
    if(best > alpha) alpha = best;

    move** captureMoves = generateMoveList(board, 1);
    if(captureMoves != NULL)
    {
        //Sorting
        int index = 0;
        while(captureMoves[index]) index++;
        qsort_s(captureMoves, index, sizeof(move*), sortMoves, NULL);

        index = 0;
        while(captureMoves[index])
        {
            move* currentMove = captureMoves[index];
            if(!moveFromStruct(board, currentMove))
            {
                updateMoveAccumulator(board, board->moveStackTop, PLAYER_NNUE, 0);

                double score = -quiesce(board, -beta, -alpha, depth - 1);

                move* poppedMove = unmove(board);
                updateMoveAccumulator(board, poppedMove, PLAYER_NNUE, 1);

                if(score >= beta)
                {
                    freeMoveList(captureMoves);
                    return score;
                }
                if(score > best) best = score;
                if(score > alpha) alpha = score;
            }
            index++;
        }
        freeMoveList(captureMoves);
    }
    return best;
}

void copyNMoves(move* dest, move* source, int count)  {while(count--) *dest++ = *source++;}

double principalVariationSearch(bitboard* board, double alpha, double beta, int maxDepth, int depth, move* pv, int pvIndex, clock_t timeLimit)
{
    //Transposition table
    table_entry_tt* old_tt_entry = NULL;
    if((old_tt_entry = transposition_table_get(board, transpositionTable)) != NULL && 
        old_tt_entry->evaluationDepth >= depth &&
            (old_tt_entry->nodeType == NODE_TYPE_PV ||
            (old_tt_entry->nodeType == NODE_TYPE_ALL && old_tt_entry->evaluation <= alpha) ||
            (old_tt_entry->nodeType == NODE_TYPE_CUT && old_tt_entry->evaluation >= beta)))
    {
        return old_tt_entry->evaluation;
    }

    table_entry_tt new_tt_entry = {
        .age = clock(),
        .evaluationDepth = depth,
        .hashCode = getHashCode(board)
    };

    if(depth == 0 || clock() > timeLimit || board->victor) 
    {
        new_tt_entry.nodeType = NODE_TYPE_PV; // Exact evaluation.
        new_tt_entry.evaluation = quiesce(board, alpha, beta, 5);
        transposition_table_set(transpositionTable, new_tt_entry);
        return new_tt_entry.evaluation;
    }
    
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
                updateMoveAccumulator(board, board->moveStackTop, PLAYER_NNUE, 0);
                if(index == 0)
                {
                    score = -principalVariationSearch(board, -beta, -alpha, maxDepth, depth - 1, pv, pvIndex + depth, timeLimit);
                }
                else
                {
                    score = -principalVariationSearch(board, -alpha - 1, -alpha, maxDepth, depth - 1, pv, pvIndex + depth, timeLimit);
                    //Re-search PV node
                    if (score > alpha && score < beta) score = -principalVariationSearch(board, -beta, -alpha, maxDepth, depth - 1, pv, pvIndex + depth, timeLimit);
                }
                
                move* poppedMove = unmove(board);
                updateMoveAccumulator(board, poppedMove, PLAYER_NNUE, 1);
                
                if(score >= beta)
                {
                    new_tt_entry.nodeType = NODE_TYPE_CUT; // Lowerbound evaluation.
                    new_tt_entry.evaluation = score;
                    transposition_table_set(transpositionTable, new_tt_entry);
                    freeMoveList(moveList);
                    return beta;
                }
                else if(score > alpha) 
                {
                    new_tt_entry.nodeType = NODE_TYPE_ALL; // Upperbound evaluation.
                    new_tt_entry.evaluation = score;
                    transposition_table_set(transpositionTable, new_tt_entry);
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

move* calculateBestMove(bitboard* board, int maxDepth, int maxTimeSeconds)
{
    //Book moves
    if(entries) 
    {
        move* bookMove = NULL;
        if((bookMove = getBookMove(board))) return bookMove;
        else unloadBook();
    }

    move* principalVariation = CALLOC(maxDepth, sizeof(move));
    move* tempPVTable = NULL;

    clock_t endTime = clock() + CLOCKS_PER_SEC*maxTimeSeconds;

    for(int currentDepth = 1; currentDepth <= maxDepth; currentDepth++)
    {
        if(clock() > endTime) break;

        tempPVTable = CALLOC(0.5*currentDepth*(currentDepth + 1), sizeof(move));
        copyNMoves(tempPVTable, principalVariation, currentDepth);

        //Always fully evaluate at depth 1.
        if(currentDepth == 1) principalVariationSearch(board, -DBL_MAX, DBL_MAX, currentDepth, currentDepth, tempPVTable, 0, LONG_MAX);
        else principalVariationSearch(board, -DBL_MAX, DBL_MAX, currentDepth, currentDepth, tempPVTable, 0, endTime);
        
        copyNMoves(principalVariation, tempPVTable, currentDepth);
        FREE(tempPVTable);
        tempPVTable = NULL;
    }

    if(tempPVTable) FREE(tempPVTable);

    move* bestMove = CALLOC(1, sizeof(move));
    memcpy(bestMove, &principalVariation[0], sizeof(move));

    if(bestMove->startSquare == bestMove->endSquare && bestMove->startSquare == 0)
    {
        DEBUG("Engine returned empty move.");
        if(printDebugMessages)
        {
            for(int i = 0; i < maxDepth; i++) printf("\tpv[%d] = %d->%d\n", i, principalVariation[i].startSquare, principalVariation[i].endSquare);
        }
    }

    FREE(principalVariation);
    principalVariation = NULL;
    return bestMove;
}