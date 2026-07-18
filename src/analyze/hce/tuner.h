#ifndef TEXEL_TUNER
#define TEXEL_TUNER

#include "analyze/hce/hce.h"

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8f

#define PI 3.141592653589793

typedef struct tuningTuple {
    int index;
    int coefficient; //White - black
} tuningTuple;

typedef struct tuningEntry {
    double heuristic_eval;
    double eg_score;
    double mg_score;
    int phase[PHASE_COUNT];
    double result;
    double phaseFactors[PHASE_COUNT];
    tuningTuple* activeTuples;
    int activeTupleCount;
} tuningEntry;

typedef union {
    struct {
        double genericPieceValues[PHASE_COUNT][PIECE_TYPE_COUNT];
        double rawPieceTables[PHASE_COUNT][6][64];
        double tempo[PHASE_COUNT];
        double virtualMobilityBonus[PHASE_COUNT];
        double kingThreats[PHASE_COUNT][PIECE_TYPE_COUNT];
        double mobilityBonus[PHASE_COUNT][6];
        double bishopPairBonus[PHASE_COUNT];
        double openFileRookBonus[PHASE_COUNT][8];
        double passedPawnBonus[PHASE_COUNT][8];
        double doubledPawnBonus[PHASE_COUNT][8];
        double isolatedPawnBonus[PHASE_COUNT][8];
    };
    double parameters[0];
} evalParameters_fp;

#define sigmoidK(val, K) (1.0 / (1.0 + exp(-val * K / 400.0)))

//https://www.chessprogramming.org/Texel%27s_Tuning_Method
//  - base algorithm, was horribly slow.
//https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf
//  - much faster algorithm, used here.
void Tune(const char* dataPath, const char* outputPath, uint64_t epochs, double max_lr, double min_lr);

#endif