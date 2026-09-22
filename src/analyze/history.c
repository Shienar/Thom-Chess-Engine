#include "analyze/history.h"
#include "board/moves.h"

int historyBonusScale = 290;
int historyBonusOffset = 137;
int historyPenaltyScale = 392;
int historyPenaltyOffset = 131;

void updateKillerMoves(threadContext* context, move currentMove, int ply)
{
    if(currentMove.raw != context->killerMoves[ply][0].raw)
    {
        context->killerMoves[ply][1] = context->killerMoves[ply][0];
        context->killerMoves[ply][0] = currentMove;
    }
}

void updateHistoryValues(int16_t* historyTable, int boostedIndex, int searchedQuietIndices[MAX_MOVES], int searchedQuietCount, int depth)
{
    int bonus = historyBonusScale * depth + historyBonusOffset;
    int penalty = historyPenaltyScale * depth + historyPenaltyOffset;

    historyTable[boostedIndex] = _min(historyTable[boostedIndex] + bonus, MAX_HISTORY_SCORE);
    for(int i = 0; i < searchedQuietCount; i++)
        historyTable[searchedQuietIndices[i]] = _max(historyTable[searchedQuietIndices[i]] - penalty, -MAX_HISTORY_SCORE);
}

void updateContinuationHistory(threadContext* context, move currentMove, int ply)
{
    //Countermove heuristic
    if(ply >= 1)
    {
        int side = context->boardStack[ply - 1].turn;
        int from = context->moveStack[ply - 1].startSquare;
        int piece = PIECE(findPieceOnSquare((&context->boardStack[ply - 1]), from)) / 2;
        int to = context->moveStack[ply - 1].endSquare;
        context->countermove[side][piece][to] = currentMove;
    }
    
    //Follow-up Move heuristic
    if(ply >= 2)
    {
        int side = context->boardStack[ply - 2].turn;
        int from = context->moveStack[ply - 2].startSquare;
        int piece = PIECE(findPieceOnSquare((&context->boardStack[ply - 2]), from)) / 2;
        int to = context->moveStack[ply - 2].endSquare;
        context->followUpMove[side][piece][to] = currentMove;
    }
}

void applyCaptureHistoryBonus(int16_t* dest, int depth)
{
    *dest = _min(*dest + (historyBonusScale * depth + historyBonusOffset), MAX_HISTORY_SCORE);
}

void applyCaptureHistoryPenalty(int16_t* dest, int depth)
{
    *dest = _min(*dest - (historyPenaltyScale * depth + historyPenaltyOffset), -MAX_HISTORY_SCORE);
}

void updateCorrectionHistory(int16_t* oldHist, int depth, int searchScore, int staticScore)
{
    *oldHist = clamp(*oldHist + depth * depth * (searchScore - staticScore), -MAX_CORRHIST_VAL, MAX_CORRHIST_VAL);
}

int getCorrectionHistoryOffset(threadContext* context, bitboard* board)
{
    int correction = context->pawnCorrHist[board->turn][board->pawnHash & (CORRHIST_SIZE - 1)];
    return correction / CORRHIST_GRAIN;
}