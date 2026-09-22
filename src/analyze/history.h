#ifndef EVAL_HISTORY
#define EVAL_HISTORY

#include "types.h"

#define CAPTURE_HISTORY_SCALE 64
#define MAX_CORRHIST_VAL 256
#define CORRHIST_GRAIN 32

extern int historyBonusScale;
extern int historyBonusOffset;
extern int historyPenaltyScale;
extern int historyPenaltyOffset;

void updateKillerMoves(threadContext* context, move currentMove, int ply);

int  getHistoryValue(threadContext* context, int turn, int piece, int to);
void updateHistoryValues(int16_t* historyTable, int boostedIndex, int searchedQuietIndices[MAX_MOVES], int searchedQuietCount, int depth);

void  updateContinuationHistory(threadContext* context, move currentMove, int ply);
move* getCounterMove(threadContext* context, int ply);
move* getFollowupMove(threadContext* context, int ply);

int  getCaptureHistoryOffset(threadContext* context, int piece, int to, int capturedPiece);
void applyCaptureHistoryBonus(int16_t* dest, int depth);
void applyCaptureHistoryPenalty(int16_t* dest, int depth);

void updateCorrectionHistory(int16_t* oldHist, int depth, int searchScore, int staticScore);
int  getCorrectionHistoryOffset(threadContext* context, bitboard* board);


#endif