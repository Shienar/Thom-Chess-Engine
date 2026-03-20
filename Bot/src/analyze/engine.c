#include "../../include/analyze/engine.h"
#include "../../include/board/moves.h"
#include "../../include/board/bitboard.h"
#include "../../include/debug.h"
#include "../../include/analyze/book.h"
#include "../../include/analyze/neuralnet.h"
#include "../../include/pyrrhic/tbprobe.h"
#include <string.h>
#include <float.h>
#include <math.h>

int useHelperThreads = 1;

#ifdef COUNT_NODES_VISITED
int nodesEvaluated = 0;
int nodesVisited = 0;
#endif

double evaluate(bitboard* board)
{
    #ifdef COUNT_NODES_VISITED 
    nodesEvaluated++;
    #endif

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

        DEBUG("Unhandled draw condition.");
        return 0.0;
    }
    else if(board->victor == WHITE) 
    {
        if(board->turn == WHITE) return INT8_MAX + 2;
        else return INT8_MIN - 1;
    }
    else if(board->victor == BLACK) 
    {
        if(board->turn == BLACK) return INT8_MAX + 2;
        else return -INT8_MIN - 1;
    }

    if(trainingNNUE) return (double) forwardPropagate_Float(board->turn);
    else return (double) forwardPropagate_Int(board->turn);
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
    //else return move_b->piece - move_a->piece;
    else return rand()%2?-1:1; // A bit of randomness in the sorting to help out on multithreaded diversity.
    
}

double quiesce(bitboard* board, double alpha, double beta, int depth)
{
    #ifdef COUNT_NODES_VISITED 
    nodesVisited++;
    #endif

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
                updateMoveAccumulator(board, board->moveStackTop, 0);

                double score = -quiesce(board, -beta, -alpha, depth - 1);

                move* poppedMove = unmove(board);
                updateMoveAccumulator(board, poppedMove, 1);

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

double principalVariationSearch(bitboard* board, double alpha, double beta, int maxDepth, int depth, move* pv, int pvIndex, clock_t* timeLimit)
{
    #ifdef COUNT_NODES_VISITED 
    nodesVisited++;
    #endif

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

    //Sygyzy table probing.
    if(board->moveStackTop && board->moveStackTop->capturedPiece && __builtin_popcountll(board->pieces_all) <= 5 && !(board->flags&0x30))
    {
        uint32_t ep = board->enPassantSquare;
        if(ep == -1) ep = 0;

        bool turn = PYRRHIC_WHITE;
        if(ISBLACK(board->turn)) turn = PYRRHIC_BLACK;

        int result = tb_probe_wdl(board->pieces_w, board->pieces_b, 
                                        board->king_b|board->king_w, board->queen_b|board->queen_w, 
                                        board->rook_b|board->rook_w, board->bishop_b|board->bishop_w,
                                        board->knight_b|board->knight_w, board->pawn_b|board->pawn_w,
                                        ep, turn);
        
        if(result == TB_LOSS) return -DBL_MAX;
        else if(result == TB_BLESSED_LOSS || TB_DRAW || TB_CURSED_WIN) return CONTEMPT_FACTOR_FIFTYMOVERULE;
        else if(result == TB_WIN) return DBL_MAX;
        
    }

    table_entry_tt new_tt_entry = {
        .age = clock(),
        .evaluationDepth = depth,
        .hashCode = getHashCode(board)
    };

    if(depth == 0 || (timeLimit && clock() > *timeLimit) || board->victor) 
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
                updateMoveAccumulator(board, board->moveStackTop, 0);
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
                updateMoveAccumulator(board, poppedMove, 1);
                
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
                    if(pv) 
                    {
                        pv[pvIndex] = *moveList[index];
                        copyNMoves(&pv[pvIndex + 1], &pv[pvIndex + depth], depth - 1);
                    }
                }
            }
            index++;
        }
        freeMoveList(moveList);
    }
    return alpha;
}

typedef struct threadData {
    double alpha;
    double beta;
    int depth;
    bitboard* board;
    clock_t* endTime;
    move* pvTable;
    double* score;
} threadData;

DWORD WINAPI helperThreadFunction(LPVOID lpParam)
{
    threadData* data = (threadData*)lpParam;

    //Always fully evaluate at depth 1:
    *data->score = principalVariationSearch(data->board, -DBL_MAX, DBL_MAX, 1, 1, data->pvTable, 0, NULL);

    while(*data->endTime != 0)
    {
        *data->score = principalVariationSearch(data->board, data->alpha, data->beta, data->depth, data->depth, data->pvTable, 0, data->endTime);
    }
    return 0;
}

move* calculateBestMove(bitboard* board, int maxDepth, int maxTimeSeconds)
{
    #ifdef COUNT_NODES_VISITED
    nodesEvaluated = 0;
    nodesVisited = 0;
    clock_t startTime = clock();
    #endif

    if(entries) //Book moves
    {
        move* bookMove = NULL;
        if((bookMove = getBookMove(board))) return bookMove;
        else unloadBook();
    }
    else if(__builtin_popcountll(board->pieces_all) <= 5 && !(board->flags&0x30)) //3-5man sygyzy endgame with no castling rights.v
    {
        uint32_t ep = board->enPassantSquare;
        if(ep == -1) ep = 0;

        bool turn = PYRRHIC_WHITE;
        if(ISBLACK(board->turn)) turn = PYRRHIC_BLACK;

        int hasRepeated = get_pos_table_value(board->ht, board);
        if(hasRepeated > 1) hasRepeated = 1;

        struct TbRootMoves moveResults = {0};

        int result = tb_probe_root_dtz(board->pieces_w, board->pieces_b, 
                                        board->king_b|board->king_w, board->queen_b|board->queen_w, 
                                        board->rook_b|board->rook_w, board->bishop_b|board->bishop_w,
                                        board->knight_b|board->knight_w, board->pawn_b|board->pawn_w,
                                        (unsigned) board->movesSinceLastChange/2, ep, turn, hasRepeated, &moveResults);
        
        if(!result) DEBUG("Failed to probe sygyzy.");
        else
        {
            int bestScore = INT32_MIN;
            int bestIndex = 0;
            for(int i = 0; i < moveResults.size; i++)
            {
                if(moveResults.moves[i].tbRank > bestScore)
                {
                    bestScore = moveResults.moves[i].tbRank;
                    bestIndex = i;
                }
            }   
            
            move* bestMove = CALLOC(1, sizeof(move));
            bestMove->endSquare = PYRRHIC_MOVE_TO(moveResults.moves[bestIndex].move);
            bestMove->startSquare = PYRRHIC_MOVE_FROM(moveResults.moves[bestIndex].move);
            bestMove->flags = board->flags;
            bestMove->prevEnPassantSquare = board->enPassantSquare;
            bestMove->previousMovesSinceLastChange = board->movesSinceLastChange;
            bestMove->piece = findPieceOnSquare(board, bestMove->startSquare);

            if(PYRRHIC_MOVE_IS_QPROMO(moveResults.moves[bestIndex].move)) bestMove->promoteTo = QUEEN;
            else if(PYRRHIC_MOVE_IS_RPROMO(moveResults.moves[bestIndex].move)) bestMove->promoteTo = ROOK;
            else if(PYRRHIC_MOVE_IS_BPROMO(moveResults.moves[bestIndex].move)) bestMove->promoteTo = BISHOP;
            else if(PYRRHIC_MOVE_IS_NPROMO(moveResults.moves[bestIndex].move)) bestMove->promoteTo = KNIGHT;

            bestMove->capturedPiece = findPieceOnSquare(board, bestMove->endSquare);
            if(bestMove->capturedPiece) bestMove->capturedPieceSquare = bestMove->endSquare;

            bestMove->nextMove = NULL;

            return bestMove;
        }
    }

    move* principalVariation = CALLOC(maxDepth, sizeof(move));
    move* tempPVTable = NULL;

    clock_t endTime = clock() + CLOCKS_PER_SEC*maxTimeSeconds;

    HANDLE helperThreads[HELPER_THREAD_COUNT] = {0};
    DWORD helperThreadID[HELPER_THREAD_COUNT] = {0};
    threadData params[HELPER_THREAD_COUNT] = {0};

    clock_t terminateFlags[HELPER_THREAD_COUNT] = {LONG_MAX}; //These are passed as end times for the thread searches. Cancel threads by setting values to 0.
    double threadScores[HELPER_THREAD_COUNT] = {0};
    bitboard threadBoards[HELPER_THREAD_COUNT] = {0};
    for(int i = 0; i < HELPER_THREAD_COUNT; i++) 
    {
        params[i].pvTable = NULL;
        params[i].board = &threadBoards[i];
        params[i].score = &threadScores[i];
        params[i].endTime = &terminateFlags[i];
    }

    //Always fully evaluate at depth 1:
    double aspiration_expectedValue = principalVariationSearch(board, -DBL_MAX, DBL_MAX, 1, 1, principalVariation, 0, NULL);

    for(int currentDepth = 2; currentDepth <= maxDepth; currentDepth++)
    {
        if(clock() > endTime) break;

        tempPVTable = CALLOC(0.5*currentDepth*(currentDepth + 1), sizeof(move));
        copyNMoves(tempPVTable, principalVariation, currentDepth);

        //Initialize helper threads
        if(useHelperThreads)
        {
            for(int i = 0; i < HELPER_THREAD_COUNT; i++)
            {
                copy_board(params[i].board, board, 1);
                params[i].depth = currentDepth + (i%2);
                *params[i].endTime = LONG_MAX;
                params[i].pvTable = CALLOC(0.5*currentDepth*(currentDepth + 1), sizeof(move));
                copyNMoves(params[i].pvTable, principalVariation, currentDepth);

                helperThreads[i] = CreateThread(NULL, 0, helperThreadFunction, &params[i], 0, &helperThreadID[i]);
            }
        }
        

        double aspiration_margin = INITIAL_ASPIRATION_MARGIN;
        double alpha = aspiration_expectedValue - aspiration_margin;
        double beta = aspiration_expectedValue + aspiration_margin;

        //Aspiration Window Loop
        while(1)
        {
            if(clock() > endTime) break;
            double score = principalVariationSearch(board, alpha, beta, currentDepth, currentDepth, tempPVTable, 0, &endTime);

            if(score <= alpha)
            {
                alpha-= aspiration_margin;
                aspiration_margin*=ASPIRATION_MARGIN_MULT_FACTOR;
            }
            else if(score >= beta)
            {
                beta+= aspiration_margin;
                aspiration_margin*=ASPIRATION_MARGIN_MULT_FACTOR;
            }
            else
            {
                aspiration_expectedValue = score;
                break;
            }

            if(aspiration_margin > MAXIMUM_ASPIRATION_MARGIN)
            {
                alpha = -DBL_MAX;
                beta = DBL_MAX;
            }
        }
        
        
        if(useHelperThreads)
        {
            //Stop helper threads
            memset(terminateFlags, 0, HELPER_THREAD_COUNT * sizeof(clock_t));
            for(int i = 0; i < HELPER_THREAD_COUNT; i++) 
            {
                WaitForSingleObject(helperThreads[i], INFINITE);
                CloseHandle(helperThreads[i]);
            }
        
            //Move voting
            double worstScore = aspiration_expectedValue;
            for(int i = 0; i < HELPER_THREAD_COUNT; i++)  worstScore = min(*params[i].score, worstScore);

            int mainThreadVote = (aspiration_expectedValue - worstScore + 1) * currentDepth;
            int totalVoteWeights = mainThreadVote;
            int votes[HELPER_THREAD_COUNT] = {0.0};
            for(int i = 0; i < HELPER_THREAD_COUNT; i++) 
            {
                votes[i] = (*params[i].score - worstScore + 1) * params[i].depth;
                totalVoteWeights+= votes[i];
            }

            totalVoteWeights = rand()%totalVoteWeights;
            if(totalVoteWeights >= mainThreadVote)
            {
                int8_t threadIndex;
                for(threadIndex = 0; threadIndex < HELPER_THREAD_COUNT - 1; threadIndex++)
                {
                    if(totalVoteWeights < mainThreadVote) break;
                    else mainThreadVote += votes[threadIndex];
                }

                if(params[threadIndex].pvTable[0].startSquare != params[threadIndex].pvTable[0].endSquare)
                {
                    //Sanity check that a nonzero move was generated.
                    copyNMoves(tempPVTable, params[threadIndex].pvTable, currentDepth);
                }
                
            }
        }

        //Sanity check that a nonzero move was generated.
        if(tempPVTable[0].startSquare != tempPVTable[0].endSquare) copyNMoves(principalVariation, tempPVTable, currentDepth);
        FREE(tempPVTable);
        tempPVTable = NULL;
        for(int i = 0; i < HELPER_THREAD_COUNT; i++) 
        {
            FREE(params[i].pvTable);
            params[i].pvTable = NULL;
        }
    }

    if(tempPVTable) FREE(tempPVTable);

    move* bestMove = CALLOC(1, sizeof(move));
    memcpy(bestMove, &principalVariation[0], sizeof(move));

    if(bestMove->startSquare == bestMove->endSquare && bestMove->startSquare == 0)
    {
        DEBUG("Engine returned empty move.");
        //if(printDebugMessages) for(int i = 0; i < maxDepth; i++) printf("\tpv[%d] = %d->%d\n", i, principalVariation[i].startSquare, principalVariation[i].endSquare);
    }

    #ifdef COUNT_NODES_VISITED 
    if(nodesEvaluated) {
        float duration = (float)(clock()-startTime)/1000.0;
        printf("\nEvaluated %d nodes in %.2lf seconds at %.4f NPS\n", nodesEvaluated, duration, (float)nodesEvaluated/duration);
        printf("Visited %d nodes in %.2lf seconds at %.4f NPS\n", nodesVisited, duration, (float)nodesVisited/duration);
    }
    #endif

    FREE(principalVariation);
    principalVariation = NULL;
    return bestMove;
}