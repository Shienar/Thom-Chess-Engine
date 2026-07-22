#ifndef HANDCRAFTED_EVAL
#define HANDCRAFTED_EVAL

#include "board/moves.h"
#include "board/bitboard.h"

#define PHASE_COUNT 2
#define MIDDLEGAME 0
#define ENDGAME 1

#define HALF_OPEN_FILE 0
#define OPEN_FILE 1

#define CONNECTED_ROW 0
#define CONNECTED_COLUMN 1
#define CONNECTED_DIAGONAL 2
#define MAX_ROOK_CONNECTIONS 2
#define MAX_QUEEN_CONNECTIONS 3

//Externally visible for tuning.
//Everything is a bonus and everything gets added. Some bonuses happen to be negative.
typedef union {
    struct {
        int genericPieceValues[PHASE_COUNT][PIECE_TYPE_COUNT];
        int rawPieceTables[PHASE_COUNT][PIECE_TYPE_COUNT][64];

        int knightMobilityBonus[PHASE_COUNT][9];
        int bishopMobilityBonus[PHASE_COUNT][14];
        int rookMobilityBonus[PHASE_COUNT][15];
        int queenMobilityBonus[PHASE_COUNT][28];
        int virtualMobilityBonus[PHASE_COUNT][28];
        
        int minorPawnCover[PHASE_COUNT];
        int passedPawnBonus[PHASE_COUNT][ROW_COUNT];
        int connectedPawnBonus[PHASE_COUNT][ROW_COUNT];
        int doubledPawnBonus[PHASE_COUNT][COLUMN_COUNT];
        int isolatedPawnBonus[PHASE_COUNT][COLUMN_COUNT];

        int knightOutputBonus[PHASE_COUNT];

        int bishopPairBonus[PHASE_COUNT];
        int badBishopBonus[PHASE_COUNT];
        
        int openRookFileBonus[PHASE_COUNT][2];
        int connectedRookBonus[PHASE_COUNT][MAX_ROOK_CONNECTIONS];

        int connectedQueenBonus[PHASE_COUNT][MAX_QUEEN_CONNECTIONS];
        
        int tempo[PHASE_COUNT];
    };
    int parameters[0];
} evalParameters;

#define PARAMETER_COUNT (sizeof(evalParameters) / sizeof(int))

extern evalParameters hce_params; 
extern evalParameters is_param_eg;
extern int gamephasePieceValues[PIECE_COUNT];
extern uint64_t bordering_files[COLUMN_COUNT];

void init_HCE_tables();
int evaluatePhasedScore(bitboard* board, int middlegameScore, int endgameScore);
int hce_eval(bitboard* board);

#endif