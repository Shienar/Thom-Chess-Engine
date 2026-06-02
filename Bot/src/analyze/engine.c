#include "engine.h"
#include "../board/moves.h"
#include "../board/bitboard.h"
#include "../debug.h"
#include "book.h"
#include "../pyrrhic/tbprobe.h"
#include <string.h>
#include <float.h>
#include <math.h>

int threadCount = 8;
int enablePonder = 0;
int isCalculating = 0;

int perft(bitboard* board, int depth, int maxDepth, int verbose)
{
    if(!depth) return 1;
    int nodes = 0;

    move moveList[256];
    int count = generateMoveList(moveList, board, 0);
    for(int index = 0; index < count; index++)
    {
        if(!moveFromStruct(board, moveList[index]))
        {
            int branchNodes = perft(board, depth - 1, maxDepth, 0);
            nodes += branchNodes;
            if (verbose) 
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

float evaluateEndstate(bitboard* board, int ply)
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
        //Draws are less desirable in the early/middlegame and more desirable in the lategame.
        int scale;
        if(board->halfMoveCount < MIDDLEGAME_START_HALFMOVES) scale = CONTEMPT_FACTOR_SCALE_EARLYGAME;
        else if(board->halfMoveCount < MIDDLEGAME_END_HALFMOVES) scale = CONTEMPT_FACTOR_SCALE_MIDDLEGAME;
        else scale = CONTEMPT_FACTOR_SCALE_ENDGAME;

        if(board->victor == VICTOR_DRAW_STALEMATE_WHITE || board->victor == VICTOR_DRAW_STALEMATE_BLACK) return scale*CONTEMPT_FACTOR_STALEMATE;
        if(board->victor == VICTOR_DRAW_THREEFOLD) return scale*CONTEMPT_FACTOR_THREEFOLD;
        if(board->victor == VICTOR_DRAW_FIFTY_MOVE_RULE) return scale*CONTEMPT_FACTOR_FIFTYMOVERULE;
        if(board->victor == VICTOR_DRAW_INSUFFICIENT_MATERIAL) return scale*CONTEMPT_FACTOR_INSUFFICIENT_MATERIAL;

        DEBUG_ERROR("Unhandled draw condition.");
        return 0.0;
    }
}

move getSygyzyMove(bitboard* board)
{
    //3-n man sygyzy endgame with no castling rights.
    if(__builtin_popcountll(board->pieces_all) > sygyzyProbeLimit || (board->flags&0x30)) return (move){0}; 

    uint32_t ep = board->enPassantSquare;
    if(ep == -1) ep = 0;

    bool turn = PYRRHIC_WHITE;
    if(ISBLACK(board->turn)) turn = PYRRHIC_BLACK;

    bool hasRepeated = false;
    
    int index = board->repetitionIndex - 1;
    uint64_t checkedVal = board->repetitionHashCodes[index];
    for(index = index - 4; index >= 0; index -= 2)
    {
        if(checkedVal == board->repetitionHashCodes[index])
        {
            hasRepeated = true;
            break;
        }
    }

    struct TbRootMoves moveResults = {0};

    int result = tb_probe_root_dtz(board->pieces_side[WHITE], board->pieces_side[BLACK], 
                                    board->pieces[BLACK_KING]|board->pieces[WHITE_KING], board->pieces[BLACK_QUEEN]|board->pieces[WHITE_QUEEN], 
                                    board->pieces[BLACK_ROOK]|board->pieces[WHITE_ROOK], board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP],
                                    board->pieces[BLACK_KNIGHT]|board->pieces[WHITE_KNIGHT], board->pieces[BLACK_PAWN]|board->pieces[WHITE_PAWN],
                                    (unsigned) board->movesSinceLastChange/2, ep, turn, hasRepeated, &moveResults);
    
    if(result)
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
        
        move bestMove = {0};
        bestMove.endSquare = PYRRHIC_MOVE_TO(moveResults.moves[bestIndex].move);
        bestMove.startSquare = PYRRHIC_MOVE_FROM(moveResults.moves[bestIndex].move);
        bestMove.piece = findPieceOnSquare(board, bestMove.startSquare);

        if(PYRRHIC_MOVE_IS_QPROMO(moveResults.moves[bestIndex].move)) bestMove.promoteTo = QUEEN;
        else if(PYRRHIC_MOVE_IS_RPROMO(moveResults.moves[bestIndex].move)) bestMove.promoteTo = ROOK;
        else if(PYRRHIC_MOVE_IS_BPROMO(moveResults.moves[bestIndex].move)) bestMove.promoteTo = BISHOP;
        else if(PYRRHIC_MOVE_IS_NPROMO(moveResults.moves[bestIndex].move)) bestMove.promoteTo = KNIGHT;

        bestMove.capturedPiece = findPieceOnSquare(board, bestMove.endSquare);
        if(bestMove.capturedPiece) bestMove.capturedPieceSquare = bestMove.endSquare;

        return bestMove;
    }

    DEBUG_ERROR("Failed to probe sygyzy.");
    return (move){0}; 
}

//Return -1.0f on error. Valid results are SCORE_WIN, -SCORE_WIN, 0
float getSygyzyResult(bitboard* board)
{
    //3-n man sygyzy endgame with no castling rights.
    if(__builtin_popcountll(board->pieces_all) > sygyzyProbeLimit || (board->flags&0x30)) return 0;

    uint32_t ep = board->enPassantSquare;
    if(ep == -1) ep = 0;

    bool turn = PYRRHIC_WHITE;
    if(ISBLACK(board->turn)) turn = PYRRHIC_BLACK;

    int result = tb_probe_wdl(board->pieces_side[WHITE], board->pieces_side[BLACK], 
                                    board->pieces[BLACK_KING]|board->pieces[WHITE_KING], board->pieces[BLACK_QUEEN]|board->pieces[WHITE_QUEEN], 
                                    board->pieces[BLACK_ROOK]|board->pieces[WHITE_ROOK], board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP],
                                    board->pieces[BLACK_KNIGHT]|board->pieces[WHITE_KNIGHT], board->pieces[BLACK_PAWN]|board->pieces[WHITE_PAWN],
                                    ep, turn);
    switch(result)
    {
        case TB_LOSS:
            return -SCORE_WIN;
        case TB_WIN:
            return SCORE_WIN;
        case TB_BLESSED_LOSS:
        case TB_CURSED_WIN:
        case TB_DRAW:
            return 0.0f;
        default:
            return -1.0f;
    }
}

float quiesce(THREAD_PARAM param, float alpha, float beta, int ply)
{
    searchThreadContext* context = (searchThreadContext*)param;
    context->countedNodes++;

    bitboard* board = context->board;

    if(board->victor)
    {
        evaluateEndstate(board, ply);
    }

    table_entry_tt* entry = transposition_table_get(board, transpositionTable);
    if(entry)
    {
        if (entry->nodeType == NODE_TYPE_PV) return entry->evaluation;
        if (entry->nodeType == NODE_TYPE_ALL && entry->evaluation <= alpha) return alpha;
        if (entry->nodeType == NODE_TYPE_CUT && entry->evaluation >= beta) return beta;
    }
     
    float best = forwardPropagate(board->turn, context->accumulator, __builtin_popcountll(board->pieces_all));

    if(best >= beta) return best;
    if(best > alpha) alpha = best;

    if(ply >= MAX_PLY) return best;

    move moveList[256];
    int entryCount = generateMoveList(moveList, board, 1);
    if(entryCount)
    {
        int moveScores[256] = {0};
        for(int i = 0; i < entryCount; i++)
        {
            move m = moveList[i];

            int pieceScore = PIECE(m.piece);
            if(pieceScore == KING) pieceScore = 1;
            
            if(m.capturedPiece != EMPTY_PIECE) moveScores[i] = 10 + (PIECE(m.capturedPiece)) - pieceScore;
            else moveScores[i] = pieceScore;
        }

        for(int i = 0; i < entryCount; i++)
        {
            int moveIndex = 0;
            int maxScoreRemaining = INT32_MIN;
            for(int j = 0; j < entryCount; j++)
            {
                if(moveScores[j] > maxScoreRemaining)
                {
                    moveIndex = j;
                    maxScoreRemaining = moveScores[j];
                }
            }
            moveScores[moveIndex] = INT32_MIN;
            
            move currentMove = moveList[moveIndex];
            if(!moveFromStruct(board, currentMove))
            {
                updateMoveAccumulator(board, currentMove, 0, context->accumulator, context->accumulatorTable);
                float score = -quiesce(param, -beta, -alpha, ply + 1);

                unmove(board);
                updateMoveAccumulator(board, currentMove, 1, context->accumulator, context->accumulatorTable);

                if(score >= beta)
                {
                    return score;
                }
                if(score > best) best = score;
                if(score > alpha) alpha = score;
            }
        }
    }
    return best;
}

void copyNMoves(move* dest, move* source, int count)  {while(count--) *dest++ = *source++;}

float principalVariationSearch(THREAD_PARAM param, float alpha, float beta, int maxDepth, int depth, int pvIndex)
{
    searchThreadContext* context = (searchThreadContext*)param;
    context->countedNodes++;
    bitboard* board = context->board;
    int ply = maxDepth - depth;

    //Sygyzy
    if(depth >= sygyzyProbeDepth)
    {
        int result = getSygyzyResult(context->board);
        if(result != -1.0f) 
        {
            //We aren't saving a pv move, but 
            //this isn't the root node.
            return result;
        }
    }

    //Transposition table
    table_entry_tt* old_tt_entry = NULL;
    if((old_tt_entry = transposition_table_get(board, transpositionTable)) != NULL && 
        old_tt_entry->ply >= ply &&
            (old_tt_entry->nodeType == NODE_TYPE_PV ||
            (old_tt_entry->nodeType == NODE_TYPE_ALL && old_tt_entry->evaluation <= alpha) ||
            (old_tt_entry->nodeType == NODE_TYPE_CUT && old_tt_entry->evaluation >= beta)))
    {
        //Do not return if this is a pv node. 
        //We need to populate the pv table.
        //Pv nodes are searched with a full window
        //Only do a transposition table cutoff if searched with a null window.
        if(alpha == beta - 1) return old_tt_entry->evaluation;
    }

    table_entry_tt new_tt_entry = {
        .age = clock(),
        .ply = ply,
        .hashCode = board->hashCode
    };

    if(depth == 0 || (context->endTime && clock() > *context->endTime) || board->victor) 
    {
        new_tt_entry.evaluation = quiesce(param, alpha, beta, ply);

        if(new_tt_entry.evaluation <= alpha) new_tt_entry.nodeType = NODE_TYPE_ALL;
        if(new_tt_entry.evaluation >= beta) new_tt_entry.nodeType = NODE_TYPE_CUT;
        else new_tt_entry.nodeType = NODE_TYPE_PV;
       
        //We aren't saving a bestmnove.

        transposition_table_set(transpositionTable, new_tt_entry);
        return new_tt_entry.evaluation;
    }
    
    float score = 0;

    int entryCount = 0;
    move moveList[256];
    if(ply == 0)
    {
        for(int i = 0; i < 16; i++)
        {
            if(IS_VALID_MOVE(context->searchedMoves[i])) entryCount++;
            else break;
        }
        if(entryCount) memcpy(moveList, context->searchedMoves, entryCount * sizeof(move));
        else entryCount = generateMoveList(moveList, board, 0);
    }
    else entryCount = generateMoveList(moveList, board, 0);
    if(entryCount)
    {
        move* pvMove = &context->pvTable[ply];

        int moveScores[256] = {0};
        for(int i = 0; i < entryCount; i++)
        {
            move m = moveList[i];
 
            int pieceScore = PIECE(m.piece);
            if(pieceScore == KING) pieceScore = 1;

            if(pvMove && m.startSquare == pvMove->startSquare && m.endSquare == pvMove->endSquare) moveScores[i] = INT32_MAX;
            else if(m.capturedPiece != EMPTY_PIECE) moveScores[i] = 10 + (PIECE(m.capturedPiece)) - pieceScore;
            else moveScores[i] = pieceScore;
        }

        float bestScore = -FLT_MAX;
        for(int i = 0; i < entryCount; i++)
        {
            int moveIndex = 0;
            int maxScoreRemaining = INT32_MIN;
            for(int j = 0; j < entryCount; j++)
            {
                if(moveScores[j] > maxScoreRemaining)
                {
                    moveIndex = j;
                    maxScoreRemaining = moveScores[j];
                }
            }
            moveScores[moveIndex] = INT32_MIN;

            if(!moveFromStruct(board, moveList[moveIndex]))
            {
                updateMoveAccumulator(board, moveList[moveIndex], 0, context->accumulator, context->accumulatorTable);
                if(i == 0)
                {
                    score = -principalVariationSearch(param, -beta, -alpha, maxDepth, depth - 1, pvIndex + depth);
                }
                else
                {
                    score = -principalVariationSearch(param, -beta, -alpha, maxDepth, depth - 1, pvIndex + depth);
                    //Re-search PV node
                    if (score > alpha && beta - alpha > 1) score = -principalVariationSearch(param, -beta, -alpha, maxDepth, depth - 1, pvIndex + depth);
                }
                
                unmove(board);
                updateMoveAccumulator(board, moveList[moveIndex], 1, context->accumulator, context->accumulatorTable);
                
                if(score >= beta)
                {
                    new_tt_entry.nodeType = NODE_TYPE_CUT;
                    new_tt_entry.evaluation = score;
                    new_tt_entry.bestMove = moveList[moveIndex];
                    transposition_table_set(transpositionTable, new_tt_entry);
                    return score;
                }
                else if(score > bestScore)
                {
                    bestScore = score;

                    if(score > alpha)
                    {
                        new_tt_entry.nodeType = NODE_TYPE_PV;
                        new_tt_entry.evaluation = score;
                        new_tt_entry.bestMove = moveList[moveIndex];
                        transposition_table_set(transpositionTable, new_tt_entry);
                        alpha = score;
                        context->pvTable[pvIndex] = moveList[moveIndex];
                        copyNMoves(&context->pvTable[pvIndex + 1], &context->pvTable[pvIndex + depth], depth - 1);
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
    }
    return alpha;
}

void printResultingMoves(move bestMove, move ponderMove, int isPonder)
{
    char startSq[3];
    char endSq[3];
    int startSquare = bestMove.startSquare;
    int endSquare = bestMove.endSquare;

    getSquareName(startSquare, startSq);
    getSquareName(endSquare, endSq);

    printf("bestmove %s%s", startSq, endSq);

    if(enablePonder && IS_VALID_MOVE(ponderMove))
    {
        startSquare = ponderMove.startSquare;
        endSquare = ponderMove.endSquare;

        getSquareName(startSquare, startSq);
        getSquareName(endSquare, endSq);

        printf(" ponder %s%s", startSq, endSq);
    }

    printf("\n");
    fflush(stdout);
}

void aspiration_window(THREAD_PARAM param)
{
    searchThreadContext* context = (searchThreadContext*)param;
    bitboard* board = context->board;
    int currentDepth = context->depth;
    clock_t* endTime = context->endTime;
    accumulator* acc = context->accumulator;
    accumulatorRefreshTable* accumulatorTable = context->accumulatorTable;
    
    if(context->depth < MIN_ASPIRATION_DEPTH)
    {
        updateAccumulatorFromTable(board, context->accumulator, context->accumulatorTable);
        *context->score = principalVariationSearch(param, -FLT_MAX, FLT_MAX, currentDepth, currentDepth, 0);
    }
    else
    {

        float aspiration_margin = INITIAL_ASPIRATION_MARGIN;
        float alpha = *context->score - aspiration_margin;
        float beta = *context->score + aspiration_margin;
        while(1)
        {
            if(clock() > *endTime) break;

            updateAccumulatorFromTable(board, acc, accumulatorTable);
            float score = principalVariationSearch(param, alpha, beta, currentDepth, currentDepth, 0);

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
                *context->score = score;
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
    updateAccumulatorFromTable(context->board, context->accumulator, context->accumulatorTable);

    //Always fully evaluate at depth 1:
    clock_t temp = *context->endTime;
    *context->endTime = LONG_MAX;
    *context->score = principalVariationSearch(param, -FLT_MAX, FLT_MAX, 1, 1, 0);
    *context->endTime = temp;

    while(*context->endTime != 0) aspiration_window(param);

    return 0;
}

THREAD_RETURN calculateBestMove(THREAD_PARAM param)
{
    searchThreadContext* context = (searchThreadContext*)param;
    
    int maxDepth = context->depth;
    clock_t *endTime = context->endTime;
    
    move bestMove, ponderMove = (move){0};
    int helperThreadCount = threadCount - 1;

    bitboard* board = context->board;
    board->historyIndex = 0;

    //Book moves
    if(entries)
    {
        move bookMove = getBookMove(board);
        if(IS_VALID_MOVE(bookMove)) { printResultingMoves(bookMove, (move){0}, context->isPonder); isCalculating = 0; return 0; }
        else unloadBook();
    }
    
    //Sygyzy
    bestMove = getSygyzyMove(board);
    if(IS_VALID_MOVE(bestMove)){ printResultingMoves(bestMove, (move){0}, context->isPonder); isCalculating = 0; return 0; }

    accumulator* acc = playerAccumulator;
    accumulatorRefreshTable* accumulatorTable = playingRefreshTable;

    THREADTYPE *helperThreads = NULL;
    searchThreadContext* helperThreadContext = NULL;
    clock_t* terminateFlags = NULL;
    float* threadScores = NULL;
    bitboard* threadBoards = NULL;
    accumulator* threadAccumulators = NULL;
    accumulatorRefreshTable** threadRefreshTables = NULL;
    if(helperThreadCount)
    {
        helperThreads = calloc(threadCount - 1, sizeof(THREADTYPE));
        helperThreadContext = calloc(threadCount - 1, sizeof(searchThreadContext));
        terminateFlags = calloc(threadCount - 1, sizeof(clock_t));
        threadScores = calloc(threadCount - 1, sizeof(float));
        threadBoards = calloc(threadCount - 1, sizeof(bitboard));
        threadAccumulators = calloc(threadCount - 1, sizeof(accumulator));
        threadRefreshTables = calloc(threadCount - 1, sizeof(accumulatorRefreshTable*));
        
        for(int i = 0; i < threadCount - 1; i++) 
        {
            helperThreadContext[i].board = &threadBoards[i];
            helperThreadContext[i].score = &threadScores[i];
            helperThreadContext[i].endTime = &terminateFlags[i];

            threadRefreshTables[i] = createRefreshTable();
            helperThreadContext[i].accumulatorTable = threadRefreshTables[i];
            helperThreadContext[i].accumulator = &threadAccumulators[i];

            memcpy(helperThreadContext[i].searchedMoves, context->searchedMoves, 16*sizeof(move));
        }
    }

    context->countedNodes = 0;

    //Always fully evaluate at depth 1:
    updateAccumulatorFromTable(board, acc, accumulatorTable);
    clock_t temp = *context->endTime;
    *context->endTime = LONG_MAX;
    *context->score = principalVariationSearch(param, -FLT_MAX, FLT_MAX, 1, 1, 0);
    *context->endTime = temp;

    for(int currentDepth = 2; currentDepth <= maxDepth; currentDepth++)
    {
        if(clock() > *endTime || context->countedNodes > context->maxNodes) break;

        //Initialize helper threads
        if(helperThreadCount)
        {
            for(int i = 0; i < threadCount - 1; i++)
            {
                memcpy(helperThreadContext[i].board, board, sizeof(bitboard));
                helperThreadContext[i].depth = _min(currentDepth + (i%3), MAX_PLY);

                THREAD_START(helperThreads[i], helperThreadFunction, &helperThreadContext[i]);
            }
        }
        
        //Aspiration Window Loop
        aspiration_window(param);
        
        if(helperThreadCount)
        {
            //Stop helper threads
            memset(terminateFlags, 0, (threadCount - 1) * sizeof(clock_t));
            for(int i = 0; i < threadCount - 1; i++) 
            {
                THREAD_WAIT(helperThreads[i]);
                context->countedNodes += helperThreadContext[i].countedNodes;
                helperThreadContext[i].countedNodes = 0;
            }
        
            //Move voting
            float worstScore = *context->score;
            for(int i = 0; i < helperThreadCount; i++)  worstScore = _min(*helperThreadContext[i].score, worstScore);

            int mainThreadVote = (*context->score - worstScore + 1) * currentDepth;
            int totalVoteWeights = mainThreadVote;
            int *votes = calloc(helperThreadCount, sizeof(int));
            for(int i = 0; i < helperThreadCount; i++) 
            {
                votes[i] = (*helperThreadContext[i].score - worstScore + 1) * helperThreadContext[i].depth;
                totalVoteWeights+= votes[i];
            }

            totalVoteWeights = rand()%totalVoteWeights;
            if(totalVoteWeights >= mainThreadVote)
            {
                int8_t threadIndex;
                for(threadIndex = 0; threadIndex < helperThreadCount - 1; threadIndex++)
                {
                    if(totalVoteWeights < mainThreadVote) break;
                    else mainThreadVote += votes[threadIndex];
                }

                memcpy(context->pvTable, helperThreadContext[threadIndex].pvTable, 0.5 * currentDepth * (currentDepth + 1) * sizeof(move));
            }
            free(votes);
        }

        bestMove = context->pvTable[0];
        ponderMove = context->pvTable[1];
    }

    free(threadBoards);
    free(helperThreads);
    free(helperThreadContext);
    free(terminateFlags);
    free(threadScores);

    free(threadAccumulators);
    for(int i = 0; i < helperThreadCount; i++) destroyRefreshTable(threadRefreshTables[i]);
    free(threadRefreshTables);

    if(!IS_VALID_MOVE(bestMove) && bestMove.startSquare == 0)
    {
        char FEN[100] = { '\0' };
        export_fen_from_board(board, FEN);
        DEBUG_ERROR("Engine returned empty move on %s", FEN);
        for(int i = 0; i < maxDepth; i++) DEBUG_INFO("\tpv[%d] = %d->%d", i, context->pvTable[i].startSquare, context->pvTable[i].endSquare);
    }

    printResultingMoves(bestMove, ponderMove, context->isPonder);
    isCalculating = 0;
    return 0;
}