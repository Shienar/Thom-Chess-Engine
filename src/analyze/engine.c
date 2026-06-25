#include "analyze/engine.h"
#include "board/moves.h"
#include "board/bitboard.h"
#include "debug.h"
#include "analyze/book.h"
#include "analyze/sygyzy.h"
#include "pyrrhic/tbprobe.h"
#include <string.h>
#include <math.h>

int threadCount = 4;
int enablePonder = 0;
int isCalculating = 0;

int reductionTable[MAX_PLY][MAX_MOVES] = {0};

void initLMTable()
{
    if(reductionTable[25][25]) return;

    for(int depth = 0; depth < MAX_PLY; depth++)
    {
        int count = LM_BASE + LM_SCALE * depth * depth;
        for(int moveCount = 0; moveCount < MAX_MOVES; moveCount++)
        {
            if(depth >= LM_DEPTH && moveCount >= count)
                reductionTable[depth][moveCount] = (int)(0.99 + log(depth) * log(moveCount) / 3.14);
            else reductionTable[depth][moveCount] = 0;
        }
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
        return 0;
    }
}

int quiesce(searchThreadContext* context, int alpha, int beta, int ply)
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
     
    int best = forwardPropagate(board, context->accumulator);
    
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

    moveIterator* iter = create_move_iterator(board, 1, NULL, NULL, NULL, NULL, NULL);
    if(iter)
    {
        move* currentMove;
        while((currentMove = iterate_next_move(iter)) != NULL)
        {
            if(!moveFromStruct(board, currentMove))
            {
                updateMoveAccumulator(board, *currentMove, 0, context->accumulator, context->accumulatorTable);
                int score = -quiesce(context, -beta, -alpha, ply + 1);

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

int principalVariationSearch(searchThreadContext* context, int alpha, int beta, int maxDepth, int depth, int ply, PVar* myPV)
{
    assert(context);
    context->countedNodes++;
    bitboard* board = context->board;
    assert(board);

    myPV->length = 0;
    PVar childPV;

    int pvNode = (beta != alpha + 1);
    int inCheck = IS_IN_CHECK_ANY(board->flags);
    
    move* pvMove = &context->pv.line[ply];
    move* tt_move = NULL;
    move temp; //Copy in from TT instead of saving a ptr to a volatile TT slot.

    int searchedQuietIndices[MAX_MOVES] = {0};
    int searchedQuietCount = 0;

    if(ply > context->seldepth) context->seldepth = ply;
    
    if(depth <= 0 || (context->isPonder == 0 && context->endTime && clock() > *context->endTime && maxDepth > 1) || board->victor || ply >= MAX_PLY - 1) 
    {
        return quiesce(context, alpha, beta, ply);
    }

    //Mate distance pruning for non-root nodes.
    if(maxDepth != depth)
    {
        int a = _max(alpha, -SCORE_WIN + ply);
        int b = _min(beta, SCORE_WIN - ply - 1);
        if(a >= b) return a;
    }

    //Transposition table
    table_entry_tt new_tt_entry = {
        .depth = depth,
        .hashCode = board->hashCode
    };
    table_entry_tt* old_tt_entry = transposition_table_get(board, transpositionTable);
    if(old_tt_entry != NULL) 
    {
        //Move sorting
        temp = old_tt_entry->bestMove; 
        tt_move = &temp; 

        if(old_tt_entry->depth >= depth && !pvNode) 
        {
            if(old_tt_entry->nodeType == NODE_TYPE_PV) return old_tt_entry->evaluation;
            if(old_tt_entry->nodeType == NODE_TYPE_ALL && old_tt_entry->evaluation <= alpha) return alpha;
            if(old_tt_entry->nodeType == NODE_TYPE_CUT && old_tt_entry->evaluation >= beta) return beta;
        }
    }
    
    //Sygyzy
    if(depth >= sygyzyProbeDepth)
    {
        int result = getSygyzyResult(context->board);
        if(result != -1) 
        {
            //We aren't saving a pv move, but 
            //this isn't the root node so it doesn't matter much.
            new_tt_entry.nodeType = NODE_TYPE_CUT;
            new_tt_entry.evaluation = result;
            transposition_table_set(transpositionTable, new_tt_entry);
            return result;
        }
    }

    int score = 0;
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
            int nullScore = -principalVariationSearch(context, -alpha - 1, -alpha, maxDepth, depth - r, ply + 1, &childPV);
            applyNullMove(board);
            if(nullScore >= beta) return nullScore;
        }

    }

    moveIterator* iter;
    iter = create_move_iterator(board, 0, pvMove, tt_move, (ply == 0) ? context->searchedMoves : NULL, context->killerMoves[ply], context->historyTable);
    if(iter)
    {
        move* currentMove;
        int validMovesVisited = 0;
        int bestScore = -INT32_MAX;

        while((currentMove = iterate_next_move(iter)) != NULL)
        {
            if(!moveFromStruct(board, currentMove))
            {
                updateMoveAccumulator(board, *currentMove, 0, context->accumulator, context->accumulatorTable);

                int next_depth = depth - 1;
                int isQuietMove = (!IS_IN_CHECK_ANY(board->flags) && currentMove->capturedPiece == EMPTY_PIECE && !currentMove->promoteTo);
                if(IS_IN_CHECK_ANY(board->flags)) next_depth++;

                if(isQuietMove) next_depth -= reductionTable[depth][validMovesVisited];

                if(validMovesVisited == 0) score = -principalVariationSearch(context, -beta, -alpha, maxDepth, next_depth, ply + 1, &childPV);
                else
                {
                    score = -principalVariationSearch(context, -alpha - 1, -alpha, maxDepth, next_depth, ply + 1, &childPV);
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

                    if(isQuietMove)
                    {
                        //Killer heuristic
                        context->killerMoves[ply][1] = context->killerMoves[ply][0];
                        context->killerMoves[ply][0] = *currentMove;
                    
                        //History heuristic
                        int bonus = depth * depth;
                        int* dest = &context->historyTable[board->turn][PIECE(currentMove->piece) / 2][currentMove->endSquare];
                        *dest = _min(*dest + bonus, HISTORY_LIMIT);
                        int* straightArr = (int*) context->historyTable;
                        for(int i = 0; i < searchedQuietCount; i++)
                            straightArr[searchedQuietIndices[i]] = _max(straightArr[searchedQuietIndices[i]] - bonus, -HISTORY_LIMIT);
                    }

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
                validMovesVisited++;

                if(isQuietMove)
                    searchedQuietIndices[searchedQuietCount++] = (board->turn * 384) + ((PIECE(currentMove->piece) / 2) * 64) + currentMove->endSquare;
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
        context->score = principalVariationSearch(context, -INT32_MAX, INT32_MAX, currentDepth, currentDepth, 0, &context->pv);
        context->completedDepth = currentDepth;
    }
    else
    {

        int aspiration_margin = INITIAL_ASPIRATION_MARGIN;
        int alpha = context->score - aspiration_margin;
        int beta = context->score + aspiration_margin;
        while(1)
        {
            if(context->isPonder == 0 && clock() > *endTime) break;

            updateAccumulatorFromTable(board, acc, accumulatorTable);
            int score = principalVariationSearch(context, alpha, beta, currentDepth, currentDepth, 0, &context->pv);

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
                alpha = -INT32_MAX;
                beta = INT32_MAX;
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
        
        if(abs(context->score) > MIN_MATE_SCORE) break;
    }

    //Load the last stable pv
    memcpy(&context->pv, &tempPV, sizeof(PVar));

    return 0;
}

void findBestThread(searchThreadContext* mainThread, searchThreadContext* helperThreads, move* bestMove, move* ponderMove)
{
    searchThreadContext* best = mainThread;
    int bestDepth = best->completedDepth;
    int bestScore = best->score;
    int totalNodes = mainThread->countedNodes;
    if(helperThreads)
    {
        for(int i = 0; i < threadCount - 1; i++)
        {
            if(!IS_VALID_MOVE(helperThreads[i].pv.line[0])) continue;
            int curDepth = helperThreads[i].completedDepth;
            int curScore = helperThreads[i].score;

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
    int absScore = abs(bestScore);
    assert(absScore <= SCORE_WIN);
    if(absScore >= MIN_MATE_SCORE)
    {
        int mateInPlies = SCORE_WIN - absScore;
        int mateInMoves = (mateInPlies + 1) / 2;
        if(bestScore < 0) mateInMoves = -mateInMoves;
        printf(" score mate %d", mateInMoves);
    }
    else printf(" score cp %d", bestScore);
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
    memset(context->historyTable, 0, sizeof(context->historyTable));
    
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
            int absScore = abs(context->score);
            assert(absScore <= SCORE_WIN);
            if(absScore >= MIN_MATE_SCORE)
            {
                int mateInPlies = SCORE_WIN - absScore;
                int mateInMoves = (mateInPlies + 1) / 2;
                if(context->score < 0) mateInMoves = -mateInMoves;
                printf(" score mate %d", mateInMoves);
            }
            else printf(" score cp %d", context->score);
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

        if(abs(context->score) > MIN_MATE_SCORE) break;
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