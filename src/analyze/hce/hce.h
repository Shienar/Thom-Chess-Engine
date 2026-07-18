#ifndef HANDCRAFTED_EVAL
#define HANDCRAFTED_EVAL

#include "board/moves.h"
#include "board/bitboard.h"

#define PHASE_COUNT 2
#define MIDDLEGAME 0
#define ENDGAME 1

//Externally visible for tuning.
typedef union {
    struct {
        int genericPieceValues[PHASE_COUNT][PIECE_TYPE_COUNT];
        int rawPieceTables[PHASE_COUNT][PIECE_TYPE_COUNT][64];
        int tempo[PHASE_COUNT];
        int virtualMobilityBonus[PHASE_COUNT];
        int kingThreats[PHASE_COUNT][PIECE_TYPE_COUNT];
        int mobilityBonus[PHASE_COUNT][6];
        int bishopPairBonus[PHASE_COUNT];
        int openFileRookBonus[PHASE_COUNT][8];
        int passedPawnBonus[PHASE_COUNT][8];
        int doubledPawnBonus[PHASE_COUNT][8];
        int isolatedPawnBonus[PHASE_COUNT][8];
    };
    int parameters[0];
} evalParameters;

#define PARAMETER_COUNT (sizeof(evalParameters) / sizeof(int))

extern evalParameters hce_params; 
extern evalParameters is_param_eg;
extern int gamephasePieceValues[PIECE_COUNT];
extern uint8_t manhattanDistance[64][64];
extern uint64_t bordering_files[8];

void init_HCE_tables();
int evaluatePhasedScore(bitboard* board, int middlegameScore, int endgameScore);
int hce_eval(bitboard* board);

#endif