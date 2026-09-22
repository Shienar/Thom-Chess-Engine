#ifndef EVAL_HISTORY
#define EVAL_HISTORY

#include "types.h"

#define MAX_CORRHIST_VAL 256
#define CORRHIST_GRAIN 32

extern int historyBonusScale;
extern int historyBonusOffset;
extern int historyPenaltyScale;
extern int historyPenaltyOffset;

void updateKillerMoves(threadContext* context, move currentMove, int ply);
void updateHistoryValues(int16_t* historyTable, int boostedIndex, int searchedQuietIndices[MAX_MOVES], int searchedQuietCount, int depth);
void updateContinuationHistory(threadContext* context, move currentMove, int ply);

void applyCaptureHistoryBonus(int16_t* dest, int depth);
void applyCaptureHistoryPenalty(int16_t* dest, int depth);

void updateCorrectionHistory(int16_t* oldHist, int depth, int searchScore, int staticScore);
int  getCorrectionHistoryOffset(threadContext* context, bitboard* board);

#endif