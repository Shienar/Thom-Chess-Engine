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

volatile uint8_t isPonder = 0;

const int min_aspiration_depth = 5;
const int reverse_futility_pruning_depth = 4;
const int futility_pruning_depth = 8;
const int nullmove_pruning_depth = 3;
const int probcut_depth = 8;
const int probcut_depth_reduction = 4;
const int tt_reduction_depth = 7;
const int tt_reduction_min_depth_offset = 3;
const int lmr_depth = 4;
const int singular_extension_depth = 7;

//ax^2 + b
int razoring_a = 122;
int razoring_b = 63;

int initial_aspiration_margin = 38;
int maximum_aspiration_margin = 150;
float aspiration_margin_mult_factor = 2.0f;

int delta_pruning_offset = 52;
int delta_pruning_nnue_offset = 998;

int futility_margin = 70;
int futility_depth_margin = 51;

int reverse_futility_margin = 185;
int reverse_futility_margin_improving = 122;

int probcut_offset = 400;
int probcut_offset_improving = 250;

int historyBonusScale = 290;    
int historyBonusOffset = 137;
int historyPenaltyScale = 392;
int historyPenaltyOffset = 131;

int lowHistoryVal = -123;

int stable_eval_margin = 39;

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

    move moveList[MAX_MOVES];
    int count = generateMoveList(moveList, board, 0);
    bitboard newBoard;
    for(int index = 0; index < count; index++)
    {
        if(moveFromStruct(board, &newBoard, moveList[index], NULL)) continue;
        
        int branchNodes = perft(&newBoard, depth - 1, 0);
        nodes += branchNodes;
        if(verbose) 
        {
            char fromSquare[3] = {'\0'};
            char toSquare[3] = {'\0'};
            getSquareName(moveList[index].startSquare, fromSquare);
            getSquareName(moveList[index].endSquare, toSquare);
            printf("Move %s%s: nodes %d\n", fromSquare, toSquare, branchNodes);
        }
    }
    
    return nodes;
}

int evaluate(searchThreadContext* context, int ply)
{
    RECORD_SEARCH(context->evaluations++;);
    return (useNNUE) ? forwardPropagate(&context->boardStack[ply], &context->accumulatorStack[ply]) : hce_eval(&context->boardStack[ply]);
}

int quiescentSearch(searchThreadContext* context, int alpha, int beta, int ply, int pvNode)
{
    context->countedNodes++;
    RECORD_SEARCH(context->qs_nodes++;);
    bitboard* curBoard = &context->boardStack[ply];

    if(*context->abortFlag || (!isPonder && (((context->countedNodes & 1023) == 0 && clock() > context->hardEndTime) || context->countedNodes >= (context->hardMaxNodes / threadCount))))
    {
        *context->abortFlag = 1;
        return 0;
    }

    if(isDraw(curBoard, &context->repetitions) == VICTOR_DRAW)
        return (ply & 3) - 1;
    if(ply >= MAX_PLY - 1)
        return evaluate(context, ply);
        
    bitboard* nextBoard = &context->boardStack[ply + 1];
    move* pvMove = (curBoard->hashCode == context->pv.hashCodes[ply]) ? &context->pv.line[ply] : NULL;

    if(pvNode)
        context->seldepth = _max(context->seldepth, ply);
    
    int lowestBound = alpha;
    move* tt_move = NULL;
    move temp; //Copy in from TT instead of saving a ptr to a volatile TT slot.

    int best;
    uint8_t tt_hit;
    tt_entry entry = transposition_table_get(curBoard, context->tt, &tt_hit, ply);
    if(tt_hit)
    {
        RECORD_SEARCH(context->tt_hits++;);
        if(entry.nodeType == NODE_BOUND_EXACT ||
            (entry.nodeType == NODE_BOUND_UPPER && entry.evaluation <= alpha) ||
            (entry.nodeType == NODE_BOUND_LOWER && entry.evaluation >= beta))
            {
                RECORD_SEARCH(context->tt_cutoffs++;);
                return entry.evaluation;
            }

        temp.raw = entry.bestMove;
        tt_move = &temp;
        best = clamp(entry.evaluation, alpha, beta);
    }
    else 
    {
        RECORD_SEARCH(context->tt_misses++;);
        best = evaluate(context, ply);
    
        tt_entry shallowEntry = {
            .depth = 0,
            .hashCode = curBoard->hashCode,
            .nodeType = NODE_BOUND_UNKNOWN,
            .evaluation = best,
            .age = curBoard->halfMoveCount
        };
        transposition_table_set(context->tt, shallowEntry, ply);
    }

    //Stand Pat
    alpha = _max(alpha, best);
    if(alpha >= beta) return best;

    //Delta pruning
    if(!useNNUE)
    {
        int largestDelta = delta_pruning_offset;

        int opposingColor = FLIP_COLOR(curBoard->turn);

        if(curBoard->pieces[QUEEN | opposingColor])
            largestDelta += evaluatePhasedScore(curBoard, hce_params.genericPieceValues[QUEEN / 2]);
        else if(curBoard->pieces[ROOK | opposingColor])
            largestDelta += evaluatePhasedScore(curBoard, hce_params.genericPieceValues[ROOK / 2]);
        else if(curBoard->pieces[BISHOP | opposingColor])
            largestDelta += evaluatePhasedScore(curBoard, hce_params.genericPieceValues[BISHOP / 2]);
        else if(curBoard->pieces[KNIGHT | opposingColor])
            largestDelta += evaluatePhasedScore(curBoard, hce_params.genericPieceValues[KNIGHT / 2]);
        else if(curBoard->pieces[PAWN | opposingColor])
            largestDelta += evaluatePhasedScore(curBoard, hce_params.genericPieceValues[PAWN / 2]);
        if(largestDelta + best < alpha) 
            return best;
    }
    else if(delta_pruning_nnue_offset + best < alpha)
        return best;

    moveIterator* iter = create_move_iterator(context, curBoard->in_check ? GET_ALL_MOVES : GET_CAPTURES_AND_CHECKS, ply, pvMove, tt_move);

    int validMovesVisited = 0;
    move bestMove = {0};
    if(iter)
    {
        move* currentMove;
        while((currentMove = iterate_next_move(iter)) != NULL)
        {
            if(moveFromStruct(curBoard, nextBoard, *currentMove, &context->repetitions)) 
                continue;
            context->moveStack[ply] = *currentMove;

            //SEE pruning
            if(!nextBoard->in_check && iter->moveScores[iter->visitedCount - 1] < -CAPTURE_SCORE)
                continue;


            int piece = findPieceOnSquare(curBoard, currentMove->startSquare);
            int capturedPiece = findPieceOnSquare(curBoard, currentMove->endSquare);
            int isEP = 0;
            if(capturedPiece == EMPTY_PIECE && ISPAWN(piece) && currentMove->endSquare == curBoard->enPassantSquare)
            {
                capturedPiece = FLIP_COLOR(piece);
                isEP = 1;
            }
            
            validMovesVisited++;
            if(useNNUE)
                updateMoveAccumulator(nextBoard, *currentMove, capturedPiece, isEP, &context->accumulatorStack[ply], &context->accumulatorStack[ply + 1], context->refreshTable);

            int score = -quiescentSearch(context, -beta, -alpha, ply + 1, pvNode);

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
    
    if(validMovesVisited > 0)
    {
        RECORD_SEARCH(context->quiescentSearchedMoves += validMovesVisited; 
                      context->quiescentSearchedPositions++;);
        tt_entry shallowEntry = {
            .depth = 0,
            .hashCode = curBoard->hashCode,
            .nodeType = (best >= beta) ? NODE_BOUND_LOWER : ( (best > lowestBound) ? NODE_BOUND_EXACT : NODE_BOUND_UPPER),
            .evaluation = best,
            .age = curBoard->halfMoveCount,
            .bestMove = bestMove.raw
        };
        transposition_table_set(context->tt, shallowEntry, ply);
    }
    else if(curBoard->in_check)
        return -SCORE_WIN + ply;

    return best;
}

int principalVariationSearch(searchThreadContext* context, int alpha, int beta, int depth, int ply, PVar* myPV, int pvNode, int cutNode)
{
    assert(context);
    context->countedNodes++;
    RECORD_SEARCH(context->pvs_nodes++;);
    bitboard* curBoard = &context->boardStack[ply];

    myPV->length = 0;
    PVar childPV;

    move* pvMove = (curBoard->hashCode == context->pv.hashCodes[ply]) ? &context->pv.line[ply] : NULL;
    move* tt_move = NULL;
    move temp; //Copy in from TT instead of saving a ptr to a volatile TT slot.

    move bestMove = {0};

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
    
    if(isDraw(curBoard, &context->repetitions) == VICTOR_DRAW)
        return (ply & 3) - 1;
    //Mate distance pruning for non-root nodes.
    if(ply != 0)
    {
        alpha = _max(alpha, -SCORE_WIN + ply);
        beta = _min(beta, SCORE_WIN - ply - 1);
        if(alpha >= beta) return alpha;
    }

    int score = 0;

    //Transposition table
    tt_entry new_tt_entry = {
        .depth = depth,
        .hashCode = curBoard->hashCode,
        .age = curBoard->halfMoveCount
    };
    uint8_t hit;
    tt_entry old_tt_entry = transposition_table_get(curBoard, context->tt, &hit, ply);
    if(hit && context->excludedMove[ply].raw && old_tt_entry.bestMove == context->excludedMove[ply].raw)
        hit = 0;
    
    if(hit)
    {
        RECORD_SEARCH(context->tt_hits++;);
        if(old_tt_entry.depth >= depth && (!pvNode || depth == 0) && (cutNode || old_tt_entry.evaluation <= alpha))
        {
            if(old_tt_entry.nodeType == NODE_BOUND_EXACT ||
                (old_tt_entry.nodeType == NODE_BOUND_UPPER && old_tt_entry.evaluation <= alpha) ||
                (old_tt_entry.nodeType == NODE_BOUND_LOWER && old_tt_entry.evaluation >= beta))
                {
                    RECORD_SEARCH(context->tt_cutoffs++;);
                    return old_tt_entry.evaluation;
                }
        }
        
        temp.raw = old_tt_entry.bestMove; 
        tt_move = &temp;
        score = old_tt_entry.evaluation;
    }

    if(ply >= MAX_PLY - 1)
        return evaluate(context, ply);
    if(depth <= 0)
        return quiescentSearch(context, alpha, beta, ply, pvNode);
        
    bitboard* nextBoard = &context->boardStack[ply + 1];
    
    //Syzygy
    if(!pvNode && depth >= syzygyProbeDepth)
    {
        int result = getSyzygyResult(curBoard);
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
        RECORD_SEARCH(context->tt_misses++;);
        if(curBoard->in_check) 
            score = -SCORE_WIN;
        else if(context->excludedMove[ply].raw)
            score = context->evalHistory[ply];
        else
        {
            score = evaluate(context, ply);
            int16_t correction = context->pawnCorrHist[curBoard->turn][curBoard->pawnHash & (CORRHIST_SIZE - 1)] / 1024;
            score += correction;

            tt_entry shallowEntry = {
                .depth = 0,
                .hashCode = curBoard->hashCode,
                .nodeType = NODE_BOUND_UNKNOWN,
                .evaluation = score,
                .age = curBoard->halfMoveCount
            };
            transposition_table_set(context->tt, shallowEntry, ply);
        }
    }

    //Improving
    int staticScore = score;
    context->evalHistory[ply] = staticScore;
    if(curBoard->in_check) context->improving[ply] = 0;
    else context->improving[ply] = (ply >= 2) ? (score > context->evalHistory[ply - 2]) : 1;

    if(ply < MAX_PLY - 1)
        context->killerMoves[ply+1][0].raw = context->killerMoves[ply+1][1].raw = 0;

    if(!pvNode && !curBoard->in_check && abs(score) < MIN_MATE_SCORE)
    {
        //Stable Eval Reduction
        if(ply >= 2 && staticScore >= beta && cutNode && depth > 8 && abs(context->evalHistory[ply - 2] - staticScore) < stable_eval_margin)
            depth--;

        //Reverse Futility Pruning
        if(depth <= reverse_futility_pruning_depth)
        {
            int reducedVal = context->improving[ply] ? score - reverse_futility_margin_improving * depth : score - reverse_futility_margin * depth;
            if(reducedVal >= beta) 
                return reducedVal;
        }

        //Razoring
        if(score + razoring_a * depth * depth + razoring_b <= alpha)
        {
            int qScore = quiescentSearch(context, alpha - 1, alpha, ply, pvNode);
            if(qScore < alpha)
                return qScore;
        }

        //Null move pruning
        if(score >= beta && depth >= nullmove_pruning_depth && cutNode &&
            !(ply > 0 && context->moveStack[ply - 1].raw == 0) &&
            (curBoard->pieces_all ^ (curBoard->pieces[WHITE_KING] | curBoard->pieces[BLACK_KING] | curBoard->pieces[WHITE_PAWN] | curBoard->pieces[BLACK_PAWN])))
        {
            int r = 3 + depth / 6;
            applyNullMove(curBoard, nextBoard, &context->repetitions);
            memcpy(&context->accumulatorStack[ply + 1], &context->accumulatorStack[ply], sizeof(accumulator));
            context->moveStack[ply].raw = 0;
            int nullScore = -principalVariationSearch(context, -beta, -beta + 1, depth - r, ply + 1, &childPV, 0, !cutNode);
            if(nullScore >= beta)
            {
                if(nullScore < MIN_MATE_SCORE)
                    return nullScore;
                else
                    return beta;
            }
        }

        //Probcut
        if(depth >= probcut_depth)
        {
            int nextDepth = depth - probcut_depth_reduction;
            int pBeta = (context->improving[ply]) ? beta + probcut_offset_improving: beta + probcut_offset;

            if(score >= pBeta && pBeta < MIN_MATE_SCORE && (!hit || old_tt_entry.depth < nextDepth))
            {
                int probCutScore = INT32_MIN;
                moveIterator* iter = create_move_iterator(context, GET_WINNING_CAPTURES, ply, pvMove, tt_move);
                if(iter)
                {
                    move* currentMove;
                    while((currentMove = iterate_next_move(iter)) != NULL)
                    {
                        int piece = findPieceOnSquare(curBoard, currentMove->startSquare);
                        int capturedPiece = findPieceOnSquare(curBoard, currentMove->endSquare);
                        int isEP = 0;
                        if(capturedPiece == EMPTY_PIECE && ISPAWN(piece) && currentMove->endSquare == curBoard->enPassantSquare)
                        {
                            capturedPiece = FLIP_COLOR(piece);
                            isEP = 1;
                        }

                        if(moveFromStruct(curBoard, nextBoard, *currentMove, &context->repetitions)) continue;
                        context->moveStack[ply] = *currentMove;
                        
                        if(useNNUE)
                            updateMoveAccumulator(nextBoard, *currentMove, capturedPiece, isEP, &context->accumulatorStack[ply], &context->accumulatorStack[ply + 1], context->refreshTable);

                        probCutScore = -quiescentSearch(context, -pBeta - 1, -pBeta, ply + 1, pvNode);
                        if(probCutScore >= pBeta)
                            probCutScore = -principalVariationSearch(context, -pBeta - 1, -pBeta, nextDepth, ply + 1, &childPV, 0, !cutNode);

                        if(probCutScore >= pBeta)
                        {
                            if(!hit || old_tt_entry.depth < nextDepth)
                            {
                                tt_entry pcutEntry = {
                                    .depth = nextDepth,
                                    .hashCode = curBoard->hashCode,
                                    .nodeType = NODE_BOUND_LOWER,
                                    .evaluation = beta,
                                    .age = curBoard->halfMoveCount,
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
    if(!curBoard->in_check && !context->excludedMove[ply].raw && depth >= tt_reduction_depth && (pvNode || cutNode) && (!hit || old_tt_entry.depth + tt_reduction_min_depth_offset < depth))
        depth--;

    moveIterator* iter = create_move_iterator(context, GET_ALL_MOVES, ply, pvMove, tt_move);
    int validMovesVisited = 0;
    int bestScore = -SCORE_WIN;
    if(iter)
    {
        move* currentMove;

        while((currentMove = iterate_next_move(iter)) != NULL)
        {
            int moveScore = iter->moveScores[iter->visitedCount - 1];
            int next_depth = depth - 1;

            int currentPiece = findPieceOnSquare(curBoard, currentMove->startSquare);
            int capturedPiece = findPieceOnSquare(curBoard, currentMove->endSquare);
            int isEP = 0;
            if(capturedPiece == EMPTY_PIECE && ISPAWN(currentPiece) && currentMove->endSquare == curBoard->enPassantSquare)
            {
                capturedPiece = FLIP_COLOR(currentPiece);
                isEP = 1;
            }
            int isCapture = capturedPiece != EMPTY_PIECE;

            //Singular Extension
            if(hit && depth >= singular_extension_depth && currentMove->raw == tt_move->raw && old_tt_entry.depth >= depth - 3 && 
               old_tt_entry.nodeType != NODE_BOUND_UPPER && !context->excludedMove[ply].raw)
            {
                int sBeta = old_tt_entry.evaluation - 3 * depth;
                int sDepth = depth / 2 + 1;

                context->excludedMove[ply] = *tt_move;
                int singularScore = principalVariationSearch(context, sBeta - 1, sBeta, sDepth, ply, &childPV, 0, cutNode);
                context->excludedMove[ply].raw = 0;

                if(singularScore < sBeta)
                {
                    if(singularScore + 25 * depth < sBeta)
                        next_depth += 3;
                    else if(singularScore + 10 * depth < sBeta)
                        next_depth += 2;
                    else
                        next_depth++;
                }
                //Multicut pruning, but we just take the strong singular and assume that there's going to be more.
                else if(singularScore >= beta && abs(singularScore) < MIN_MATE_SCORE)
                    return singularScore;
                //Negative Extension
                else if(cutNode || old_tt_entry.evaluation >= beta)
                    next_depth--;
            }

            //Quiet Move Pruning
            if(!shouldSkipQuiets)
            {
                //Late move pruning
                if(validMovesVisited > lmpTable[context->improving[ply]][ply])
                    shouldSkipQuiets = 1;

                //Futility Pruning
                int lmrDepth = _max(0, depth - lmrTable[depth][validMovesVisited]);
                if(!curBoard->in_check && lmrDepth <= futility_pruning_depth && staticScore + lmrDepth * futility_depth_margin + futility_margin <= alpha)
                    shouldSkipQuiets = 1;

            }

            if(moveFromStruct(curBoard, nextBoard, *currentMove, &context->repetitions)) continue;
            context->moveStack[ply] = *currentMove;
            
            int isQuietMove = (!nextBoard->in_check && !isCapture && !currentMove->promoteTo);
            
            if(isQuietMove && shouldSkipQuiets && !pvNode && abs(bestScore) < MIN_MATE_SCORE && moveScore != KILLER_1_SCORE && moveScore != KILLER_2_SCORE)
                continue;

            //Check extensions
            if(!pvNode && nextBoard->in_check)
                next_depth++;
            
            //Late move reduction
            if(!pvNode && isQuietMove && !context->improving[ply]) 
                next_depth -= lmrTable[depth][validMovesVisited];

            //SEE reduction
            if(!pvNode && moveScore < -CAPTURE_SCORE)
                next_depth-=2;
            
            //History Reduction
            if(!pvNode && isQuietMove && moveScore < lowHistoryVal)
                next_depth--;

            if(useNNUE)
                updateMoveAccumulator(nextBoard, *currentMove, capturedPiece, isEP, &context->accumulatorStack[ply], &context->accumulatorStack[ply + 1], context->refreshTable);

            if(pvNode && (validMovesVisited == 0 || score > alpha)) 
                score = -principalVariationSearch(context, -beta, -alpha, next_depth, ply + 1, &childPV, 1, 0);
            else
            {
                //Scout
                score = -principalVariationSearch(context, -alpha - 1, -alpha, next_depth, ply + 1, &childPV, 0, 1);

                //LMR Re-search
                if(score > alpha && next_depth < depth - 1)
                {
                    next_depth = depth - 1;
                    score = -principalVariationSearch(context, -alpha - 1, -alpha, next_depth, ply + 1, &childPV, 0, !cutNode);
                }
                
                //PVS Re-search
                if(score > alpha && pvNode) 
                    score = -principalVariationSearch(context, -beta, -alpha, next_depth, ply + 1, &childPV, 1, 0);
            }
            
            validMovesVisited++;
            if(score > bestScore)
            {
                bestScore = score;
                bestMove = *currentMove;

                if(score > alpha)
                {
                    alpha = score;
                    
                    //Save PV
                    if(pvNode)
                    {
                        myPV->line[0] = *currentMove;
                        myPV->hashCodes[0] = curBoard->hashCode;
                        memcpy(&myPV->line[1], childPV.line, childPV.length * sizeof(move));
                        memcpy(&myPV->hashCodes[1], childPV.hashCodes, childPV.length * sizeof(uint64_t));
                        myPV->length = childPV.length + 1;
                    }
                }

                if(alpha >= beta)
                {
                    if(isQuietMove)
                    {
                        //Killer heuristic
                        if(currentMove->raw != context->killerMoves[ply][0].raw)
                        {
                            context->killerMoves[ply][1] = context->killerMoves[ply][0];
                            context->killerMoves[ply][0] = *currentMove;
                        }
                    
                        //History heuristic
                        int bonus = historyBonusScale * depth + historyBonusOffset;
                        int penalty = historyPenaltyScale * depth + historyPenaltyOffset;
                        int16_t* dest = &context->historyTable[curBoard->turn][PIECE(currentPiece) / 2][currentMove->endSquare];
                        *dest = _min(*dest + bonus, MAX_HISTORY_SCORE);

                        int16_t* straightArr = (int16_t*) context->historyTable[curBoard->turn];
                        for(int i = 0; i < searchedQuietCount; i++)
                            straightArr[searchedQuietIndices[i]] = _max(straightArr[searchedQuietIndices[i]] - penalty, -MAX_HISTORY_SCORE);

                        //Countermove heuristic
                        if(ply >= 1)
                        {
                            int side = context->boardStack[ply - 1].turn;
                            int from = context->moveStack[ply - 1].startSquare;
                            int piece = PIECE(findPieceOnSquare((&context->boardStack[ply - 1]), from)) / 2;
                            int to = context->moveStack[ply - 1].endSquare;
                            context->countermove[side][piece][to] = *currentMove;
                        }
                        
                        //Follow-up Move heuristic
                        if(ply >= 2)
                        {
                            int side = context->boardStack[ply - 2].turn;
                            int from = context->moveStack[ply - 2].startSquare;
                            int piece = PIECE(findPieceOnSquare((&context->boardStack[ply - 2]), from)) / 2;
                            int to = context->moveStack[ply - 2].endSquare;
                            context->followUpMove[side][piece][to] = *currentMove;
                        }
                    }
                    else if(capturedPiece != EMPTY_PIECE)
                    {
                        //Capture History Herustic
                        context->captureHistoryTable[currentPiece / 2][currentMove->endSquare][capturedPiece / 2] += historyBonusScale * depth + historyBonusOffset;
                        context->captureHistoryTable[currentPiece / 2][currentMove->endSquare][capturedPiece / 2] = _min(context->captureHistoryTable[currentPiece / 2][currentMove->endSquare][capturedPiece / 2], MAX_HISTORY_SCORE);
                    }

                    break;
                }
            }

            if(isQuietMove)
                searchedQuietIndices[searchedQuietCount++] = ((PIECE(currentPiece) / 2) * 64) + currentMove->endSquare;
            else if(capturedPiece != EMPTY_PIECE)
            {
                //Capture History Herustic
                context->captureHistoryTable[currentPiece / 2][currentMove->endSquare][capturedPiece / 2] -= historyPenaltyScale * depth + historyPenaltyOffset;
                context->captureHistoryTable[currentPiece / 2][currentMove->endSquare][capturedPiece / 2] = _max(context->captureHistoryTable[currentPiece / 2][currentMove->endSquare][capturedPiece / 2], -MAX_HISTORY_SCORE);
            }
        }
        destroy_move_iterator(iter);
        
    }
    
    if(!iter || validMovesVisited == 0)
    {
        //We know its not a (stale-)mate position if an excluded move exists at this ply.
        if(context->excludedMove[ply].raw)
            return alpha;

        int victor = getMateResult(curBoard);
        if(victor == VICTOR_WHITE)
            return (curBoard->turn == WHITE) ? (SCORE_WIN - ply) : -(SCORE_WIN - ply);
        else if(victor == VICTOR_BLACK)
            return (curBoard->turn == BLACK) ? (SCORE_WIN - ply) : -(SCORE_WIN - ply);
        else
            return 0;
    }
    
    new_tt_entry.nodeType = (bestScore >= beta) ? NODE_BOUND_LOWER : ( (bestScore > lowestBound) ? NODE_BOUND_EXACT : NODE_BOUND_UPPER);
    new_tt_entry.evaluation = bestScore;
    new_tt_entry.bestMove = bestMove.raw;
    transposition_table_set(context->tt, new_tt_entry, ply);

    //Correction History
    if((bestScore >= beta || new_tt_entry.nodeType == NODE_BOUND_EXACT) && abs(bestScore) < MIN_MATE_SCORE)
    {
        int16_t error = clamp(1024 * (bestScore - staticScore), -MAX_CORRHIST_VAL, MAX_CORRHIST_VAL);
        int16_t weight = _min(depth * depth + 2, 256);
        int16_t* oldHist = &context->pawnCorrHist[curBoard->turn][curBoard->pawnHash & (CORRHIST_SIZE - 1)];
        *oldHist += clamp(weight * (error - *oldHist) / 1024, -MAX_CORRHIST_VAL, MAX_CORRHIST_VAL);
    }

    RECORD_SEARCH(context->pvsSearchedMoves += validMovesVisited; 
                  context->pvsSearchedPositions++;);

    return bestScore;
}

void printResultingMoves(move bestMove, move ponderMove, int isBookMove)
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
        score = principalVariationSearch(context, -SCORE_WIN, SCORE_WIN, currentDepth, 0, &tempPV, 1, 0);
        context->completedDepth = currentDepth;
    }
    else
    {

        int aspiration_margin = initial_aspiration_margin;
        int alpha = context->score - aspiration_margin;
        int beta = context->score + aspiration_margin;
        while(1)
        {
            score = principalVariationSearch(context, alpha, beta, currentDepth, 0, &tempPV, 1, 0);

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
                alpha = -SCORE_WIN;
                beta = SCORE_WIN;
            }
        }
    }
    
    if(*context->abortFlag == 0)
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

    bitboard* board = &context->boardStack[0];
    move bestMove = context->pv.line[0];
    
    if(useNNUE)
        updateAccumulatorFromTable(board, &context->accumulatorStack[0], context->refreshTable);
        
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
    
    printf("info depth %d seldepth %d score ", bestDepth, best->seldepth);
    
    int absScore = abs(bestScore);
    assert(absScore <= SCORE_WIN);
    if(absScore >= MIN_MATE_SCORE)
    {
        int mateInPlies = SCORE_WIN - absScore;
        int mateInMoves = (mateInPlies + 1) / 2;
        if(bestScore < 0) mateInMoves = -mateInMoves;
        printf("mate %d ", mateInMoves);
    }
    else printf("cp %d ", bestScore);

    printf("nodes %d nps %d hashfull %" PRId64 " time %d", totalNodes, NPS, (1000 * best->tt->usedSlots) / best->tt->capacity, milliseconds);

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
    memset(context->captureHistoryTable, 0, sizeof(context->captureHistoryTable));
    memset(context->countermove, 0, sizeof(context->countermove));
    memset(context->followUpMove, 0, sizeof(context->followUpMove));
    RECORD_SEARCH(context->pvs_nodes = 0;
                  context->qs_nodes = 0;
                  context->tt_hits = 0;
                  context->tt_cutoffs = 0;
                  context->tt_misses = 0;
                  context->quiescentSearchedMoves = 0;
                  context->quiescentSearchedPositions = 0;
                  context->pvsSearchedMoves = 0;
                  context->pvsSearchedPositions = 0;
                  context->evaluations = 0;);

    int maxDepth = context->maxDepth;
    
    move bestMove = (move){0}; 
    move ponderMove = (move){0};
    int helperThreadCount = threadCount - 1;

    bitboard* board = &context->boardStack[0];
    context->countedNodes = 0;
    context->seldepth = 0;
    context->completedDepth = 0;

    //Book moves
    if(!isPonder)
    {
        context->pv.line[0] = getBookMove(board);
        if(IS_VALID_MOVE(context->pv.line[0])) { printResultingMoves(context->pv.line[0], (move){0}, 1); isCalculating = 0; return 0; }
        else board->in_book = 0;
    }

    //Syzygy move recommendations
    filterSyzygyMoves(board, context->searchedMoves);

    THREADTYPE *helperThreads = NULL;
    searchThreadContext* helperThreadContext = NULL;

    if(helperThreadCount > 0)
    {
        helperThreads = calloc(helperThreadCount, sizeof(THREADTYPE));
        helperThreadContext = calloc(helperThreadCount, sizeof(searchThreadContext));

        for(int i = 0; i < helperThreadCount; i++) 
        {
            helperThreadContext[i].abortFlag = context->abortFlag;

            memcpy(&helperThreadContext[i].boardStack[0], &context->boardStack[0], sizeof(bitboard));
            helperThreadContext[i].repetitions.capacity = context->repetitions.capacity;
            helperThreadContext[i].repetitions.hashCodes = calloc(helperThreadContext[i].repetitions.capacity, sizeof(uint64_t));
            memcpy(helperThreadContext[i].repetitions.hashCodes, context->repetitions.hashCodes, helperThreadContext[i].repetitions.capacity * sizeof(uint64_t));

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
                helperThreadContext[i].accumulatorStack = calloc(MAX_PLY + 1, sizeof(accumulator));
                helperThreadContext[i].refreshTable = calloc(1, sizeof(accumulatorRefreshTable));;
            }

            memcpy(helperThreadContext[i].searchedMoves, context->searchedMoves, 16*sizeof(move));
            THREAD_START(helperThreads[i], helperThreadFunction, &helperThreadContext[i]);
        }
    }
    
    if(useNNUE)
        updateAccumulatorFromTable(board, &context->accumulatorStack[0], context->refreshTable);
    int lastScore = 0;
    for(int currentDepth = 1; currentDepth <= maxDepth; currentDepth++)
    {
        aspiration_window(context, currentDepth);
        
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

           printf("info depth %d seldepth %d score ", currentDepth, context->seldepth);
    
            int absScore = abs(context->score);
            assert(absScore <= SCORE_WIN);
            if(absScore >= MIN_MATE_SCORE)
            {
                int mateInPlies = SCORE_WIN - absScore;
                int mateInMoves = (mateInPlies + 1) / 2;
                if(context->score < 0) mateInMoves = -mateInMoves;
                printf("mate %d ", mateInMoves);
            }
            else printf("cp %d ", context->score);

            printf("nodes %d nps %d hashfull %" PRId64 " time %d", totalNodes, NPS, (1000 * context->tt->usedSlots) / context->tt->capacity, milliseconds);

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
                if(m.promoteTo == QUEEN) printf("q");
                else if(m.promoteTo == ROOK) printf("r");
                else if(m.promoteTo == BISHOP) printf("b");
                else if(m.promoteTo == KNIGHT) printf("n");
            }

            printf("\n");
            fflush(stdout);
            
            if(!isPonder && currentDepth > 1 && (*context->abortFlag || clock() > context->softEndTime || context->countedNodes >= (context->softMaxNodes / threadCount))) break;
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
            free(helperThreadContext[i].repetitions.hashCodes);
            if(useNNUE)
            {
                free(helperThreadContext[i].accumulatorStack);
                free(helperThreadContext[i].refreshTable);
            }
        }
        findBestThread(context, helperThreadContext, &bestMove, &ponderMove);
        
        free(helperThreads);
        free(helperThreadContext);
    }

    #ifdef SEARCHINFO
    float total_tt = context->tt_hits + context->tt_misses;
    printf("Search Statistics:\n");
    printf("\tQuiescent Nodes: %" PRId64 "\n", context->qs_nodes);
    printf("\tQS Branching Factor: %f\n", (float) context->quiescentSearchedMoves / context->quiescentSearchedPositions);
    printf("\tPVS Nodes: %" PRId64 "\n", context->pvs_nodes);
    printf("\tPVS Branching Factor: %f\n", (float) context->pvsSearchedMoves / context->pvsSearchedPositions);
    printf("\tTT Hits: %" PRId64 " (%f%%)\n", context->tt_hits, (100.0 * context->tt_hits) / total_tt);
    printf("\tTT Cutoffs: %" PRId64 " (%f%%)\n", context->tt_cutoffs, (100.0 * context->tt_cutoffs) / total_tt);
    printf("\tTT Misses: %" PRId64 " (%f%%)\n", context->tt_misses, (100.0 * context->tt_misses) / total_tt);
    printf("\tEvaluations: %" PRId64 "\n", context->evaluations);
    #endif

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