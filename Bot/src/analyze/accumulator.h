#ifndef ACCUMULATOR
#define ACCUMULATOR

#include "../structs.h"
#include "neuralnet.h"

#define TRAINING 1
#define PLAYING 2

#define MIRROR_SQUARE(x) x^=7

//https://www.chessprogramming.org/Flipping_Mirroring_and_Rotating#Vertical
#define k1 0x5555555555555555
#define k2 0x3333333333333333
#define k4 0x0f0f0f0f0f0f0f0f
static inline uint64_t mirrorBoard(uint64_t x)
{
   x = ((x >> 1) & k1) | ((x & k1) << 1);
   x = ((x >> 2) & k2) | ((x & k2) << 2);
   x = ((x >> 4) & k4) | ((x & k4) << 4);
   return x;
}

//Main thread accumulators
extern accumulator_training* trainingAccumulator;
extern accumulator_playing* playerAccumulator;
extern accumulator_training_refreshTable* trainingRefreshTable;
extern accumulator_playing_refreshTable* playingRefreshTable;

void extractInputLayerToArray(uint64_t* inputLayerCompact_w, uint64_t* inputLayerCompact_b, void* output, int outputType);

/**
 * Reinitializes input nodes and accumulator based off of a bitboard.
 */
void loadInputAccumulator(bitboard* board, void* accumulator, int accumulatorType, int color);

// Updates an accumulator & board until it reaches the same state as the currentboard.
// Both boards must have the same kingsquares.
void updateBoardAccumulator(bitboard* currentBoard, bitboard* accumulatorBoard, void* accumulator, int accumulatorType, int color);

void updateAccumulatorFromTable(bitboard* currentBoard, void* accumulator, void* refreshTable, int accumulatorType);

accumulator_playing_refreshTable* createPlayingRefreshTable();
accumulator_training_refreshTable* createTrainingRefreshTable();

void destroyRefreshTable(void* table, int accumulatorType);

#endif