#ifndef HANDCRAFTED_EVAL
#define HANDCRAFTED_EVAL

#include "board/moves.h"
#include "board/bitboard.h"

#define SEMI_OPEN_FILE 0
#define OPEN_FILE 1

#define CONNECTED_ROW 0
#define CONNECTED_COLUMN 1
#define CONNECTED_DIAGONAL 2
#define MAX_ROOK_CONNECTIONS 2
#define MAX_QUEEN_CONNECTIONS 3

#define ATTACKING_KNIGHT 0
#define ATTACKING_BISHOP 1 
#define ATTACKING_ROOK 2
#define ATTACKING_QUEEN 3
#define PAWN_ATTACK_TYPES 4

typedef struct {
    int32_t mg;
    int32_t eg;
} eval_t;

#define P(mg, eg) {mg, eg}

#define EVAL_ADD(dest, val) \
    do { \
        (dest).mg += (val).mg; \
        (dest).eg += (val).eg; \
    } while(0)
    
#define EVAL_MADD(dest, val, mul) \
    do { \
        (dest).mg += mul * (val).mg; \
        (dest).eg += mul * (val).eg; \
    } while(0)

#define EVAL_SUB(dest, val) \
    do { \
        (dest).mg -= (val).mg; \
        (dest).eg -= (val).eg; \
    } while(0)

//Externally visible for tuning.
//Everything is a bonus and everything gets added. Some bonuses happen to be negative.
typedef union {
    struct {
        eval_t genericPieceValues[PIECE_TYPE_COUNT];
        eval_t rawPieceTables[PIECE_TYPE_COUNT][64];

        eval_t knightMobilityBonus[9];
        eval_t bishopMobilityBonus[14];
        eval_t rookMobilityBonus[15];
        eval_t queenMobilityBonus[28];
        eval_t virtualMobilityBonus[28];
        
        eval_t pawnAttacks[PAWN_ATTACK_TYPES];
        eval_t minorPawnCover;
        eval_t passedPawnBonus[ROW_COUNT];
        eval_t connectedPawnBonus[ROW_COUNT];
        eval_t doubledPawnBonus[COLUMN_COUNT];
        eval_t isolatedPawnBonus[COLUMN_COUNT];

        eval_t knightOutpostBonus;

        eval_t bishopPairBonus;
        eval_t badBishopBonus;
        
        eval_t openRookFileBonus[2];
        eval_t connectedRookBonus[MAX_ROOK_CONNECTIONS];

        eval_t connectedQueenBonus[MAX_QUEEN_CONNECTIONS];

        eval_t kingPawnShieldBonus[COLUMN_COUNT];
        eval_t kingPawnStormBonus[ROW_COUNT];
        
        eval_t tempo;
    };
    eval_t parameters[0];
} evalParameters;

#define PARAMETER_COUNT (sizeof(evalParameters) / sizeof(eval_t))

extern evalParameters hce_params; 
extern evalParameters is_param_eg;
extern int gamephasePieceValues[PIECE_COUNT];
extern uint64_t kingPawnShieldMask[2][COLUMN_COUNT];
extern uint64_t kingPawnStormMask[COLUMN_COUNT];

void init_HCE_tables();
int evaluatePhasedScore(bitboard* board, eval_t eval);
int hce_eval(bitboard* board);

#endif