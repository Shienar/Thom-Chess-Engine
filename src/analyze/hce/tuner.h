#ifndef TEXEL_TUNER
#define TEXEL_TUNER

#include "analyze/hce/hce.h"

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8f

#define PI 3.141592653589793

typedef struct {
    double mg;
    double eg;
} evalfp_t;

#define UPDATE_COEFFICIENTS(var, val, op, index) \
    do { \
        (coefficients->var)index.mg op val; \
        (coefficients->var)index.eg op val; \
        score->mg op val * (params.var)index.mg; \
        score->eg op val * (params.var)index.eg; \
    } while(0)

typedef struct tuningTuple {
    int index;
    int coefficient; //White - black
} tuningTuple;

typedef struct tuningEntry {
    double heuristic_eval;
    evalfp_t score;
    eval_t phase;
    double result;
    evalfp_t phaseFactors;
    tuningTuple* activeTuples;
    int activeTupleCount;
} tuningEntry;

typedef union {
    struct {
        evalfp_t genericPieceValues[PIECE_TYPE_COUNT];
        evalfp_t rawPieceTables[PIECE_TYPE_COUNT][64];

        evalfp_t knightMobilityBonus[9];
        evalfp_t bishopMobilityBonus[14];
        evalfp_t rookMobilityBonus[15];
        evalfp_t queenMobilityBonus[28];
        evalfp_t virtualMobilityBonus[28];
        
        evalfp_t pawnAttacks[PAWN_ATTACK_TYPES];
        evalfp_t minorPawnCover;
        evalfp_t passedPawnBonus[ROW_COUNT];
        evalfp_t connectedPawnBonus[ROW_COUNT];
        evalfp_t doubledPawnBonus[COLUMN_COUNT];
        evalfp_t isolatedPawnBonus[COLUMN_COUNT];

        evalfp_t knightOutpostBonus;

        evalfp_t bishopPairBonus;
        evalfp_t badBishopBonus;
        
        evalfp_t openRookFileBonus[2];
        evalfp_t connectedRookBonus[MAX_ROOK_CONNECTIONS];

        evalfp_t connectedQueenBonus[MAX_QUEEN_CONNECTIONS];

        evalfp_t kingPawnShieldBonus[COLUMN_COUNT];
        evalfp_t kingPawnStormBonus[ROW_COUNT];
        
        evalfp_t tempo;
    };
    evalfp_t parameters[0];
} evalParameters_fp;

#define sigmoidK(val, K) (1.0 / (1.0 + exp(-val * K / 400.0)))

//https://www.chessprogramming.org/Texel%27s_Tuning_Method
//  - base algorithm, was horribly slow.
//https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf
//  - much faster algorithm, used here.
void Tune(const char* dataPath, const char* outputPath, double forcedK, uint64_t epochs, double max_lr, double min_lr);

#endif