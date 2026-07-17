#ifndef HANDCRAFTED_EVAL
#define HANDCRAFTED_EVAL

#include "board/moves.h"
#include "board/bitboard.h"

#define PHASE_COUNT 2
#define MIDDLEGAME 0
#define ENDGAME 1

#define PARAMETER_COUNT (PHASE_COUNT * PIECE_COUNT / 2   +   PHASE_COUNT * 6 * 64   +   PHASE_COUNT * 6   +   4 * (PHASE_COUNT * 8))
//Externally visible for tuning.
typedef union {
    int parameters[PARAMETER_COUNT];
    struct {
        int genericPieceValues[PHASE_COUNT][PIECE_COUNT / 2];
        int rawPieceTables[PHASE_COUNT][6][64];
        int mobilityBonus[PHASE_COUNT][6];
        int openFileRookBonus[PHASE_COUNT][8];
        int passedPawnBonus[PHASE_COUNT][8];
        int doubledPawnBonus[PHASE_COUNT][8];
        int isolatedPawnBonus[PHASE_COUNT][8];
    };
} evalParameters;

extern evalParameters hce_params; 
extern evalParameters is_param_eg;
extern int gamephasePieceValues[PIECE_COUNT];
extern uint64_t bordering_files[8];

void init_HCE_tables(int hasNewValues);
int evaluatePhasedScore(bitboard* board, int middlegameScore, int endgameScore);
int hce_eval(bitboard* board);

#endif