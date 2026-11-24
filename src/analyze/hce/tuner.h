#pragma once

#include "analyze/hce/hce.h"
#include "board/bitboard.h"

typedef struct phased_eval_fp {
	double mg;
	double eg;
} phased_eval_fp;

#define UPDATE_COEFFICIENTS(var, val, op, index) 	\
	do { 											\
		(coefficients->var)index.mg op val; 		\
		(coefficients->var)index.eg op val; 		\
		score->mg op val * (params.var)index.mg; 	\
		score->eg op val * (params.var)index.eg; 	\
	} while (0)

typedef struct tuning_tuple {
	int index;
	int coefficient; //White - black
} tuning_tuple;

typedef struct tuning_entry {
	double heuristic_eval;
	phased_eval_fp score;
	phased_eval phase;
	double result;
	phased_eval_fp phase_factors;
	tuning_tuple *active_tuples;
	int active_tuple_count;
} tuning_entry;

typedef union eval_parameters_fp {
	struct {
		phased_eval_fp generic_piece_values[PIECE_TYPE_COUNT];
		phased_eval_fp raw_piece_tables[PIECE_TYPE_COUNT][64];

		phased_eval_fp knight_mobility_bonus[9];
		phased_eval_fp bishop_mobility_bonus[14];
		phased_eval_fp rook_mobility_bonus[15];
		phased_eval_fp queen_mobility_bonus[28];
		phased_eval_fp virtual_mobility_bonus[28];

		phased_eval_fp minor_pawn_cover;
		phased_eval_fp passed_pawn_bonus[ROW_COUNT];
		phased_eval_fp connected_pawn_bonus[ROW_COUNT];
		phased_eval_fp neighbor_pawn_bonus[ROW_COUNT];
		phased_eval_fp doubled_pawn_bonus[COLUMN_COUNT];
		phased_eval_fp isolated_pawn_bonus[COLUMN_COUNT];

		phased_eval_fp knight_outpost_bonus;

		phased_eval_fp bishop_pair_bonus;
		phased_eval_fp bad_bishop_bonus;

		phased_eval_fp open_rook_file_bonus[2];
		phased_eval_fp connected_rook_bonus[MAX_ROOK_CONNECTIONS];

		phased_eval_fp connected_queen_bonus[MAX_QUEEN_CONNECTIONS];

		phased_eval_fp king_shield_bonus[COLUMN_COUNT];
		phased_eval_fp king_storm_bonus[ROW_COUNT];
		phased_eval_fp open_king_file[2];
		phased_eval_fp king_safety[100];

		phased_eval_fp tempo;
	};
	phased_eval_fp parameters[0];
} eval_parameters_fp;

#define sigmoidK(val, K) (1.0 / (1.0 + exp(-val * K / 400.0)))

//https://www.chessprogramming.org/Texel%27s_Tuning_Method
//  - base algorithm, was horribly slow.
//https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf
//  - much faster algorithm, used here.
void tune(const char *data_path, const char *output_path, double forced_k, uint64_t epochs, double max_lr, double min_lr);
