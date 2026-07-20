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
        double rawPieceTables[PHASE_COUNT][PIECE_TYPE_COUNT][64];

        double knightMobilityBonus[PHASE_COUNT][9];
        double bishopMobilityBonus[PHASE_COUNT][14];
        double rookMobilityBonus[PHASE_COUNT][15];
        double queenMobilityBonus[PHASE_COUNT][28];
        
        double virtualMobilityBonus[PHASE_COUNT][28];

        double bishopPairBonus[PHASE_COUNT];
        double openFileRookBonus[PHASE_COUNT][COLUMN_COUNT];

        double passedPawnBonus[PHASE_COUNT][ROW_COUNT];
        double doubledPawnBonus[PHASE_COUNT][COLUMN_COUNT];
        double isolatedPawnBonus[PHASE_COUNT][COLUMN_COUNT];
        
        double tempo[PHASE_COUNT];
    };
    double parameters[0];
} evalParameters_fp;

#define sigmoidK(val, K) (1.0 / (1.0 + exp(-val * K / 400.0)))

//https://www.chessprogramming.org/Texel%27s_Tuning_Method
//  - base algorithm, was horribly slow.
//https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf
//  - much faster algorithm, used here.
void Tune(const char* dataPath, const char* outputPath, double forcedK, uint64_t epochs, double max_lr, double min_lr);

#endif