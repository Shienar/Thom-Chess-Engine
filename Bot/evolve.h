#ifndef EVOLVE
#define EVOLVE

#include "engine.h"
#define ENGINE_DEPTH 4
#define MUTATION_CHANCE 0.1

/**
 * Steepest Ascent Hill Climbing
 */
void hill_climb(weights* starting_weights, int maxIterations, int numTweaksPerIteration);

weights* createTweakedCopy(weights* original_weight);

/**
 * Compares engines by having them play two games, once with each color.
 * Returns 1 if w1 won.
 * Returns 0 on draw.
 * Returns -1 if w2 won.
 */
int compareEngines(weights* w1, weights* w2);

/**
 * Plays a game between engines
 * Returns 1 if white won.
 * Returns 0 on draw.
 * Returns -1 if black won.
 */
int play_engine_game(weights* whiteWeights, weights* blackWeights);

#endif