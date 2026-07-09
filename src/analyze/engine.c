#include "analyze/engine.h"
#include "board/moves.h"
#include "board/bitboard.h"
#include "debug.h"
#include "analyze/book.h"
#include "analyze/sygyzy.h"
#include "pyrrhic/tbprobe.h"
#include <string.h>
#include <math.h>

int threadCount = 1;
int enablePonder = 0;
int isCalculating = 0;
int suppressUCIMessages = 0;

const int min_aspiration_depth = 5;
const int reverse_futility_pruning_depth = 4;
const int futility_pruning_depth = 2;
const int nullmove_pruning_depth = 5;
const int probcut_depth = 8;
const int probcut_depth_reduction = 4;
const int tt_reduction_depth = 7;
const int tt_reduction_min_depth_offset = 3;
const int lm_depth = 4;

int initial_aspiration_margin = 38;
int maximum_aspiration_margin = 150;
float aspiration_margin_mult_factor = 2.0f;

int reverse_futility_margin = 205;
int reverse_futility_margin_improving = 120;
int futility_margin = 375;
int delta_pruning_offset = 475;

int probcut_offset = 400;
int probcut_offset_improving = 250;


int reductionTable[MAX_PLY][MAX_MOVES] = {0};

void initSearchTables()
{
    if(reductionTable[4][10]) return;

    for(int depth = 0; depth < MAX_PLY; depth++)
    {
        int count = 2.0f + 0.5f * depth * depth;
        for(int moveCount = 0; moveCount < MAX_MOVES; moveCount++)
        {
            if(depth >= lm_depth && moveCount >= count)
                reductionTable[depth][moveCount] = (int)( 0.99f + log(depth) * log(moveCount) / 3.14f);
            else reductionTable[depth][moveCount] = 0;
        }
    }
}

//Draws get ignored. Naturally stops depth at checkmate/stalemate positions.
int perft(bitboard* board, int depth, int verbose)
{
    if(!depth) return 1;
    int nodes = 0;

    move_c moveList[MAX_MOVES];
    int count = generateMoveList(moveList, board, 0);
    for(int index = 0; index < count; index++)
    {
        if(moveFromStruct(board, moveList[index])) continue;
        
        int branchNodes = perft(board, depth - 1, 0);
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
    
    return nodes;
}

int quiescentSearch(searchThreadContext* context, int alpha, int beta, int ply)
{
    context->countedNodes++;
    
    bitboard* board = context->board;

    if(isDraw(board))
        return (ply & 3) - 1;
    if(ply >= MAX_PLY - 1 || (context->endTime && clock() > *context->endTime)) 
        return forwardPropagate(board, context->accumulator);

    int lowestBound = alpha;
    move_c* tt_move = NULL;
    move_c temp; //Copy in from TT instead of saving a ptr to a volatile TT slot.

    int best;
    uint8_t tt_hit;
    table_entry_tt entry = transposition_table_get(board, context->tt, &tt_hit, ply);
    if(tt_hit)
    {
        if(entry.nodeType == NODE_TYPE_PV ||
          (entry.nodeType == NODE_TYPE_ALL && entry.evaluation <= alpha) ||
          (entry.nodeType == NODE_TYPE_CUT && entry.evaluation >= beta)) 
            return entry.evaluation;

        temp.raw = entry.bestMove;
        tt_move = &temp;
        best = entry.evaluation;
    }
    else 
    {
        best = forwardPropagate(board, context->accumulator);
    
        table_entry_tt shallowEntry = {
            .depth = 0,
            .hashCode = board->hashCode,
            .nodeType = NODE_TYPE_UNKNOWN,
            .evaluation = best,
            .age = board->halfMoveCount
        };
        transposition_table_set(context->tt, shallowEntry, ply);
    }

    if(best >= beta) return best;
    alpha = _max(alpha, best);

    //Delta pruning
    if(delta_pruning_offset + best < alpha) return best;

    moveIterator* iter = create_move_iterator(board, GET_CAPTURES_AND_PROMOTIONS, NULL, tt_move, NULL, NULL, NULL);
    int validMovesVisited = 0;
    if(iter)
    {
        move_c* currentMove;
        while((currentMove = iterate_next_move(iter)) != NULL)
        {
            if(moveFromStruct(board, *currentMove)) continue;
            validMovesVisited++;

            move_d detailedMove = board->history[board->historyIndex - 1];
            updateMoveAccumulator(board, detailedMove, 0, context->accumulator, context->refreshTable);
            int score = -quiescentSearch(context, -beta, -alpha, ply + 1);

            unmove(board);
            updateMoveAccumulator(board, detailedMove, 1, context->accumulator, context->refreshTable);

            if(score > best)
            {
                best = score;

                if(score > alpha) alpha = score;

                if(alpha >= beta)
                    break;
            }
        }
        destroy_move_iterator(iter);
    }
    
    table_entry_tt shallowEntry = {
        .depth = 0,
        .hashCode = board->hashCode,
        .nodeType = (best >= beta) ? NODE_TYPE_CUT : ( (best > lowestBound) ? NODE_TYPE_PV : NODE_TYPE_ALL),
        .evaluation = best,
        .age = board->halfMoveCount
    };
    transposition_table_set(context->tt, shallowEntry, ply);
    return best;
}

int principalVariationSearch(searchThreadContext* context, int alpha, int beta, int depth, int ply, PVar* myPV)
{
    assert(context);
    context->countedNodes++;
    bitboard* board = context->board;
    assert(board);

    myPV->length = 0;
    PVar childPV;

    int pvNode = (beta != alpha + 1);
    int inCheck = IS_IN_CHECK_ANY(board->flags);
    
    move_c* pvMove = &context->pv.line[ply];
    move_c* tt_move = NULL;
    move_c temp; //Copy in from TT instead of saving a ptr to a volatile TT slot.

    int searchedQuietIndices[MAX_MOVES] = {0};
    int searchedQuietCount = 0;

    if(ply > context->seldepth) context->seldepth = ply;
    
    if(isDraw(board)) return (ply & 3) - 1;

    //Mate distance pruning for non-root nodes.
    if(ply != 0)
    {
        int a = _max(alpha, -SCORE_WIN + ply);
        int b = _min(beta, SCORE_WIN - ply - 1);
        if(a >= b) return a;
    }

    int score = 0;

    //Transposition table
    table_entry_tt new_tt_entry = {
        .depth = depth,
        .hashCode = board->hashCode,
        .age = board->halfMoveCount
    };
    uint8_t hit;
    table_entry_tt old_tt_entry = transposition_table_get(board, context->tt, &hit, ply);
    if(hit) 
    {
        if(old_tt_entry.depth >= depth && (!pvNode || depth == 0)) 
        {
            if(old_tt_entry.nodeType == NODE_TYPE_PV ||
               (old_tt_entry.nodeType == NODE_TYPE_ALL && old_tt_entry.evaluation <= alpha) ||
               (old_tt_entry.nodeType == NODE_TYPE_CUT && old_tt_entry.evaluation >= beta)) 
                    return old_tt_entry.evaluation;
        }
        
        temp.raw = old_tt_entry.bestMove; 
        tt_move = &temp;
        score = old_tt_entry.evaluation;
    }

    if(ply >= MAX_PLY - 1) 
        return forwardPropagate(board, context->accumulator);
    if(depth <= 0 || (context->isPonder == 0 && context->endTime && clock() > *context->endTime && ply >= 1))
        return quiescentSearch(context, alpha, beta, ply);
    
    //Sygyzy
    if(!pvNode && depth >= sygyzyProbeDepth)
    {
        int result = getSygyzyResult(context->board);
        if(result != -1) 
        {
            new_tt_entry.nodeType = NODE_TYPE_PV;
            new_tt_entry.evaluation = result;
            transposition_table_set(context->tt, new_tt_entry, ply);
            return result;
        }
    }

    if(!hit) 
    {
        if(inCheck) score = -INT32_MAX;
        else
        {
            score = forwardPropagate(board, context->accumulator);
            table_entry_tt shallowEntry = {
                .depth = 0,
                .hashCode = board->hashCode,
                .nodeType = NODE_TYPE_UNKNOWN,
                .evaluation = score,
                .age = board->halfMoveCount
            };
            transposition_table_set(context->tt, shallowEntry, ply);
        }
    }

    //Improving
    if(inCheck) context->improving[ply] = 0;
    else context->improving[ply] = (ply >= 2) ? (score > context->evalHistory[ply - 2]) : 1;

    context->killerMoves[ply+1][0].raw = context->killerMoves[ply+1][1].raw = 0;

    if(!pvNode && !inCheck && abs(score) < MIN_MATE_SCORE)
    {
        //Reverse Futility Pruning
        if(depth <= reverse_futility_pruning_depth)
        {
            int reducedVal;
            if(context->improving[ply]) reducedVal = score - reverse_futility_margin_improving * depth;
            else reducedVal = score - reverse_futility_margin * depth;

            if(reducedVal >= beta) return beta;
        }

        //Futility pruning
        if(depth <= futility_pruning_depth && (score + futility_margin) <= alpha)
            return alpha;

        //Null move pruning
        if(score >= beta && depth >= nullmove_pruning_depth &&
            (board->pieces_all ^ (board->pieces[WHITE_KING] | board->pieces[BLACK_KING] | board->pieces[WHITE_PAWN] | board->pieces[BLACK_PAWN])))
        {
            int r = 3 + depth / 6;
            applyNullMove(board);
            int nullScore = -principalVariationSearch(context, -beta, -beta + 1, depth - r, ply + 1, &childPV);
            applyNullMove(board);
            if(nullScore >= beta) return beta;
        }

        //Probcut
        if(depth >= probcut_depth)
        {
            int nextDepth = depth - probcut_depth_reduction;
            int pBeta = (context->improving[ply]) ? beta + probcut_offset_improving: beta + probcut_offset;

            if(score >= pBeta && pBeta < MIN_MATE_SCORE && (!hit || old_tt_entry.depth < nextDepth))
            {
                int probCutScore = INT32_MIN;
                moveIterator* iter = create_move_iterator(board, GET_WINNING_CAPTURES, pvMove, tt_move, NULL, context->killerMoves[ply], context->historyTable);
                if(iter)
                {
                    move_c* currentMove;
                    while((currentMove = iterate_next_move(iter)) != NULL)
                    {
                        if(moveFromStruct(board, *currentMove)) continue;
                        
                        move_d detailedMove = board->history[board->historyIndex - 1];
                        updateMoveAccumulator(board, detailedMove, 0, context->accumulator, context->refreshTable);

                        probCutScore = -principalVariationSearch(context, -pBeta - 1, -pBeta, nextDepth, ply + 1, &childPV);

                        unmove(board);
                        updateMoveAccumulator(board, detailedMove, 1, context->accumulator, context->refreshTable);

                        if(probCutScore >= pBeta)
                        {
                            if(!hit || old_tt_entry.depth < nextDepth)
                            {
                                table_entry_tt pcutEntry = {
                                    .depth = nextDepth,
                                    .hashCode = board->hashCode,
                                    .nodeType = NODE_TYPE_CUT,
                                    .evaluation = beta,
                                    .age = board->halfMoveCount
                                };
                                transposition_table_set(context->tt, pcutEntry, ply);
                            }
                            destroy_move_iterator(iter);
                            return beta;
                        }
                    }   
                    destroy_move_iterator(iter);
                }
            }
        }
    }
    
    //TT reductions
    if(depth >= tt_reduction_depth && pvNode && (!hit || old_tt_entry.depth + tt_reduction_min_depth_offset < depth))
        depth -= 1;

    moveIterator* iter = create_move_iterator(board, GET_ALL_MOVES, pvMove, tt_move, (ply == 0) ? context->searchedMoves : NULL, context->killerMoves[ply], context->historyTable);
    int validMovesVisited = 0;
    int bestScore = -INT32_MAX;
    if(iter)
    {
        move_c* currentMove;

        while((currentMove = iterate_next_move(iter)) != NULL)
        {
            int currentPiece = findPieceOnSquare(board, currentMove->startSquare);

            if(moveFromStruct(board, *currentMove)) continue;

            move_d detailedMove = board->history[board->historyIndex - 1];
            updateMoveAccumulator(board, detailedMove, 0, context->accumulator, context->refreshTable);

            int next_depth = depth - 1;
            int isCapture = findPieceOnSquare(board, currentMove->endSquare) != EMPTY_PIECE || (ISPAWN(currentPiece) && board->enPassantSquare == currentMove->endSquare);
            int isQuietMove = (!IS_IN_CHECK_ANY(board->flags) && !isCapture && !currentMove->promoteTo);
            if(IS_IN_CHECK_ANY(board->flags)) next_depth++;
            
            if(!pvNode && isQuietMove && !context->improving[ply]) next_depth -= reductionTable[depth][validMovesVisited];

            if(validMovesVisited == 0) score = -principalVariationSearch(context, -beta, -alpha, next_depth, ply + 1, &childPV);
            else
            {
                //Scout
                score = -principalVariationSearch(context, -alpha - 1, -alpha, next_depth, ply + 1, &childPV);

                //LMR Re-search
                if(score > alpha && next_depth < depth - 1)
                {
                    next_depth = depth - 1;
                    score = -principalVariationSearch(context, -alpha - 1, -alpha, next_depth, ply + 1, &childPV);
                }
                
                //PVS Re-search
                if(score > alpha && pvNode) score = -principalVariationSearch(context, -beta, -alpha, next_depth, ply + 1, &childPV);
            }
            
            unmove(board);
            updateMoveAccumulator(board, detailedMove, 1, context->accumulator, context->refreshTable);
            
            if(score >= beta)
            {
                new_tt_entry.nodeType = NODE_TYPE_CUT;
                new_tt_entry.evaluation = score;
                transposition_table_set(context->tt, new_tt_entry, ply);
                destroy_move_iterator(iter);

                if(isQuietMove)
                {
                    //Killer heuristic
                    context->killerMoves[ply][1] = context->killerMoves[ply][0];
                    context->killerMoves[ply][0] = *currentMove;
                
                    //History heuristic
                    int bonus = depth * depth;
                    int16_t* dest = &context->historyTable[board->turn][PIECE(currentPiece) / 2][currentMove->endSquare];
                    *dest = _min(*dest + bonus, MAX_HISTORY_SCORE);
                    int16_t* straightArr = (int16_t*) context->historyTable;
                    for(int i = 0; i < searchedQuietCount; i++)
                        straightArr[searchedQuietIndices[i]] = _max(straightArr[searchedQuietIndices[i]] - bonus, -MAX_HISTORY_SCORE);
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
                    new_tt_entry.bestMove = currentMove->raw;
                    transposition_table_set(context->tt, new_tt_entry, ply);
                    alpha = score;
                    
                    myPV->line[0] = *currentMove;
                    memcpy(&myPV->line[1], childPV.line, childPV.length * sizeof(move_c));
                    myPV->length = childPV.length + 1;
                }
            }
            validMovesVisited++;

            if(isQuietMove)
                searchedQuietIndices[searchedQuietCount++] = (board->turn * 384) + ((PIECE(currentPiece) / 2) * 64) + currentMove->endSquare;
        }

        if(bestScore < alpha)
        {
            //Don't save a bestmove since we couldn't find one that fits in window.
            new_tt_entry.nodeType = NODE_TYPE_ALL;
            new_tt_entry.evaluation = bestScore;
            new_tt_entry.bestMove = 0;
            transposition_table_set(context->tt, new_tt_entry, ply);
        }
        destroy_move_iterator(iter);
    }
    
    if(!iter || validMovesVisited == 0)
    {
        int victor = getMateResult(board);
        if(victor == VICTOR_WHITE)
            return (board->turn == WHITE) ? (SCORE_WIN - ply) : -(SCORE_WIN - ply);
        else if(victor == VICTOR_BLACK)
            return (board->turn == BLACK) ? (SCORE_WIN - ply) : -(SCORE_WIN - ply);
        else
            return 0;
    }
    return bestScore;
}

void printResultingMoves(move_c bestMove, move_c ponderMove, int isBookMove)
{
    if(suppressUCIMessages) return;
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
    
    if(currentDepth < min_aspiration_depth)
    {
        loadInputAccumulator(board, acc, WHITE);
        loadInputAccumulator(board, acc, BLACK);
        context->score = principalVariationSearch(context, -INT32_MAX, INT32_MAX, currentDepth, 0, &context->pv);
        context->completedDepth = currentDepth;
    }
    else
    {

        int aspiration_margin = initial_aspiration_margin;
        int alpha = context->score - aspiration_margin;
        int beta = context->score + aspiration_margin;
        while(1)
        {
            if(context->isPonder == 0 && clock() > *endTime) break;

            loadInputAccumulator(board, acc, WHITE);
            loadInputAccumulator(board, acc, BLACK);
            int score = principalVariationSearch(context, alpha, beta, currentDepth, 0, &context->pv);

            if(score <= alpha)
            {
                beta = (alpha + beta) / 2;

                aspiration_margin*=aspiration_margin_mult_factor;
                alpha = score - aspiration_margin;
            }
            else if(score >= beta)
            {
                alpha = (alpha + beta) / 2;

                aspiration_margin*=aspiration_margin_mult_factor;
                beta = score + aspiration_margin;
            }
            else
            {
                context->score = score;
                context->completedDepth = currentDepth;
                break;
            }

            if(aspiration_margin > maximum_aspiration_margin)
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

void findBestThread(searchThreadContext* mainThread, searchThreadContext* helperThreads, move_c* bestMove, move_c* ponderMove)
{
    searchThreadContext* best = mainThread;
    int bestDepth = best->completedDepth;
    int bestScore = best->score;
    int totalNodes = mainThread->countedNodes;
    if(helperThreads)
    {
        for(int i = 0; i < threadCount - 1; i++)
        {
            totalNodes += helperThreads[i].countedNodes;

            if(!IS_VALID_MOVE(helperThreads[i].pv.line[0])) continue;
            int curDepth = helperThreads[i].completedDepth;
            int curScore = helperThreads[i].score;

            if(curDepth >= bestDepth || curScore > MIN_MATE_SCORE) 
            {
                best = &helperThreads[i];
                bestDepth = best->completedDepth;
                bestScore = best->score;
            }

        }
    }
    
    *bestMove = best->pv.line[0];
    *ponderMove = best->pv.line[1]; //Invalid & isPonder checks come later.

    if(suppressUCIMessages) return;

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

    printf(" pv");
    for(int i = 0; i < best->pv.length; i++)
    {
        move_c m = best->pv.line[i];
        if(!IS_VALID_MOVE(m)) break;
        char startSq[3] = {'\0'};
        char endSq[3] = {'\0'};
        getSquareName(m.startSquare, startSq);
        getSquareName(m.endSquare, endSq);
        printf(" %s%s", startSq, endSq);
        if(m.promoteTo == QUEEN) printf("q");
        else if(m.promoteTo == ROOK) printf("r");
        else if(m.promoteTo == BISHOP) printf("b");
        else if(m.promoteTo == KNIGHT) printf("n");
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
    
    move_c bestMove, ponderMove = (move_c){0};
    int helperThreadCount = threadCount - 1;

    bitboard* board = context->board;
    board->historyIndex = 0;
    context->countedNodes = 0;
    context->seldepth = 0;
    context->completedDepth = 0;

    //Book moves
    if(!context->isPonder)
    {
        context->pv.line[0] = getBookMove(board);
        if(IS_VALID_MOVE(context->pv.line[0])) { printResultingMoves(context->pv.line[0], (move_c){0}, 1); isCalculating = 0; return 0; }
    }

    //Sygyzy move recommendations
    filterSygyzyMoves(board, context->searchedMoves);

    THREADTYPE *helperThreads = NULL;
    searchThreadContext* helperThreadContext = NULL;
    bitboard* threadBoards = NULL;
    accumulator* threadAccumulators = NULL;
    accumulatorRefreshTable** threadRefreshTables = NULL;
    clock_t terminateFlag = LONG_MAX;

    if(helperThreadCount > 0)
    {
        helperThreads = calloc(helperThreadCount, sizeof(THREADTYPE));
        helperThreadContext = calloc(helperThreadCount, sizeof(searchThreadContext));
        threadBoards = calloc(helperThreadCount, sizeof(bitboard));
        threadAccumulators = calloc(helperThreadCount, sizeof(accumulator));
        threadRefreshTables = calloc(helperThreadCount, sizeof(accumulatorRefreshTable));
        
        for(int i = 0; i < helperThreadCount; i++) 
        {
            helperThreadContext[i].board = &threadBoards[i];
            memcpy(helperThreadContext[i].board, board, sizeof(bitboard));
            helperThreadContext[i].endTime = &terminateFlag;
            helperThreadContext[i].maxDepth = context->maxDepth;
            helperThreadContext[i].deepeningSkip = 1 + (i%3);

            helperThreadContext[i].accumulator = &threadAccumulators[i];
            threadRefreshTables[i] = createRefreshTable();
            helperThreadContext[i].refreshTable = threadRefreshTables[i];

            helperThreadContext[i].tt = context->tt;

            memcpy(helperThreadContext[i].searchedMoves, context->searchedMoves, 16*sizeof(move_c));
            THREAD_START(helperThreads[i], helperThreadFunction, &helperThreadContext[i]);
        }
    }
    
    for(int currentDepth = 1; currentDepth <= maxDepth; currentDepth++)
    {
        aspiration_window(context, currentDepth);

        if(currentDepth > 1 && (clock() > *endTime || context->countedNodes > context->maxNodes)) break;
        
        bestMove = context->pv.line[0];
        ponderMove = context->pv.line[1];

        
        if(!suppressUCIMessages)
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
                move_c m = context->pv.line[i];
                char startSq[3] = {'\0'};
                char endSq[3] = {'\0'};
                getSquareName(m.startSquare, startSq);
                getSquareName(m.endSquare, endSq);
                printf(" %s%s", startSq, endSq);
                if(m.promoteTo == QUEEN) printf("q");
                else if(m.promoteTo == ROOK) printf("r");
                else if(m.promoteTo == BISHOP) printf("b");
                else if(m.promoteTo == KNIGHT) printf("n");
            }
            printf("\n");
            fflush(stdout);
        }
        
        if(abs(context->score) >= MIN_MATE_SCORE) break;
    }
    if(helperThreadCount > 0)
    {   
        terminateFlag = 0;
        for(int i = 0; i < helperThreadCount; i++) 
        {
            THREAD_WAIT(helperThreads[i]);
        }
        findBestThread(context, helperThreadContext, &bestMove, &ponderMove);
        
        free(threadBoards);
        free(helperThreads);
        free(helperThreadContext);

        free(threadAccumulators);
        for(int i = 0; i < helperThreadCount; i++)
        {
            destroyRefreshTable(threadRefreshTables[i]);
        }
        free(threadRefreshTables);
    }


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