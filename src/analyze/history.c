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

int getHistoryValue(threadContext* context, int turn, int piece, int to)
{
    return context->historyTable[turn][piece / 2][to];
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

move* getCounterMove(threadContext* context, int ply)
{
    if(ply < 1 || !context->moveStack[ply - 1].raw) return NULL;

    int side = context->boardStack[ply - 1].turn;
    int from = context->moveStack[ply - 1].startSquare;
    int piece = PIECE(findPieceOnSquare((&context->boardStack[ply - 1]), from)) / 2;
    int to = context->moveStack[ply - 1].endSquare;
    return &context->countermove[side][piece][to];
}

move* getFollowupMove(threadContext* context, int ply)
{
    if(ply < 2 || !context->moveStack[ply - 2].raw) return NULL;

    int side = context->boardStack[ply - 2].turn;
    int from = context->moveStack[ply - 2].startSquare;
    int piece = PIECE(findPieceOnSquare((&context->boardStack[ply - 2]), from)) / 2;
    int to = context->moveStack[ply - 2].endSquare;
    return &context->followUpMove[side][piece][to];
}


int getCaptureHistoryOffset(threadContext* context, int piece, int to, int capturedPiece)
{
    return context->captureHistoryTable[piece / 2][to][capturedPiece / 2] / CAPTURE_HISTORY_SCALE;
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
    int correction = 0;
    correction += 50 * context->pawnCorrHist[board->turn][board->pawnHash & (CORRHIST_SIZE - 1)];
    correction += 25 * context->nonPawnCorrHist[WHITE][board->turn][board->nonPawnHash[WHITE] & (CORRHIST_SIZE - 1)];
    correction += 25 * context->nonPawnCorrHist[BLACK][board->turn][board->nonPawnHash[BLACK] & (CORRHIST_SIZE - 1)];
    correction /= 100;
    return correction / CORRHIST_GRAIN;
}