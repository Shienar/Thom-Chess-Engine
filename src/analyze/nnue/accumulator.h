#pragma once

#include "board/bitboard.h"
#include "analyze/nnue/neuralnet.h"

void loadInputAccumulator(bitboard* board, accumulator* acc);
void updateMoveAccumulator(bitboard* board, move lastMove, int capturedPiece, int isEnPassant, accumulator* inputAcc, accumulator* outputAcc, accumulatorRefreshTable* refreshTable);
void updateAccumulatorFromTable(bitboard* board, accumulator* acc,  accumulatorRefreshTable* refreshTable);