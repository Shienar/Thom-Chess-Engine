#include "analyze/engine.h"
#include "board/moves.h"
#include "board/bitboard.h"
#include "debug.h"
#include "analyze/book.h"
#include "analyze/sygyzy.h"
#include "pyrrhic/tbprobe.h"
#include <string.h>
#include <float.h>
#include <math.h>

int threadCount = 4;
int enablePonder = 0;
int isCalculating = 0;

int lateMoveCounts[MAX_PLY] = {0};

void initLMTable()
{
    if(lateMoveCounts[0]) return;

    for(int depth = 0; depth < MAX_PLY; depth++)
    {
        lateMoveCounts[depth] = LM_BASE + LM_SCALE * depth * depth;
    }
}

int perft(bitboard* board, int depth, int maxDepth, int verbose)
{
    if(!depth) return 1;
    int nodes = 0;

    move moveList[MAX_MOVES];
    int count = generateMoveList(moveList, board, 0);
    for(int index = 0; index < count; index++)
    {
        if(!moveFromStruct(board, &moveList[index]))
        {
            int branchNodes = perft(board, depth - 1, maxDepth, 0);
            nodes += branchNodes;
            if(verbose) 
            {
                char fromSquare[3] = {'\0'};
                char toSquare[3] = {'\0'};
                getSquareName(moveList[index].startSquare, fromSquare);
                getSquareName(moveList[index].endSquare, toSquare);
                printf("Move %s%s: nodes %d\n", fromSquare, toSquare, branchNodes);
            }
            unmove(board);
        }
    }
    return nodes;
}

int evaluateEndstate(bitboard* board, int ply)
{
    assert(board->victor);

    if(board->victor == VICTOR_WHITE)
    {
        return (board->turn == WHITE) ? (SCORE_WIN - ply) : -(SCORE_WIN - ply);
    }
    else if(board->victor == VICTOR_BLACK)
    {
        return (board->turn == BLACK) ? (SCORE_WIN - ply) : -(SCORE_WIN - ply);
    }
    else
    {
        //Not going to worry about contempt with negamax.
        return 0.0;
    }
}

int quiesce(searchThreadContext* context, float alpha, float beta, int ply)
{
    context->countedNodes++;

    bitboard* board = context->board;

    if(board->victor) return evaluateEndstate(board, ply);

    table_entry_tt* entry = transposition_table_get(board, transpositionTable);
    if(entry)
    {
        if(entry->nodeType == NODE_TYPE_PV) return entry->evaluation;
        if(entry->nodeType == NODE_TYPE_ALL && entry->evaluation <= alpha) return alpha;
        if(entry->nodeType == NODE_TYPE_CUT && entry->evaluation >= beta) return beta;
    }
     
    float best = forwardPropagate(board, context->accumulator);
    
    table_entry_tt shallowEntry = {
        .depth = 0,
        .hashCode = board->hashCode,
        .nodeType = NODE_TYPE_PV,
        .evaluation = best,
    };
    transposition_table_set(transpositionTable, shallowEntry);

    if(best >= beta || ply >= MAX_PLY - 1) return best;
    if(best > alpha) alpha = best;

    //Delta pruning
    if(LARGE_DELTA + best < alpha) return best;

    moveIterator* iter = create_move_iterator(board, 1, NULL, NULL);
    if(iter)
    {
        move* currentMove;
        while((currentMove = iterate_next_move(iter)) != NULL)
        {
            if(!moveFromStruct(board, currentMove))
            {
                updateMoveAccumulator(board, *currentMove, 0, context->accumulator, context->accumulatorTable);
                float score = -quiesce(context, -beta, -alpha, ply + 1);

                unmove(board);
                updateMoveAccumulator(board, *currentMove, 1, context->accumulator, context->accumulatorTable);

                if(score >= beta)
                {
                    destroy_move_iterator(iter);
                    return score;
                }
                if(score > best) best = score;
                if(score > alpha) alpha = score;
            }
        }
        destroy_move_iterator(iter);
    }
    return best;
}

int principalVariationSearch(searchThreadContext* context, float alpha, float beta, int maxDepth, int depth, int ply, PVar* myPV)
{
    assert(context);
    context->countedNodes++;
    bitboard* board = context->board;
    int examineQuiets = 1;
    assert(board);

    myPV->length = 0;
    PVar childPV;

    int pvNode = (beta != alpha + 1);
    int inCheck = IS_IN_CHECK_ANY(board->flags);
    
    if(ply > context->seldepth) context->seldepth = ply;
    
    if(depth == 0 || (context->isPonder == 0 && context->endTime && clock() > *context->endTime && maxDepth > 1) || board->victor || ply >= MAX_PLY - 1) 
    {
        return quiesce(context, alpha, beta, ply);
    }

    //Mate distance pruning for non-root nodes.
    if(maxDepth != depth)
    {
        float a = _max(alpha, -SCORE_WIN + ply);
        float b = _min(beta, SCORE_WIN - ply - 1);
        if(a >= b) return a;
    }

    //Transposition table
    table_entry_tt new_tt_entry = {
        .depth = depth,
        .hashCode = board->hashCode
    };
    table_entry_tt* old_tt_entry = NULL;
    if((old_tt_entry = transposition_table_get(board, transpositionTable)) != NULL && 
        old_tt_entry->depth >= depth &&
            (old_tt_entry->nodeType == NODE_TYPE_PV ||
            (old_tt_entry->nodeType == NODE_TYPE_ALL && old_tt_entry->evaluation <= alpha) ||
            (old_tt_entry->nodeType == NODE_TYPE_CUT && old_tt_entry->evaluation >= beta)))
    {
        if(!pvNode) return old_tt_entry->evaluation;
        else if(old_tt_entry->nodeType == NODE_TYPE_PV)
        {
            myPV->line[0] = old_tt_entry->bestMove;
            myPV->length = 1;
            return old_tt_entry->evaluation;
        }
    }


    //Sygyzy
    if(depth >= sygyzyProbeDepth)
    {
        float result = getSygyzyResult(context->board);
        if(result != -1.0f) 
        {
            //We aren't saving a pv move, but 
            //this isn't the root node so it doesn't matter much.
            new_tt_entry.nodeType = NODE_TYPE_CUT;
            new_tt_entry.evaluation = result;
            transposition_table_set(transpositionTable, new_tt_entry);
            return result;
        }
    }

    float score = 0.0f;
    if(old_tt_entry) score = old_tt_entry->evaluation;
    else
    {
        score = forwardPropagate(board, context->accumulator);
        table_entry_tt shallowEntry = {
            .depth = 0,
            .hashCode = board->hashCode,
            .nodeType = NODE_TYPE_PV,
            .evaluation = score,
        };
        transposition_table_set(transpositionTable, shallowEntry);
    }

    if(!pvNode && !inCheck)
    {
        //Reverse Futility Pruning
        if(depth <= REVERSE_FUTILITY_PRUNING_DEPTH && score >= beta + REVERSE_FUTILITY_MARGIN * depth)
        {
            return score;
        }

        //Futility pruning
        if(depth <= FUTILITY_PRUNING_DEPTH && (score + FUTILITY_MARGIN) <= alpha )
        {
            return score;
        }

        //Null move pruning
        if(score >= beta && depth >= NULLMOVE_PRUNING_DEPTH)
        {
            int r = 3;
            applyNullMove(board);
            float nullScore = -principalVariationSearch(context, -alpha - 1.0f, -alpha, maxDepth, depth - r, ply + 1, &childPV);
            applyNullMove(board);
            if(nullScore >= beta) return nullScore;
        }

    }

    moveIterator* iter;
    if(ply > 0) iter = create_move_iterator(board, 0, NULL, NULL);
    else iter = create_move_iterator(board, 0, &context->pv.line[ply], context->searchedMoves);
    if(iter)
    {
        move* currentMove;
        int validMovesVisited = 0;
        float bestScore = -FLT_MAX;

        while((currentMove = iterate_next_move(iter)) != NULL)
        {

            //Move count pruning
            //if(score > -MIN_MATE_SCORE && depth <= LM_DEPTH && validMovesVisited >= lateMoveCounts[depth]) examineQuiets = 0;

            if(!moveFromStruct(board, currentMove))
            {
                int isQuiet = (currentMove->capturedPiece == EMPTY_PIECE && (IS_IN_CHECK_ANY(board->flags)) == 0);
                if(!examineQuiets && isQuiet)
                {
                    unmove(board);
                    continue;
                }

                updateMoveAccumulator(board, *currentMove, 0, context->accumulator, context->accumulatorTable);

                int next_depth = depth - 1;
                if(IS_IN_CHECK_ANY(board->flags)) next_depth++;

                validMovesVisited++;

                if(validMovesVisited == 1) score = -principalVariationSearch(context, -beta, -alpha, maxDepth, next_depth, ply + 1, &childPV);
                else
                {
                    score = -principalVariationSearch(context, -alpha - 1.0f, -alpha, maxDepth, next_depth, ply + 1, &childPV);
                    //Re-search PV node
                    if(score > alpha && pvNode) score = -principalVariationSearch(context, -beta, -alpha, maxDepth, next_depth, ply + 1, &childPV);
                }
                
                unmove(board);
                updateMoveAccumulator(board, *currentMove, 1, context->accumulator, context->accumulatorTable);
                
                if(score >= beta)
                {
                    new_tt_entry.nodeType = NODE_TYPE_CUT;
                    new_tt_entry.evaluation = score;
                    transposition_table_set(transpositionTable, new_tt_entry);
                    destroy_move_iterator(iter);
                    return score;
                }
                else if(score > bestScore)
                {
                    bestScore = score;

                    if(score > alpha)
                    {
                        new_tt_entry.nodeType = NODE_TYPE_PV;
                        new_tt_entry.evaluation = score;
                        new_tt_entry.bestMove = *currentMove;
                        transposition_table_set(transpositionTable, new_tt_entry);
                        alpha = score;
                        
                        myPV->line[0] = *currentMove;
                        memcpy(&myPV->line[1], childPV.line, childPV.length * sizeof(move));
                        myPV->length = childPV.length + 1;
                    }
                }
            }
        }

        if(bestScore < alpha)
        {
            //Don't save a bestmove since we couldn't find one that fits in window.
            new_tt_entry.nodeType = NODE_TYPE_ALL;
            new_tt_entry.evaluation = bestScore;
            transposition_table_set(transpositionTable, new_tt_entry);
        }
        destroy_move_iterator(iter);
    }
    return alpha;
}

void printResultingMoves(move bestMove, move ponderMove, int isBookMove)
{
    char startSq[3];
    char endSq[3];
    int startSquare = bestMove.startSquare;
    int endSquare = bestMove.endSquare;

    getSquareName(startSquare, startSq);
    getSquareName(endSquare, endSq);

    if(isBookMove) printf("info string Book move played: %s%s\n", startSq, endSq);
    printf("bestmove %s%s", startSq, endSq);
    
    if(bestMove.promoteTo)
    {
        switch(bestMove.promoteTo)
        {
            case QUEEN:
                printf("q");
                break;
            case KNIGHT:
                printf("n");
                break;
            case ROOK:
                printf("r");
                break;
            case BISHOP:
                printf("b");
                break;
            default:
                break;
        }
    }

    if(enablePonder && IS_VALID_MOVE(ponderMove))
    {
        startSquare = ponderMove.startSquare;
        endSquare = ponderMove.endSquare;

        getSquareName(startSquare, startSq);
        getSquareName(endSquare, endSq);

        printf(" ponder %s%s", startSq, endSq);

        if(ponderMove.promoteTo)
        {
            switch(ponderMove.promoteTo)
            {
                case QUEEN:
                    printf("q");
                    break;
                case KNIGHT:
                    printf("n");
                    break;
                case ROOK:
                    printf("r");
                    break;
                case BISHOP:
                    printf("b");
                    break;
                default:
                    break;
            }
        }
        
    }

    printf("\n");
    fflush(stdout);
}

void aspiration_window(searchThreadContext* context, int currentDepth)
{
    bitboard* board = context->board;
    volatile clock_t* endTime = context->endTime;
    accumulator* acc = context->accumulator;
    accumulatorRefreshTable* accumulatorTable = context->accumulatorTable;
    
    if(currentDepth < MIN_ASPIRATION_DEPTH)
    {
        updateAccumulatorFromTable(board, context->accumulator, context->accumulatorTable);
        context->score = principalVariationSearch(context, -FLT_MAX, FLT_MAX, currentDepth, currentDepth, 0, &context->pv);
        context->completedDepth = currentDepth;
    }
    else
    {

        float aspiration_margin = INITIAL_ASPIRATION_MARGIN;
        float alpha = context->score - aspiration_margin;
        float beta = context->score + aspiration_margin;
        while(1)
        {
            if(context->isPonder == 0 && clock() > *endTime) break;

            updateAccumulatorFromTable(board, acc, accumulatorTable);
            float score = principalVariationSearch(context, alpha, beta, currentDepth, currentDepth, 0, &context->pv);

            if(score <= alpha)
            {
                alpha-= aspiration_margin;
                aspiration_margin*=ASPIRATION_MARGIN_MULT_FACTOR;
            }
            else if(score >= beta)
            {
                beta+= aspiration_margin;
                aspiration_margin*=ASPIRATION_MARGIN_MULT_FACTOR;
                context->completedDepth = currentDepth;
            }
            else
            {
                context->score = score;
                context->completedDepth = currentDepth;
                break;
            }

            if(aspiration_margin > MAXIMUM_ASPIRATION_MARGIN)
            {
                alpha = -FLT_MAX;
                beta = FLT_MAX;
            }
        }
    }
}

THREAD_RETURN helperThreadFunction(THREAD_PARAM param)
{
    searchThreadContext* context = (searchThreadContext*)param;
    context->seldepth = 0;
    context->completedDepth = 0;
    PVar tempPV;

    for(int currentDepth = 1; currentDepth <= context->maxDepth; currentDepth+=context->deepeningSkip)
    {
        if(context->isPonder == 0 && clock() > *context->endTime) break;

        aspiration_window(context, currentDepth);
        if(clock() <= *context->endTime) memcpy(&tempPV, &context->pv, sizeof(PVar));
        
        if(fabsf(context->score) > MIN_MATE_SCORE) break;
    }

    //Load the last stable pv
    memcpy(&context->pv, &tempPV, sizeof(PVar));

    return 0;
}

void findBestThread(searchThreadContext* mainThread, searchThreadContext* helperThreads, move* bestMove, move* ponderMove)
{
    searchThreadContext* best = mainThread;
    int bestDepth = best->completedDepth;
    float bestScore = best->score;
    int totalNodes = mainThread->countedNodes;
    if(helperThreads)
    {
        for(int i = 0; i < threadCount - 1; i++)
        {
            if(!IS_VALID_MOVE(helperThreads[i].pv.line[0])) continue;
            int curDepth = helperThreads[i].completedDepth;
            float curScore = helperThreads[i].score;

            if(curDepth >= bestDepth || curScore > MIN_MATE_SCORE) 
            {
                best = &helperThreads[i];
                bestDepth = best->completedDepth;
                bestScore = best->score;
            }

            totalNodes += helperThreads[i].countedNodes;
        }
    }
    
    *bestMove = best->pv.line[0];
    *ponderMove = best->pv.line[1]; //Invalid & isPonder checks come later.

    int milliseconds = (double) (clock() - mainThread->startTime) / (CLOCKS_PER_SEC / 1000.0);
    milliseconds = _max(milliseconds, 1);
    int NPS = totalNodes / (milliseconds / 1000.0);
    printf("info depth %d seldepth %d nodes %d nps %d time %d", bestDepth, best->seldepth, totalNodes, NPS, milliseconds);
    float absScore = fabsf(bestScore);
    assert(absScore <= SCORE_WIN);
    if(absScore > MIN_MATE_SCORE)
    {
        int mateInPlies = SCORE_WIN - absScore;
        int mateInMoves = (mateInPlies + 1) / 2;
        if(bestScore < 0) mateInMoves = -mateInMoves;
        printf(" score mate %d", mateInMoves);
    }
    else printf(" score cp %d", (int)lroundf(bestScore));
    if(bestDepth)
    {
        printf(" pv");
        for(int i = 0; i < best->pv.length; i++)
        {
            move m = best->pv.line[i];
            if(!IS_VALID_MOVE(m)) break;
            char startSq[3] = {'\0'};
            char endSq[3] = {'\0'};
            getSquareName(m.startSquare, startSq);
            getSquareName(m.endSquare, endSq);
            printf(" %s%s", startSq, endSq);
            if(m.promoteTo)
            {
                switch(m.promoteTo)
                {
                    case QUEEN:
                        printf("q");
                        break;
                    case KNIGHT:
                        printf("n");
                        break;
                    case ROOK:
                        printf("r");
                        break;
                    case BISHOP:
                        printf("b");
                        break;
                    default:
                        break;
                }
            }
        }
    }
    printf("\n");
    fflush(stdout);
}

THREAD_RETURN calculateBestMove(THREAD_PARAM param)
{
    srand(time(NULL));

    searchThreadContext* context = (searchThreadContext*)param;
    
    int maxDepth = context->maxDepth;
    volatile clock_t *endTime = context->endTime;
    context->startTime = clock();
    
    move bestMove, ponderMove = (move){0};
    int helperThreadCount = threadCount - 1;

    bitboard* board = context->board;
    assert(!board->victor);
    board->historyIndex = 0;
    context->countedNodes = 0;
    context->seldepth = 0;
    context->completedDepth = 0;

    //Book moves
    if(entries)
    {
        context->pv.line[0] = getBookMove(board);
        if(IS_VALID_MOVE(context->pv.line[0])) { printResultingMoves(context->pv.line[0], (move){0}, 1); isCalculating = 0; return 0; }
        else unloadBook();
    }
    
    //Sygyzy move recommendations
    filterSygyzyMoves(board, context->searchedMoves);

    THREADTYPE *helperThreads = NULL;
    searchThreadContext* helperThreadContext = NULL;
    clock_t* terminateFlags = NULL;
    bitboard* threadBoards = NULL;
    accumulator* threadAccumulators = NULL;
    accumulatorRefreshTable** threadRefreshTables = NULL;

    if(helperThreadCount > 0)
    {
        helperThreads = calloc(helperThreadCount, sizeof(THREADTYPE));
        helperThreadContext = calloc(helperThreadCount, sizeof(searchThreadContext));
        terminateFlags = calloc(helperThreadCount, sizeof(clock_t));
        threadBoards = calloc(helperThreadCount, sizeof(bitboard));
        threadAccumulators = calloc(helperThreadCount, sizeof(accumulator));
        threadRefreshTables = calloc(helperThreadCount, sizeof(accumulatorRefreshTable*));
        
        for(int i = 0; i < helperThreadCount; i++) 
        {
            helperThreadContext[i].board = &threadBoards[i];
            memcpy(helperThreadContext[i].board, board, sizeof(bitboard));
            helperThreadContext[i].endTime = &terminateFlags[i];
            terminateFlags[i] = *context->endTime;
            helperThreadContext[i].maxDepth = context->maxDepth;
            helperThreadContext[i].deepeningSkip = 1 + (i%3);

            threadRefreshTables[i] = createRefreshTable();
            helperThreadContext[i].accumulatorTable = threadRefreshTables[i];
            helperThreadContext[i].accumulator = &threadAccumulators[i];

            memcpy(helperThreadContext[i].searchedMoves, context->searchedMoves, 16*sizeof(move));
            THREAD_START(helperThreads[i], helperThreadFunction, &helperThreadContext[i]);
        }
    }
    
    for(int currentDepth = 1; currentDepth <= maxDepth; currentDepth++)
    {
        if(currentDepth > 1 && (clock() > *endTime || context->countedNodes > context->maxNodes)) break;

        aspiration_window(context, currentDepth);

        if(currentDepth < maxDepth)
        {
            int totalNodes = context->countedNodes;
            for(int i = 0; i < helperThreadCount; i++) totalNodes += helperThreadContext[i].countedNodes; //very volatile, basically a best-guess until the cleanup.
            int milliseconds = (double) (clock() - context->startTime) / (CLOCKS_PER_SEC / 1000.0);
            milliseconds = _max(milliseconds, 1);
            int NPS = totalNodes / (milliseconds / 1000.0);
            printf("info depth %d seldepth %d nodes %d nps %d time %d", currentDepth, context->seldepth, totalNodes, NPS, milliseconds);
            float absScore = fabsf(context->score);
            assert(absScore <= SCORE_WIN);
            if(absScore > MIN_MATE_SCORE)
            {
                int mateInPlies = SCORE_WIN - absScore;
                int mateInMoves = (mateInPlies + 1) / 2;
                if(context->score < 0) mateInMoves = -mateInMoves;
                printf(" score mate %d", mateInMoves);
            }
            else printf(" score cp %d", (int)lroundf(context->score));
            printf(" pv");
            for(int i = 0; i < context->pv.length; i++)
            {
                move m = context->pv.line[i];
                if(!IS_VALID_MOVE(m)) break;
                char startSq[3] = {'\0'};
                char endSq[3] = {'\0'};
                getSquareName(m.startSquare, startSq);
                getSquareName(m.endSquare, endSq);
                printf(" %s%s", startSq, endSq);
                if(m.promoteTo)
                {
                    switch(m.promoteTo)
                    {
                        case QUEEN:
                            printf("q");
                            break;
                        case KNIGHT:
                            printf("n");
                            break;
                        case ROOK:
                            printf("r");
                            break;
                        case BISHOP:
                            printf("b");
                            break;
                        default:
                            break;
                    }
                }
            }
            printf("\n");
            fflush(stdout);
        }
        
        bestMove = context->pv.line[0];
        ponderMove = context->pv.line[1];

        if(fabsf(context->score) > MIN_MATE_SCORE) break;
    }
    if(helperThreadCount > 0)
    {
        memset(terminateFlags, 0, helperThreadCount * sizeof(clock_t));
        for(int i = 0; i < helperThreadCount; i++) 
        {
            THREAD_WAIT(helperThreads[i]);
        }
    }
    findBestThread(context, helperThreadContext, &bestMove, &ponderMove);

    free(threadBoards);
    free(helperThreads);
    free(helperThreadContext);
    free(terminateFlags);

    free(threadAccumulators);
    for(int i = 0; i < helperThreadCount; i++) destroyRefreshTable(threadRefreshTables[i]);
    free(threadRefreshTables);

    if(!IS_VALID_MOVE(bestMove) && bestMove.startSquare == 0)
    {
        char FEN[100] = { '\0' };
        export_fen_from_board(board, FEN);
        DEBUG_ERROR("Engine returned empty move on %s", FEN);
    }

    printResultingMoves(bestMove, ponderMove, 0);
    isCalculating = 0;
    return 0;
}