#include "analyze/search.h"
#include "board/moves.h"
#include "board/bitboard.h"
#include "debug.h"
#include "analyze/book.h"
#include "analyze/syzygy.h"
#include "pyrrhic/tbprobe.h"
#include "analyze/nnue/neuralnet.h"
#include <string.h>
#include <math.h>

int threadCount = 1;
int enablePonder = 0;
int isCalculating = 0;
int suppressUCIMessages = 0;

//Global flag. Each context's abort flag will 
//point to this with the exception of data generation 
//since that involves multiple games from one process.
uint8_t abortFlag = 0;

uint8_t isPonder = 0;

const int min_aspiration_depth = 5;
const int reverse_futility_pruning_depth = 4;
const int futility_pruning_depth = 2;
const int quiet_futility_pruning_depth = 8;
const int nullmove_pruning_depth = 5;
const int probcut_depth = 8;
const int probcut_depth_reduction = 4;
const int tt_reduction_depth = 7;
const int tt_reduction_min_depth_offset = 3;
const int lmr_depth = 4;
const int singular_extension_depth = 7;

int initial_aspiration_margin = 38;
int maximum_aspiration_margin = 150;
float aspiration_margin_mult_factor = 2.0f;

int delta_pruning_offset = 52;

int futility_margin = 403;
int futility_margin_improving = 368;

int reverse_futility_margin = 190;
int reverse_futility_margin_improving = 122;

int probcut_offset = 400;
int probcut_offset_improving = 250;

int historyBonusScale = 290;
int historyBonusOffset = 137;
int historyPenaltyScale = 392;
int historyPenaltyOffset = 131;

int lowHistoryVal = -123;

//a * log(depth) * log(moveCount) / b
float lmr_a = 0.649f;
float lmr_b = 3.363f;

//a * depth * depth + b
float lmp_a = 1.849f;
float lmp_b = 5.0f;
float lmp_improving_a = 1.434f;
float lmp_improving_b = 4.0f;

int lmrTable[MAX_PLY][MAX_MOVES] = {0};
int lmpTable[2][MAX_PLY] = {0};

int searchInit = 0;
void initSearchTables()
{
    if(searchInit) 
        return;
    searchInit = 1;

    for(int depth = lmr_depth; depth < MAX_PLY; depth++)
    {
        int count = 2.0f + 0.5f * depth * depth;
        for(int moveCount = 0; moveCount < MAX_MOVES; moveCount++)
        {
            if(moveCount >= count)
                lmrTable[depth][moveCount] = (int)( lmr_a + log(depth) * log(moveCount) / lmr_b );
        }
    }

    for(int depth = 0; depth < MAX_PLY; depth++)
    {
        lmpTable[0][depth] = lmp_a * depth * depth + lmp_b;
        lmpTable[1][depth] = lmp_improving_a * depth * depth + lmp_improving_b;
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

int evaluate(searchThreadContext* context)
{
    return (useNNUE) ? forwardPropagate(context->board, context->accumulator) : hce_eval(context->board);
}

int quiescentSearch(searchThreadContext* context, int alpha, int beta, int ply)
{
    context->countedNodes++;
    
    bitboard* board = context->board;

    if(*context->abortFlag || (!isPonder && (((context->countedNodes & 1023) == 0 && clock() > context->hardEndTime) || context->countedNodes >= (context->hardMaxNodes / threadCount))))
    {
        *context->abortFlag = 1;
        return 0;
    }
    if(isDraw(board))
        return (ply & 3) - 1;
    if(ply >= MAX_PLY - 1)
        return evaluate(context);

    int lowestBound = alpha;
    move_c* tt_move = NULL;
    move_c temp; //Copy in from TT instead of saving a ptr to a volatile TT slot.

    int best;
    uint8_t tt_hit;
    table_entry_tt entry = transposition_table_get(board, context->tt, &tt_hit, ply);
    if(tt_hit)
    {
        if(entry.nodeType == NODE_BOUND_EXACT)
            return entry.evaluation;
        else if(entry.nodeType == NODE_BOUND_UPPER)
        {
            if(entry.evaluation <= alpha)
                return entry.evaluation;
            beta = _min(beta, entry.evaluation);
        }
        else if(entry.nodeType == NODE_BOUND_LOWER)
        {
            if(entry.evaluation >= beta)
                return entry.evaluation;
            alpha = _max(alpha, entry.evaluation);
        }
            
        if(alpha >= beta)
            return alpha;

        temp.raw = entry.bestMove;
        tt_move = &temp;
        best = clamp(entry.evaluation, alpha, beta);
    }
    else 
    {
        best = evaluate(context);
    
        table_entry_tt shallowEntry = {
            .depth = 0,
            .hashCode = board->hashCode,
            .nodeType = NODE_BOUND_UNKNOWN,
            .evaluation = best,
            .age = board->halfMoveCount
        };
        transposition_table_set(context->tt, shallowEntry, ply);
    }

    if(best >= beta) return best;
    alpha = _max(alpha, best);

    //Delta pruning
    int largestDelta = delta_pruning_offset;
    int opposingColor = FLIP_COLOR(board->turn);

    if(board->pieces[QUEEN | opposingColor])
        largestDelta += evaluatePhasedScore(board, hce_params.genericPieceValues[QUEEN / 2]);
    else if(board->pieces[ROOK | opposingColor])
        largestDelta += evaluatePhasedScore(board, hce_params.genericPieceValues[ROOK / 2]);
    else if(board->pieces[BISHOP | opposingColor])
        largestDelta += evaluatePhasedScore(board, hce_params.genericPieceValues[BISHOP / 2]);
    else if(board->pieces[KNIGHT | opposingColor])
        largestDelta += evaluatePhasedScore(board, hce_params.genericPieceValues[KNIGHT / 2]);
    else if(board->pieces[PAWN | opposingColor])
        largestDelta += evaluatePhasedScore(board, hce_params.genericPieceValues[PAWN / 2]);

    if(largestDelta && largestDelta + best < alpha) return best;

    moveIterator* iter = create_move_iterator(board, GET_CAPTURES_AND_CHECKS, 
                                                NULL, NULL, 
                                                NULL, tt_move, 
                                                NULL, NULL,
                                                NULL, NULL);
    move_c bestMove = {0};
    if(iter)
    {
        move_c* currentMove;
        while((currentMove = iterate_next_move(iter)) != NULL)
        {
            //SEE pruning
            if(iter->moveScores[iter->visitedCount - 1] < -CAPTURE_SCORE - 50)
                continue;

            if(moveFromStruct(board, *currentMove)) 
                continue;

            move_d lastMove = board->history[board->historyIndex - 1];
            if(useNNUE)
                updateMoveAccumulator(board, lastMove, 0, context->accumulator, context->refreshTable);

            int score = -quiescentSearch(context, -beta, -alpha, ply + 1);

            unmove(board);

            if(useNNUE)
                updateMoveAccumulator(board, lastMove, 1, context->accumulator, context->refreshTable);

            

            if(score > best)
            {
                best = score;
                bestMove = *currentMove;

                if(score > alpha) 
                    alpha = score;

                if(alpha >= beta)
                    break;
            }
        }
        destroy_move_iterator(iter);
    }
    
    table_entry_tt shallowEntry = {
        .depth = 0,
        .hashCode = board->hashCode,
        .nodeType = (best >= beta) ? NODE_BOUND_LOWER : ( (best > lowestBound) ? NODE_BOUND_EXACT : NODE_BOUND_UPPER),
        .evaluation = best,
        .age = board->halfMoveCount,
        .bestMove = bestMove.raw
    };
    transposition_table_set(context->tt, shallowEntry, ply);
    return best;
}

int principalVariationSearch(searchThreadContext* context, int alpha, int beta, int depth, int ply, PVar* myPV, int cutNode)
{
    assert(context);
    context->countedNodes++;
    bitboard* board = context->board;
    assert(board);

    myPV->length = 0;
    PVar childPV;

    int pvNode = (beta != alpha + 1);
    int inCheck = IS_IN_CHECK_ANY(board->flags);
    
    move_c* counterMove = NULL;
    if(board->repetitionIndex >= 1)
    {
        move_c compact;
        compact.raw = board->history[board->historyIndex - 1].compactMove;

        int from = compact.startSquare;
        int to = compact.endSquare;
        counterMove = &context->countermove[from][to];
    }

    move_c* followUpMove = NULL;
    if(board->repetitionIndex >= 2)
    {
        move_c compact;
        compact.raw = board->history[board->historyIndex - 2].compactMove;

        int from = compact.startSquare;
        int to = compact.endSquare;
        followUpMove = &context->followUpMove[from][to];
    }

    move_c* pvMove = (ply == 0) ? &context->pv.line[0] : NULL;
    move_c* tt_move = NULL;
    move_c temp; //Copy in from TT instead of saving a ptr to a volatile TT slot.

    move_c bestMove = {0};

    int searchedQuietIndices[MAX_MOVES] = {0};
    int searchedQuietCount = 0;
    int shouldSkipQuiets = 0;
    
    int lowestBound = alpha;

    if(*context->abortFlag || (ply >= 1 && !isPonder && (((context->countedNodes & 1023) == 0 && clock() > context->hardEndTime) || context->countedNodes >= (context->hardMaxNodes / threadCount))))
    {
        *context->abortFlag = 1;
        return 0;
    }

    if(pvNode)
        context->seldepth = _max(context->seldepth, ply);
    
    if(isDraw(board)) return (ply & 3) - 1;

    //Mate distance pruning for non-root nodes.
    if(ply != 0)
    {
        alpha = _max(alpha, -SCORE_WIN + ply);
        beta = _min(beta, SCORE_WIN - ply - 1);
        if(alpha >= beta) return alpha;
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
        if(old_tt_entry.depth >= depth && (!pvNode || depth == 0) && (cutNode || old_tt_entry.evaluation <= alpha)) 
        {
            if(old_tt_entry.nodeType == NODE_BOUND_EXACT)
                return old_tt_entry.evaluation;
            else if(old_tt_entry.nodeType == NODE_BOUND_UPPER)
            {
                if(old_tt_entry.evaluation <= alpha)
                    return old_tt_entry.evaluation;
                beta = _min(beta, old_tt_entry.evaluation);
            }
            else if(old_tt_entry.nodeType == NODE_BOUND_LOWER)
            {
                if(old_tt_entry.evaluation >= beta)
                    return old_tt_entry.evaluation;
                alpha = _max(alpha, old_tt_entry.evaluation);
            }
                
            if(alpha >= beta)
                return alpha;
        }
        
        temp.raw = old_tt_entry.bestMove; 
        tt_move = &temp;
        score = old_tt_entry.evaluation;
    }

    if(ply >= MAX_PLY - 1)
        return evaluate(context);
    if(depth <= 0)
        return quiescentSearch(context, alpha, beta, ply);
    
    //Syzygy
    if(!pvNode && depth >= syzygyProbeDepth)
    {
        int result = getSyzygyResult(context->board);
        if(result != -1)
        {
            if(result > 0)
                result -= ply;
            if(result < 0)
                result += ply;

            new_tt_entry.nodeType = NODE_BOUND_EXACT;
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
            score = evaluate(context);
            table_entry_tt shallowEntry = {
                .depth = 0,
                .hashCode = board->hashCode,
                .nodeType = NODE_BOUND_UNKNOWN,
                .evaluation = score,
                .age = board->halfMoveCount
            };
            transposition_table_set(context->tt, shallowEntry, ply);
        }
    }

    //Improving
    int staticScore = score;
    context->evalHistory[ply] = staticScore;
    if(inCheck) context->improving[ply] = 0;
    else context->improving[ply] = (ply >= 2) ? (score > context->evalHistory[ply - 2]) : 1;

    if(ply < MAX_PLY - 1)
        context->killerMoves[ply+1][0].raw = context->killerMoves[ply+1][1].raw = 0;

    if(!pvNode && !inCheck && abs(score) < MIN_MATE_SCORE)
    {
        //Futility pruning
        if(depth <= futility_pruning_depth)
        {
            int boostedVal;
            if(context->improving[ply]) boostedVal = score + futility_margin_improving;
            else boostedVal = score + futility_margin;

            if(boostedVal <= alpha) return boostedVal;
        }

        //Reverse Futility Pruning
        if(depth <= reverse_futility_pruning_depth)
        {
            int reducedVal;
            if(context->improving[ply]) reducedVal = score - reverse_futility_margin_improving * depth;
            else reducedVal = score - reverse_futility_margin * depth;

            if(reducedVal >= beta) return reducedVal;
        }

        //Null move pruning
        if(score >= beta && depth >= nullmove_pruning_depth && cutNode &&
            !(board->historyIndex > 0 && board->history[board->historyIndex - 1].raw == 0) &&
            (board->pieces_all ^ (board->pieces[WHITE_KING] | board->pieces[BLACK_KING] | board->pieces[WHITE_PAWN] | board->pieces[BLACK_PAWN])))
        {
            int r = 3 + depth / 6;
            applyNullMove(board);
            int nullScore = -principalVariationSearch(context, -beta, -beta + 1, depth - r, ply + 1, &childPV, !cutNode);
            revertNullMove(board);
            if(nullScore >= beta)
                return beta;
        }

        //Probcut
        if(depth >= probcut_depth)
        {
            int nextDepth = depth - probcut_depth_reduction;
            int pBeta = (context->improving[ply]) ? beta + probcut_offset_improving: beta + probcut_offset;

            if(score >= pBeta && pBeta < MIN_MATE_SCORE && (!hit || old_tt_entry.depth < nextDepth))
            {
                int probCutScore = INT32_MIN;
                moveIterator* iter = create_move_iterator(board, GET_WINNING_CAPTURES,
                                                            NULL, NULL,
                                                            pvMove, tt_move, 
                                                            context->historyTable, context->killerMoves[ply], 
                                                            counterMove, followUpMove);
                if(iter)
                {
                    move_c* currentMove;
                    while((currentMove = iterate_next_move(iter)) != NULL)
                    {
                        if(moveFromStruct(board, *currentMove)) continue;
                        
                        move_d lastMove = board->history[board->historyIndex - 1];
                        if(useNNUE)
                            updateMoveAccumulator(board, lastMove, 0, context->accumulator, context->refreshTable);

                        probCutScore = -quiescentSearch(context, -pBeta - 1, -pBeta, ply + 1);
                        if(probCutScore >= pBeta)
                            probCutScore = -principalVariationSearch(context, -pBeta - 1, -pBeta, nextDepth, ply + 1, &childPV, !cutNode);

                        unmove(board);

                        if(useNNUE)
                            updateMoveAccumulator(board, lastMove, 1, context->accumulator, context->refreshTable);


                        if(probCutScore >= pBeta)
                        {
                            if(!hit || old_tt_entry.depth < nextDepth)
                            {
                                table_entry_tt pcutEntry = {
                                    .depth = nextDepth,
                                    .hashCode = board->hashCode,
                                    .nodeType = NODE_BOUND_LOWER,
                                    .evaluation = beta,
                                    .age = board->halfMoveCount,
                                    .bestMove = currentMove->raw
                                };
                                transposition_table_set(context->tt, pcutEntry, ply);
                            }
                            destroy_move_iterator(iter);
                            return probCutScore;
                        }
                    }   
                    destroy_move_iterator(iter);
                }
            }
        }
    }
    
    //TT reductions
    if(!context->excludedMove.raw && depth >= tt_reduction_depth && (pvNode || cutNode) && (!hit || old_tt_entry.depth + tt_reduction_min_depth_offset < depth))
        depth -= 1;

    moveIterator* iter = create_move_iterator(board, GET_ALL_MOVES,
                                                (ply == 0) ? context->searchedMoves : NULL, &context->excludedMove,
                                                pvMove, tt_move, 
                                                context->historyTable, context->killerMoves[ply], 
                                                counterMove, followUpMove);
    int validMovesVisited = 0;
    int bestScore = -INT32_MAX;
    if(iter)
    {
        move_c* currentMove;

        while((currentMove = iterate_next_move(iter)) != NULL)
        {
            int currentPiece = findPieceOnSquare(board, currentMove->startSquare);
            
            int next_depth = depth - 1;
            int isCapture = findPieceOnSquare(board, currentMove->endSquare) != EMPTY_PIECE || 
                            (ISPAWN(currentPiece) && board->enPassantSquare == currentMove->endSquare);

            //Singular Extension
            if(hit && depth >= singular_extension_depth && currentMove->raw == tt_move->raw && old_tt_entry.depth >= depth - 3 && 
               old_tt_entry.nodeType == NODE_BOUND_LOWER && !context->excludedMove.raw)
            {
                int sBeta = old_tt_entry.evaluation  - 3 * depth;
                int sDepth = depth / 2 + 1;

                context->excludedMove = *tt_move;
                int singularScore = principalVariationSearch(context, sBeta - 1, sBeta, sDepth, ply + 1, &childPV, cutNode);
                context->excludedMove.raw = 0;

                if(singularScore < sBeta)
                    next_depth++;
                //Multicut pruning, but we just take the strong singular and assume that there's going to be more.
                else if(singularScore >= beta)
                    return singularScore;
                //Negative Extension
                else if(cutNode && old_tt_entry.evaluation >= beta)
                    next_depth--;
            }

            if(moveFromStruct(board, *currentMove)) continue;
            
            move_d lastMove = board->history[board->historyIndex - 1];
            if(useNNUE)
                updateMoveAccumulator(board, lastMove, 0, context->accumulator, context->refreshTable);
            
            int isQuietMove = (!IS_IN_CHECK_ANY(board->flags) && !isCapture && !currentMove->promoteTo);

            //Quiet move pruning.
            if(!shouldSkipQuiets && isQuietMove && ply > 0 && abs(bestScore) < MIN_MATE_SCORE)
            {
                //Late move pruning
                if(!pvNode && validMovesVisited > lmpTable[context->improving[ply]][ply])
                {
                    shouldSkipQuiets = 1;
                    unmove(board);
                    if(useNNUE)
                        updateMoveAccumulator(board, lastMove, 1, context->accumulator, context->refreshTable);
                    continue;
                }
            }
            if(isQuietMove && shouldSkipQuiets)
            {
                unmove(board);
                if(useNNUE)
                    updateMoveAccumulator(board, lastMove, 1, context->accumulator, context->refreshTable);
                continue;
            }

            //Check extensions
            if(IS_IN_CHECK_ANY(board->flags)) 
                next_depth++;
            
            //Late move reduction
            if(!pvNode && isQuietMove && !context->improving[ply]) 
                next_depth -= lmrTable[depth][validMovesVisited];

            //SEE reduction
            if(!pvNode && iter->moveScores[iter->visitedCount - 1] < -CAPTURE_SCORE - 50)
                next_depth--;
            
            //History Reduction
            if(!pvNode && isQuietMove && iter->moveScores[iter->visitedCount] < lowHistoryVal)
                next_depth--;

            if(pvNode && validMovesVisited == 0) score = -principalVariationSearch(context, -beta, -alpha, next_depth, ply + 1, &childPV, 0);
            else
            {
                //Scout
                score = -principalVariationSearch(context, -alpha - 1, -alpha, next_depth, ply + 1, &childPV, 1);

                //LMR Re-search
                if(score > alpha && next_depth < depth - 1)
                {
                    next_depth = depth - 1;
                    score = -principalVariationSearch(context, -alpha - 1, -alpha, next_depth, ply + 1, &childPV, !cutNode);
                }
                
                //PVS Re-search
                if(score > alpha && pvNode) score = -principalVariationSearch(context, -beta, -alpha, next_depth, ply + 1, &childPV, 0);
            }
            
            unmove(board);
            if(useNNUE)
                updateMoveAccumulator(board, lastMove, 1, context->accumulator, context->refreshTable);
            
            if(score >= beta)
            {
                new_tt_entry.nodeType = NODE_BOUND_LOWER;
                new_tt_entry.evaluation = score;
                new_tt_entry.bestMove = currentMove->raw;
                transposition_table_set(context->tt, new_tt_entry, ply);

                if(isQuietMove)
                {
                    //Killer heuristic
                    context->killerMoves[ply][1] = context->killerMoves[ply][0];
                    context->killerMoves[ply][0] = *currentMove;
                
                    //History heuristic
                    int bonus = historyBonusScale * depth + historyBonusOffset;
                    int penalty = historyPenaltyScale * depth + historyPenaltyOffset;
                    int16_t* dest = &context->historyTable[board->turn][PIECE(currentPiece) / 2][currentMove->endSquare];
                    *dest = _min(*dest + bonus, MAX_HISTORY_SCORE);

                    int16_t* straightArr = (int16_t*) context->historyTable;
                    for(int i = 0; i < searchedQuietCount; i++)
                    straightArr[searchedQuietIndices[i]] = _max(straightArr[searchedQuietIndices[i]] - penalty, -MAX_HISTORY_SCORE);

                    //Countermove heuristic
                    if(board->repetitionIndex >= 2)
                    {
                        move_c compact;
                        compact.raw = board->history[board->historyIndex - 2].compactMove;

                        int from = compact.startSquare;
                        int to = compact.endSquare;
                        context->countermove[from][to] = *currentMove;
                    }
                    
                    //Follow-up Move heuristic
                    if(board->repetitionIndex >= 3)
                    {
                        move_c compact;
                        compact.raw = board->history[board->historyIndex - 3].compactMove;

                        int from = compact.startSquare;
                        int to = compact.endSquare;
                        context->followUpMove[from][to] = *currentMove;
                    }
                }

                destroy_move_iterator(iter);

                return score;
            }
            else if(score > bestScore)
            {
                bestScore = score;
                bestMove = *currentMove;

                if(score > alpha)
                {
                    new_tt_entry.nodeType = NODE_BOUND_EXACT;
                    new_tt_entry.evaluation = score;
                    new_tt_entry.bestMove = currentMove->raw;
                    transposition_table_set(context->tt, new_tt_entry, ply);
                    alpha = score;
                    
                    //Save PV
                    myPV->line[0] = *currentMove;
                    memcpy(&myPV->line[1], childPV.line, childPV.length * sizeof(move_c));
                    myPV->length = childPV.length + 1;

                }
            }
            validMovesVisited++;

            if(isQuietMove)
                searchedQuietIndices[searchedQuietCount++] = (board->turn * 384) + ((PIECE(currentPiece) / 2) * 64) + currentMove->endSquare;
        }
        destroy_move_iterator(iter);
        
        new_tt_entry.nodeType = (bestScore >= beta) ? NODE_BOUND_LOWER : ( (bestScore > lowestBound) ? NODE_BOUND_EXACT : NODE_BOUND_UPPER);
        new_tt_entry.evaluation = bestScore;
        new_tt_entry.bestMove = bestMove.raw;
        transposition_table_set(context->tt, new_tt_entry, ply);
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
    //context->pv is used to save last stable pv line.
    //It is used for reporting and testing pv moves.
    //Don't corrupt it.

    PVar tempPV = {0};
    int score;

    if(currentDepth < min_aspiration_depth)
    {
        score = principalVariationSearch(context, -INT32_MAX, INT32_MAX, currentDepth, 0, &tempPV, 0);
        context->completedDepth = currentDepth;
    }
    else
    {

        int aspiration_margin = initial_aspiration_margin;
        int alpha = context->score - aspiration_margin;
        int beta = context->score + aspiration_margin;
        while(1)
        {
            score = principalVariationSearch(context, alpha, beta, currentDepth, 0, &tempPV, 0);

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
                break;

            if(aspiration_margin > maximum_aspiration_margin)
            {
                alpha = -INT32_MAX;
                beta = INT32_MAX;
            }
        }
    }
    
    //If the pv is stable (not half-done from abortion), save it.
    if(clock() <= context->hardEndTime && context->countedNodes < (context->hardMaxNodes / threadCount) && *context->abortFlag == 0)
    {
        context->score = score;
        context->completedDepth = currentDepth;
        memcpy(&context->pv, &tempPV, sizeof(PVar));
    }
}

THREAD_RETURN helperThreadFunction(THREAD_PARAM param)
{
    searchThreadContext* context = (searchThreadContext*)param;
    context->seldepth = 0;
    context->completedDepth = 0;

    move_c bestMove = context->pv.line[0];
    
    if(useNNUE)
        updateAccumulatorFromTable(context->board, context->accumulator, context->refreshTable);
        
    int lastScore = context->score;

    for(int currentDepth = 1; currentDepth <= context->maxDepth; currentDepth+=context->deepeningSkip)
    {
        if(!isPonder && currentDepth > 1 && (*context->abortFlag || clock() > context->softEndTime || context->countedNodes > context->softMaxNodes / threadCount)) 
            break;

        aspiration_window(context, currentDepth);
        
        if(currentDepth > 10)
        {
            if(bestMove.raw == context->pv.line[0].raw || abs(context->score - lastScore) < 15)
                context->softEndTime -= 0.1 * (context->softEndTime - clock());
            else
                context->softEndTime = context->hardEndTime;
        }
        
        bestMove = context->pv.line[0];
        lastScore = context->score;
        
        if(abs(context->score) > MIN_MATE_SCORE)
            context->softEndTime -= 0.5 * (context->softEndTime - clock());
    }

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
    *context->abortFlag = 0;
    memset(context->historyTable, 0, sizeof(context->historyTable));
    
    int maxDepth = context->maxDepth;
    
    move_c bestMove = (move_c){0}; 
    move_c ponderMove = (move_c){0};
    int helperThreadCount = threadCount - 1;

    bitboard* board = context->board;
    board->historyIndex = 0;
    context->countedNodes = 0;
    context->seldepth = 0;
    context->completedDepth = 0;

    //Book moves
    if(!isPonder)
    {
        context->pv.line[0] = getBookMove(board);
        if(IS_VALID_MOVE(context->pv.line[0])) { printResultingMoves(context->pv.line[0], (move_c){0}, 1); isCalculating = 0; return 0; }
        else LEAVE_BOOK_OPENING(context->board->flags);
    }

    //Syzygy move recommendations
    filterSyzygyMoves(board, context->searchedMoves);

    THREADTYPE *helperThreads = NULL;
    searchThreadContext* helperThreadContext = NULL;
    bitboard* threadBoards = NULL;

    if(helperThreadCount > 0)
    {
        helperThreads = calloc(helperThreadCount, sizeof(THREADTYPE));
        helperThreadContext = calloc(helperThreadCount, sizeof(searchThreadContext));
        threadBoards = calloc(helperThreadCount, sizeof(bitboard));

        for(int i = 0; i < helperThreadCount; i++) 
        {
            helperThreadContext[i].abortFlag = context->abortFlag;
            helperThreadContext[i].board = &threadBoards[i];
            memcpy(helperThreadContext[i].board, board, sizeof(bitboard));
            helperThreadContext[i].startTime = context->startTime;
            helperThreadContext[i].hardEndTime = context->hardEndTime;
            helperThreadContext[i].softEndTime = context->softEndTime;
            helperThreadContext[i].maxDepth = context->maxDepth;
            helperThreadContext[i].hardMaxNodes = context->hardMaxNodes;
            helperThreadContext[i].softMaxNodes = context->softMaxNodes;
            helperThreadContext[i].deepeningSkip = 1 + (i%3);
            helperThreadContext[i].tt = context->tt;

            if(useNNUE)
            {
                helperThreadContext[i].accumulator = calloc(1, sizeof(accumulator));
                helperThreadContext[i].refreshTable = createRefreshTable();
            }

            memcpy(helperThreadContext[i].searchedMoves, context->searchedMoves, 16*sizeof(move_c));
            THREAD_START(helperThreads[i], helperThreadFunction, &helperThreadContext[i]);
        }
    }
    
    if(useNNUE)
        updateAccumulatorFromTable(context->board, context->accumulator, context->refreshTable);
    int lastScore = 0;
    for(int currentDepth = 1; currentDepth <= maxDepth; currentDepth++)
    {
        aspiration_window(context, currentDepth);

        if(!isPonder && currentDepth > 1 && (*context->abortFlag || clock() > context->softEndTime || context->countedNodes >= (context->softMaxNodes / threadCount))) break;
        
        if(currentDepth > 10)
        {
            if(bestMove.raw == context->pv.line[0].raw || abs(context->score - lastScore) < 15)
                context->softEndTime -= 0.1 * (context->softEndTime - clock());
            else
                context->softEndTime = context->hardEndTime;
        }
        
        bestMove = context->pv.line[0];
        ponderMove = context->pv.line[1];
        lastScore = context->score;
        
        if(!suppressUCIMessages)
        {
            int totalNodes = context->countedNodes;
            totalNodes *= threadCount; //Unreliable, basically a best-guess until end of search.
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
        
        if(abs(context->score) > MIN_MATE_SCORE)
            context->softEndTime -= 0.5 * (context->softEndTime - clock());
    }

    if(helperThreadCount > 0)
    {   
        *context->abortFlag = 1;
        for(int i = 0; i < helperThreadCount; i++) 
        {
            THREAD_WAIT(helperThreads[i]);
            if(useNNUE)
            {
                free(helperThreadContext[i].accumulator);
                destroyRefreshTable(helperThreadContext[i].refreshTable);
            }
        }
        findBestThread(context, helperThreadContext, &bestMove, &ponderMove);
        
        free(threadBoards);
        free(helperThreads);
        free(helperThreadContext);
    }


    if(!IS_VALID_MOVE(bestMove) && bestMove.startSquare == 0)
    {
        char FEN[100] = { '\0' };
        export_fen_from_board(board, FEN);
        DEBUG_ERROR("Engine returned empty move on %s", FEN);
    }

    printResultingMoves(bestMove, ponderMove, 0);
    isCalculating = 0;

    memset(context->searchedMoves, 0, sizeof(context->searchedMoves));

    return 0;
}