#pragma once

#include <stdint.h>
#include "board/bitboard.h"

#define SEMI_OPEN_FILE 0
#define OPEN_FILE 1

#define CONNECTED_ROW 0
#define CONNECTED_COLUMN 1
#define CONNECTED_DIAGONAL 2
#define MAX_ROOK_CONNECTIONS 2
#define MAX_QUEEN_CONNECTIONS 3

#define KNIGHT_ATTACK_WEIGHT 2
#define BISHOP_ATTACK_WEIGHT 2
#define ROOK_ATTACK_WEIGHT 3
#define QUEEN_ATTACK_WEIGHT 5

typedef struct phased_eval {
	int32_t mg;
	int32_t eg;
} phased_eval;

#define P(mg, eg) {mg, eg}

static inline void eval_add(phased_eval *dest, phased_eval val)
{
	dest->mg += val.mg;
	dest->eg += val.eg;
}

static inline void eval_madd(phased_eval *dest, phased_eval val, int mul)
{
	dest->mg += mul * val.mg;
	dest->eg += mul * val.eg;
}

static inline void eval_sub(phased_eval *dest, phased_eval val)
{
	dest->mg -= val.mg;
	dest->eg -= val.eg;
}

typedef struct eval_context {
	uint64_t king_zone[2];
	int32_t attack_weight[2];
} eval_context;

//Externally visible for tuning.
//Everything is a bonus and everything gets added. Some bonuses happen to be negative.
typedef union eval_parameters {
	struct {
		phased_eval generic_piece_values[PIECE_TYPE_COUNT];
		phased_eval raw_piece_tables[PIECE_TYPE_COUNT][64];

		phased_eval knight_mobility_bonus[9];
		phased_eval bishop_mobility_bonus[14];
		phased_eval rook_mobility_bonus[15];
		phased_eval queen_mobility_bonus[28];
		phased_eval virtual_mobility_bonus[28];

		phased_eval minor_pawn_cover;
		phased_eval passed_pawn_bonus[ROW_COUNT];
		phased_eval connected_pawn_bonus[ROW_COUNT];
		phased_eval neighbor_pawn_bonus[ROW_COUNT];
		phased_eval doubled_pawn_bonus[COLUMN_COUNT];
		phased_eval isolated_pawn_bonus[COLUMN_COUNT];

		phased_eval knight_outpost_bonus;

		phased_eval bishop_pair_bonus;
		phased_eval bad_bishop_bonus;

		phased_eval open_rook_file_bonus[2];
		phased_eval connected_rook_bonus[MAX_ROOK_CONNECTIONS];

		phased_eval connected_queen_bonus[MAX_QUEEN_CONNECTIONS];

		phased_eval king_shield_bonus[COLUMN_COUNT];
		phased_eval king_storm_bonus[ROW_COUNT];
		phased_eval open_king_file[2];
		phased_eval king_safety[100];

		phased_eval tempo;
	};
	phased_eval parameters[0];
} eval_parameters;

#define PARAMETER_COUNT (sizeof(eval_parameters) / sizeof(phased_eval))

extern eval_parameters hce_params;
extern int gamephase_piece_values[PIECE_COUNT];
extern uint64_t king_shield_mask[2][COLUMN_COUNT];
extern uint64_t king_storm_mask[COLUMN_COUNT];
extern uint64_t king_zone[2][64];
extern int attack_weight[8];

void init_hce_tables(void);
int evaluate_phased_score(bitboard *board, phased_eval eval);
int hce_eval(bitboard *board);
