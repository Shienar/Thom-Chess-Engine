#ifndef ACCUMULATOR
#define ACCUMULATOR

#include "../structs.h"
#include "neuralnet.h"

#define TRAINING 1
#define PLAYING 2

#define MIRROR_SQUARE(x) x^=7

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