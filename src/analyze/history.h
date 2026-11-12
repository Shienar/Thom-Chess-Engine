#pragma once

#include "board/board.h"

#define CORRHIST_SIZE 16384
#define MAX_HISTORY_SCORE 16384
#define CAPTURE_HISTORY_SCALE 4 //This is mainly use to avoid int16 limits at this point. It used to significantly reduce the impact of history values vs SEE ordering.
#define MAX_CORRHIST_VAL 256
#define CORRHIST_SCALE 32

extern int historyBonusScale;
extern int historyBonusOffset;
extern int historyPenaltyScale;
extern int historyPenaltyOffset;

extern int captureHistoryBonusScale;
extern int captureHistoryBonusOffset;
extern int captureHistoryPenaltyScale;
extern int captureHistoryPenaltyOffset;

void updateKillerMoves(struct threadContext* context, move currentMove, int ply);

int  getHistoryValue(struct threadContext* context, int turn, int piece, int to);
void updateHistoryValues(int16_t* historyTable, int boostedIndex, int searchedQuietIndices[MAX_MOVES], int searchedQuietCount, int depth);

void updateContinuationHistory(struct threadContext* context, move currentMove, int ply);
move getCounterMove(struct threadContext* context, int ply);
move getFollowupMove(struct threadContext* context, int ply);

int  getCaptureHistoryOffset(struct threadContext* context, int piece, int to, int capturedPiece);
void applyCaptureHistoryBonus(int16_t* dest, int depth);
void applyCaptureHistoryPenalty(int16_t* dest, int depth);

void updateCorrectionHistory(int16_t* oldHist, int depth, int searchScore, int staticScore);
int  getCorrectionHistoryOffset(struct threadContext* context, bitboard* board);