#include "engine.h"
#include "../board/moves.h"
#include "../board/bitboard.h"
#include "../debug.h"
#include "book.h"
#include "../pyrrhic/tbprobe.h"
#include <string.h>
#include <float.h>
#include <math.h>


int useHelperThreads = 1;

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

double evaluate(bitboard* board, accumulator* acc)
{
    assert(board);
    assert(acc);

    return (double) forwardPropagate(board->turn, acc, __builtin_popcountll(board->pieces_all));
}

double quiesce(bitboard* board, double alpha, double beta, int depth, accumulator* acc, accumulatorRefreshTable* refreshTable)
{

    if(board->victor)
    {
        if(board->victor == VICTOR_WHITE)
        {
            if(board->turn == WHITE) return INT8_MAX + 2;
            else return INT8_MIN - 1;
        }
        else if(board->victor == VICTOR_BLACK)
        {
            if(board->turn == BLACK) return INT8_MAX + 2;
            else return -INT8_MIN - 1;
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

            DEBUG("Unhandled draw condition.");
            return 0.0;
        }
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
    else best = evaluate(board, acc);

    if(depth == 0 || best >= beta) return best;
    if(best > alpha) alpha = best;

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
                updateMoveAccumulator(board, currentMove, 0, acc, refreshTable);
                double score = -quiesce(board, -beta, -alpha, depth - 1, acc, refreshTable);

                unmove(board);
                updateMoveAccumulator(board, currentMove, 1, acc, refreshTable);

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

double principalVariationSearch(bitboard* board, double alpha, double beta, int maxDepth, int depth, move* pv, int pvIndex, clock_t* timeLimit, accumulator* acc, accumulatorRefreshTable* refreshTable)
{
    if(board->victor)
    {
        if(board->victor == VICTOR_WHITE)
        {
            if(board->turn == WHITE) return INT8_MAX + 2;
            else return INT8_MIN - 1;
        }
        else if(board->victor == VICTOR_BLACK)
        {
            if(board->turn == BLACK) return INT8_MAX + 2;
            else return -INT8_MIN - 1;
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

            DEBUG("Unhandled draw condition.");
            return 0.0;
        }
    }

    //Transposition table
    table_entry_tt* old_tt_entry = NULL;
    if((old_tt_entry = transposition_table_get(board, transpositionTable)) != NULL && 
        old_tt_entry->evaluationDepth >= depth &&
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
        .evaluationDepth = depth,
        .hashCode = board->hashCode
    };

    if(depth == 0 || (timeLimit && clock() > *timeLimit) || board->victor) 
    {
        new_tt_entry.nodeType = NODE_TYPE_PV;
        new_tt_entry.evaluation = quiesce(board, alpha, beta, 5, acc, refreshTable);
        //We aren't saving a bestmnove.
        transposition_table_set(transpositionTable, new_tt_entry);
        return new_tt_entry.evaluation;
    }
    
    double score = 0;

    move moveList[256];
    int entryCount = generateMoveList(moveList, board, 0);
    if(entryCount)
    {
        move* pvMove = NULL;
        if(pv) pvMove = &pv[maxDepth - depth];

        int moveScores[128] = {0};
        for(int i = 0; i < entryCount; i++)
        {
            move m = moveList[i];
 
            int pieceScore = PIECE(m.piece);
            if(pieceScore == KING) pieceScore = 1;

            if(pvMove && m.startSquare == pvMove->startSquare && m.endSquare == pvMove->endSquare) moveScores[i] = INT32_MAX;
            else if(m.capturedPiece != EMPTY_PIECE) moveScores[i] = 10 + (PIECE(m.capturedPiece)) - pieceScore;
            else moveScores[i] = pieceScore;
        }

        double bestScore = -DBL_MAX;
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
                updateMoveAccumulator(board, moveList[moveIndex], 0, acc, refreshTable);
                if(i == 0)
                {
                    score = -principalVariationSearch(board, -beta, -alpha, maxDepth, depth - 1, pv, pvIndex + depth, timeLimit, acc, refreshTable);
                }
                else
                {
                    score = -principalVariationSearch(board, -alpha - 1, -alpha, maxDepth, depth - 1, pv, pvIndex + depth, timeLimit, acc, refreshTable);
                    //Re-search PV node
                    if (score > alpha && beta - alpha > 1) score = -principalVariationSearch(board, -beta, -alpha, maxDepth, depth - 1, pv, pvIndex + depth, timeLimit, acc, refreshTable);
                }
                
                unmove(board);
                updateMoveAccumulator(board, moveList[moveIndex], 1, acc, refreshTable);
                
                if(score >= beta)
                {
                    new_tt_entry.nodeType = NODE_TYPE_CUT;
                    new_tt_entry.evaluation = score;
                    new_tt_entry.bestMove = moveList[moveIndex];
                    transposition_table_set(transpositionTable, new_tt_entry);
                    return beta;
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
                        if(pv) 
                        {
                            pv[pvIndex] = moveList[moveIndex];
                            copyNMoves(&pv[pvIndex + 1], &pv[pvIndex + depth], depth - 1);
                        }
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

typedef struct searchThreadData {
    double alpha;
    double beta;
    int depth;
    bitboard* board;
    clock_t* endTime;
    move* pvTable;
    double* score;
    accumulator* accumulator;
    accumulatorRefreshTable* accumulatorTable;
} searchThreadData;

DWORD WINAPI helperThreadFunction(LPVOID lpParam)
{
    searchThreadData* data = (searchThreadData*)lpParam;
                
    updateAccumulatorFromTable(data->board, data->accumulator, data->accumulatorTable);

    //Always fully evaluate at depth 1:
    *data->score = principalVariationSearch(data->board, -DBL_MAX, DBL_MAX, 1, 1, data->pvTable, 0, NULL, data->accumulator, data->accumulatorTable);

    while(*data->endTime != 0)
    {
        updateAccumulatorFromTable(data->board, data->accumulator, data->accumulatorTable);
        *data->score = principalVariationSearch(data->board, data->alpha, data->beta, data->depth, data->depth, data->pvTable, 0, data->endTime, data->accumulator, data->accumulatorTable);
    }
    return 0;
}

move calculateBestMove(bitboard* board, int maxDepth, int maxTimeSeconds)
{
    board->historyIndex = 0;

    if(entries) //Book moves
    {
        move* bookMove = NULL;
        if((bookMove = getBookMove(board))) return *bookMove;
        else unloadBook();
    }
    else if(__builtin_popcountll(board->pieces_all) <= 5 && !(board->flags&0x30)) //3-5man sygyzy endgame with no castling rights.
    {
        uint32_t ep = board->enPassantSquare;
        if(ep == -1) ep = 0;

        bool turn = PYRRHIC_WHITE;
        if(ISBLACK(board->turn)) turn = PYRRHIC_BLACK;

        int hasRepeated = 0;
        
        int index = board->repetitionIndex - 1;
        uint64_t checkedVal = board->repetitionHashCodes[index];
        for(index = index - 4; index >= 0; index -= 2)
        {
            if(checkedVal == board->repetitionHashCodes[index])
            {
                hasRepeated = 1;
                break;
            }
        }

        struct TbRootMoves moveResults = {0};

        int result = tb_probe_root_dtz(board->pieces_side[WHITE], board->pieces_side[BLACK], 
                                        board->pieces[BLACK_KING]|board->pieces[WHITE_KING], board->pieces[BLACK_QUEEN]|board->pieces[WHITE_QUEEN], 
                                        board->pieces[BLACK_ROOK]|board->pieces[WHITE_ROOK], board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP],
                                        board->pieces[BLACK_KNIGHT]|board->pieces[WHITE_KNIGHT], board->pieces[BLACK_PAWN]|board->pieces[WHITE_PAWN],
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
    }

    move* principalVariation = calloc(maxDepth, sizeof(move));
    move* tempPVTable = NULL;

    clock_t endTime = clock() + CLOCKS_PER_SEC*maxTimeSeconds;

    HANDLE helperThreads[HELPER_THREAD_COUNT] = {0};
    DWORD helperThreadID[HELPER_THREAD_COUNT] = {0};
    searchThreadData params[HELPER_THREAD_COUNT] = {0};

    clock_t terminateFlags[HELPER_THREAD_COUNT] = {LONG_MAX}; //These are passed as end times for the thread searches. Cancel threads by setting values to 0.
    double threadScores[HELPER_THREAD_COUNT] = {0};
    bitboard *threadBoards = calloc(HELPER_THREAD_COUNT, sizeof(bitboard));
    for(int i = 0; i < HELPER_THREAD_COUNT; i++) 
    {
        params[i].pvTable = NULL;
        params[i].board = &threadBoards[i];
        params[i].score = &threadScores[i];
        params[i].endTime = &terminateFlags[i];
    }

    accumulator* acc = playerAccumulator;
    if(!acc) acc = playerAccumulator = calloc(1, sizeof(accumulator));

    accumulatorRefreshTable* accumulatorTable = playingRefreshTable;
    if(!accumulatorTable) accumulatorTable = playingRefreshTable = createPlayingRefreshTable();
    
    accumulator* threadAccumulators = calloc(HELPER_THREAD_COUNT, sizeof(accumulator));
    accumulatorRefreshTable** threadRefreshTables = calloc(HELPER_THREAD_COUNT, sizeof(accumulatorRefreshTable*));
    for(int i = 0; i < HELPER_THREAD_COUNT; i++) 
    {
        threadRefreshTables[i] = createPlayingRefreshTable();
        params[i].accumulatorTable = threadRefreshTables[i];
        params[i].accumulator = &threadAccumulators[i];
    }

    //Always fully evaluate at depth 1:
    updateAccumulatorFromTable(board, acc, accumulatorTable);
    double aspiration_expectedValue = principalVariationSearch(board, -DBL_MAX, DBL_MAX, 1, 1, principalVariation, 0, NULL, acc, accumulatorTable);

    for(int currentDepth = 2; currentDepth <= maxDepth; currentDepth++)
    {
        if(clock() > endTime) break;
        tempPVTable = calloc(0.5*currentDepth*(currentDepth + 1), sizeof(move));
        copyNMoves(tempPVTable, principalVariation, currentDepth);

        //Initialize helper threadss
        if(useHelperThreads)
        {
            for(int i = 0; i < HELPER_THREAD_COUNT; i++)
            {
                memcpy(params[i].board, board, sizeof(bitboard));
                params[i].depth = currentDepth + (i%2);
                *params[i].endTime = LONG_MAX;
                params[i].pvTable = calloc(0.5*currentDepth*(currentDepth + 1), sizeof(move));
                copyNMoves(params[i].pvTable, principalVariation, currentDepth);

                helperThreads[i] = CreateThread(NULL, 0, helperThreadFunction, &params[i], 0, &helperThreadID[i]);
            }
        }
        


        //Aspiration Window Loop
        //Avoid for lower depths.
        if(currentDepth < 5)
        {
            updateAccumulatorFromTable(board, acc, accumulatorTable);
            aspiration_expectedValue = principalVariationSearch(board, -DBL_MAX, DBL_MAX, currentDepth, currentDepth, tempPVTable, 0, &endTime, acc, accumulatorTable);
        }
        else
        {

            double aspiration_margin = INITIAL_ASPIRATION_MARGIN;
            double alpha = aspiration_expectedValue - aspiration_margin;
            double beta = aspiration_expectedValue + aspiration_margin;
            while(1)
            {
                if(clock() > endTime) break;

                updateAccumulatorFromTable(board, acc, accumulatorTable);
                double score = principalVariationSearch(board, alpha, beta, currentDepth, currentDepth, tempPVTable, 0, &endTime, acc, accumulatorTable);

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

        if(tempPVTable[0].startSquare != tempPVTable[0].endSquare) copyNMoves(principalVariation, tempPVTable, currentDepth);

        free(tempPVTable);
        tempPVTable = NULL;
        for(int i = 0; i < HELPER_THREAD_COUNT; i++) 
        {
            free(params[i].pvTable);
            params[i].pvTable = NULL;
        }
    }

    free(threadBoards);
    if(tempPVTable) free(tempPVTable);

    free(threadAccumulators);
    for(int i = 0; i < HELPER_THREAD_COUNT; i++) destroyRefreshTable(threadRefreshTables[i]);
    free(threadRefreshTables);

    move bestMove = principalVariation[0];

    if(bestMove.startSquare == bestMove.endSquare && bestMove.startSquare == 0)
    {
        if(printDebugMessages) 
        {
            char FEN[100] = { '\0' };
            export_fen_from_board(board, FEN);
            DEBUG("Engine returned empty move on %s", FEN);
            for(int i = 0; i < maxDepth; i++) printf("\tpv[%d] = %d->%d\n", i, principalVariation[i].startSquare, principalVariation[i].endSquare);
        }

        //This isn't recoverable.
        exit(EXIT_FAILURE);
    }

    free(principalVariation);
    principalVariation = NULL;

    return bestMove;
}