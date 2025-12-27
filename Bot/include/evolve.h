#ifndef EVOLVE
#define EVOLVE

#include "structs.h"
#define MUTATION_CHANCE 0.1
#define LEARNING_RATE 0.1
#define MAX_WEIGHT 25
#define MIN_WEIGHT 0

/**
 * Steepest Ascent Hill Climbing
 */
void hill_climb(engine* starting_weights, int maxIterations, int numTweaksPerIteration, int maxDepth, int maxTime);

engine* createTweakedCopy(engine* original_weight);

/**
 * Compares engines by having them play two games, once with each color.
 * Returns 1 if w1 won.
 * Returns 0 on draw.
 * Returns -1 if w2 won.
 */
int compareEngines(engine* w1, engine* w2, int maxDepth, int maxTime);

/**
 * Plays a game between engines
 * Returns 1 if white won.
 * Returns 0 on draw.
 * Returns -1 if black won.
 */
int play_engine_game(engine* whiteWeights, engine* blackWeights, int maxDepth, int maxTime);

/**
 * Evolution results get saved to and loaded from a txt file.
 * Returns 0 on success, -1 on error.
 */
int saveEngineWeights(engine* e);
int loadEngineWeights(engine* e);

#endif