#include "../include/engine.h"
#include "../include/moves.h"
#include "../include/bitboard.h"
#include "../include/debug.h"
#include "../include/book.h"
#include "../include/neuralnet.h"
#include <string.h>
#include <float.h>
#include <math.h>

/* Start of PeSTO */
int middlegame_piece_values[6] = { 82, 337, 365, 477, 1025,  0};
int endgame_piece_values[6] = { 94, 281, 297, 512,  936,  0};

int middlegame_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,  0,   0,
     98, 134,  61,  95,  68, 126, 34, -11,
     -6,   7,  26,  31,  65,  56, 25, -20,
    -14,  13,   6,  21,  23,  12, 17, -23,
    -27,  -2,  -5,  12,  17,   6, 10, -25,
    -26,  -4,  -4, -10,   3,   3, 33, -12,
    -35,  -1, -20, -23, -15,  24, 38, -22,
      0,   0,   0,   0,   0,   0,  0,   0,
};

int endgame_pawn_table[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};

int middlegame_knight_table[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23,
};

int endgame_knight_table[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};

int middlegame_bishop_table[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};

int endgame_bishop_table[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9, 14,  10,   3,   2,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17,
};

int middlegame_rook_table[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26,
};

int endgame_rook_table[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20,
};

int middlegame_queen_table[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};

int endgame_queen_table[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};

int middlegame_king_table[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};

int endgame_king_table[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

int* middlegame_piece_tables[6] = {middlegame_pawn_table, middlegame_knight_table, middlegame_bishop_table, middlegame_rook_table, middlegame_queen_table, middlegame_king_table };
int* endgame_piece_tables[6] = {endgame_pawn_table, endgame_knight_table, endgame_bishop_table, endgame_rook_table, endgame_queen_table, endgame_king_table };

int gamephasePieceWeights[6] = {0,1,1,2,4,0};
int middlegame_table[12][64];
int endgame_table[12][64];

void init_tables()
{
    for (int p = 0, pc = 0; p <= 5; pc += 2, p++) {
        for (int sq = 0; sq < 64; sq++) {
            middlegame_table[pc][sq] = middlegame_piece_values[p] + middlegame_piece_tables[p][sq];
            endgame_table[pc][sq] = endgame_piece_values[p] + endgame_piece_tables[p][sq];
            middlegame_table[pc+1][sq] = middlegame_piece_values[p] + middlegame_piece_tables[p][FLIP(sq)];
            endgame_table[pc+1][sq] = endgame_piece_values[p] + endgame_piece_tables[p][FLIP(sq)];
        }
    }
}

double evaluate(bitboard* board)
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

    int score_w_middlegame, score_b_middlegame, score_w_endgame, score_b_endgame, gamePhase = 0;

    /* evaluate each piece */
    for (int sq = 0; sq < 64; sq++) {
        int piece = findPieceOnSquare(board, sq);
        if (piece) 
        {
            if(ISWHITE(piece))
            {
                score_w_middlegame += middlegame_piece_values[(piece&0xF) - 1];
                score_w_endgame += endgame_piece_values[(piece&0xF) - 1];
                gamePhase += gamephasePieceWeights[(piece&0xF) - 1];
            }
            else
            {
                score_b_middlegame += middlegame_piece_values[(piece&0xF) - 1];
                score_b_endgame += endgame_piece_values[(piece&0xF) - 1];
                gamePhase += gamephasePieceWeights[(piece&0xF) - 1];
            }
            
        }
    }

    /* tapered eval */
    int middlegame_score, endgame_score;

    //Evaluate in response to whoever's turn it is.
    if(ISWHITE(board->turn)) {
        middlegame_score = score_w_middlegame - score_b_middlegame;
        endgame_score = score_w_endgame - score_b_endgame;
    }
    else {
        middlegame_score = score_b_middlegame - score_w_middlegame;
        endgame_score = score_b_endgame - score_w_endgame;
    }

    int middlegamePhase = gamePhase;
    if (middlegamePhase > 24) middlegamePhase = 24; /* in case of early promotion */
    int endgamePhase = 24 - middlegamePhase;
    return (middlegame_score * middlegamePhase + endgame_score * endgamePhase) / 24;

}
/* End of PeSTO*/

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
                double score = -quiesce(board, -beta, -alpha, depth - 1);
                unmove(board);
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
        new_tt_entry.evaluation = quiesce(board, alpha, beta, 10);
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
                
                unmove(board);

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
    if(entries ) 
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