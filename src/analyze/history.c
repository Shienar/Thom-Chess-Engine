#include "analyze/history.h"
#include "board/bitboard.h"
#include "analyze/search.h"

int historyBonusScale = 266;
int historyBonusOffset = 181;
int historyPenaltyScale = 431;
int historyPenaltyOffset = 194;

int captureHistoryBonusScale = 346;
int captureHistoryBonusOffset = 121;
int captureHistoryPenaltyScale = 456;
int captureHistoryPenaltyOffset = 169;

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
    int bonus = _min(MAX_HISTORY_SCORE, _max(-MAX_HISTORY_SCORE, historyBonusScale * depth + historyBonusOffset));
    int penalty = -_min(MAX_HISTORY_SCORE, _max(-MAX_HISTORY_SCORE, historyPenaltyScale * depth + historyPenaltyOffset));

    historyTable[boostedIndex] += bonus - historyTable[boostedIndex] * abs(bonus) / MAX_HISTORY_SCORE;
    for(int i = 0; i < searchedQuietCount; i++)
        historyTable[searchedQuietIndices[i]] += penalty - historyTable[searchedQuietIndices[i]] * abs(penalty) / MAX_HISTORY_SCORE;
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
}

move getCounterMove(threadContext* context, int ply)
{
    if(ply < 1 || !context->moveStack[ply - 1].raw) return (move){0};

    int side = context->boardStack[ply - 1].turn;
    int from = context->moveStack[ply - 1].startSquare;
    int piece = PIECE(findPieceOnSquare((&context->boardStack[ply - 1]), from)) / 2;
    int to = context->moveStack[ply - 1].endSquare;
    return context->countermove[side][piece][to];
}

move getFollowupMove(threadContext* context, int ply)
{
    if(ply < 2 || !context->moveStack[ply - 2].raw) return (move){0};

    int side = context->boardStack[ply - 2].turn;
    int from = context->moveStack[ply - 2].startSquare;
    int piece = PIECE(findPieceOnSquare((&context->boardStack[ply - 2]), from)) / 2;
    int to = context->moveStack[ply - 2].endSquare;
    return context->followUpMove[side][piece][to];
}

int getCaptureHistoryOffset(threadContext* context, int piece, int to, int capturedPiece)
{
    return context->captureHistoryTable[piece / 2][to][capturedPiece / 2] / CAPTURE_HISTORY_SCALE;
}

void applyCaptureHistoryBonus(int16_t* dest, int depth)
{
    *dest = _min(*dest + (captureHistoryBonusScale * depth + captureHistoryBonusOffset), MAX_HISTORY_SCORE);
}

void applyCaptureHistoryPenalty(int16_t* dest, int depth)
{
    *dest = _max(*dest - (captureHistoryPenaltyScale * depth + captureHistoryPenaltyOffset), -MAX_HISTORY_SCORE);
}

void updateCorrectionHistory(int16_t* oldHist, int depth, int searchScore, int staticScore)
{
    *oldHist = _min(MAX_CORRHIST_VAL, _max(-MAX_CORRHIST_VAL, *oldHist + depth * depth * (searchScore - staticScore)));
}

int getCorrectionHistoryOffset(threadContext* context, bitboard* board)
{
    int correction = 0;
    correction += context->pawnCorrHist[board->turn][board->pawnHash & (CORRHIST_SIZE - 1)];
    correction += context->nonPawnCorrHist[WHITE][board->turn][board->nonPawnHash[WHITE] & (CORRHIST_SIZE - 1)];
    correction += context->nonPawnCorrHist[BLACK][board->turn][board->nonPawnHash[BLACK] & (CORRHIST_SIZE - 1)];
    return correction / CORRHIST_SCALE;
}