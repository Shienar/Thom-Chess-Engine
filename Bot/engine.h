#ifndef ENGINE
#define ENGINE

#include "bitboard.h"
#include "moves.h"

typedef struct {
    double pawnPieceWeights[64];
    double knightPieceWeights[64];
    double bishopPieceWeights[64];
    double rookPieceWeights[64];
    double queenPieceWeights[64];
    double kingPieceWeights[64];

    double pawnWeight;
    double knightWeight;
    double bishopWeight;
    double rookWeight;
    double queenWeight;
    double kingWeight;
} weights;


/**
 * Initializes piece weights.
 * Set useExisting to 1 to read weights from file (TODO).
 */
void initPieceWeights(weights* w, int useExisting);

/**
 * Evaluates a position based on piece/square weights.
 * Returns white's score - black's score.
 */
double evaluate(bitboard* board, weights* w);

/**
 * Standard minmax algorithm with alpha/beta pruning.
 * White is max
 * Black is min
 * 
 * Usage:
 *  - Call with alpha at DBL_MIN, beta at DBL_MAX
 *  - For each child move, score = alpha_beta()
 *  - If engine is black, choose the move with the lowest score.
 *  - If engine is white, choose the move with the highest score.
 */
double alpha_beta(bitboard board, weights* weights, double alpha, double beta, int depth);

/**
 * Calls alpha_beta as describe above and returns the best move.
 */
move calculateBestMove(bitboard* board, weights* weights, int depth);

#endif