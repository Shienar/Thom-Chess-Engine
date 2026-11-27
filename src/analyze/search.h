#pragma once

#include <time.h>
#include "transposition/transpositiontable.h"
#include "analyze/nnue/accumulator.h"
#include "analyze/history.h"
#include "board/board.h"
#include "compatibility.h"

extern uint8_t abort_flag;
extern volatile uint8_t is_ponder;

#ifdef SEARCHINFO
#define RECORD_SEARCH(x) do { x } while (0)
#else
#define RECORD_SEARCH(x)
#endif

extern int thread_count;
extern int enable_ponder;
extern int is_searching;
extern int suppress_uci_messages;

#define MIN_THREADS 1
#define MAX_THREADS 64
#define MAX_PLY 60

#define SCORE_WIN 32000
#define MIN_MATE_SCORE (SCORE_WIN - MAX_PLY)

typedef struct principal_variation {
	int length;
	move line[MAX_PLY];
	uint64_t hash_codes[MAX_PLY];
} principal_variation;

#define MAX_REQUIRED_MOVES 32
typedef struct thread_context {
	//UCI Thread settings or info
	int max_depth, seldepth, completed_depth, deepening_skip;
	int hard_max_nodes, soft_max_nodes, counted_nodes;
	clock_t start_time, soft_end_time, hard_end_time;
	move searched_moves[MAX_REQUIRED_MOVES];
	uint8_t *abort_flag;

	//Necessary search information
	int16_t score;
	bitboard board_stack[MAX_PLY];
	move move_stack[MAX_PLY];
	repetition_vector repetitions;
	hashtable_tt *tt;
	principal_variation pv;
	move excluded_move[MAX_PLY];

	accumulator *accumulator_stack;
	uint8_t is_acc_clean[MAX_PLY];
	uint8_t last_clean_ply;
	accumulator_refresh_table *refresh_table;

	//Improving heuristic
	int eval_history[MAX_PLY];
	int8_t improving[MAX_PLY]; // 0 or 1
	int8_t worsening[MAX_PLY]; // >= 0

	//Double/Triple extension tracking.
	uint8_t extensions[MAX_PLY];

	//Killer heuristic
	move killer_moves[MAX_PLY][2];

	//History heuristic
	int16_t history_table[2][6][64];
	int16_t capture_history_table[6][64][6];

	//Continuation heuristics
	move counter_move[2][6][64];
	move followup_move[2][6][64];

	//Correction History
	int16_t pawn_corrhist[2][CORRHIST_SIZE]; //[turn]
	int16_t nonpawn_corrhist[2][2][CORRHIST_SIZE]; //[color][turn]

	#ifdef SEARCHINFO
	union {
		struct {
			uint64_t pvs_nodes;
			uint64_t qs_nodes;

			uint64_t tt_hits;
			uint64_t tt_cutoffs;
			uint64_t tt_misses;

			uint64_t quiescent_searched_moves;
			uint64_t quiescent_searched_positions;

			uint64_t pvs_searched_moves;
			uint64_t pvs_searched_positions;

			uint64_t extension_overflow;
			uint64_t singular_searches;
			uint64_t triple_extensions;
			uint64_t double_extensions;
			uint64_t single_extensions;
			uint64_t multicut_prunes;
			uint64_t negative_extensions;
			uint64_t failed_extensions;

			uint64_t evaluations;

			uint64_t stable_reductions;
			uint64_t worsening_reductions;
			uint64_t tt_reductions;
			uint64_t rfp_prunes;
			uint64_t razoring_prunes;
			uint64_t failed_razoring_prunes;
			uint64_t failed_nmp_prunes;
			uint64_t nmp_prunes;
		};

		uint8_t raw_search_data[24 * sizeof(uint64_t)];
	};
	#endif
} thread_context;

extern int initial_aspiration_margin;
extern float aspiration_margin_mult_factor;

extern int razoring_a;
extern int razoring_b;

extern int futility_margin;
extern int futility_depth_margin;

extern int reverse_futility_margin;
extern int reverse_futility_margin_improving;

extern float lmr_a;
extern float lmr_b;

extern float lmp_a;
extern float lmp_b;
extern float lmp_improving_a;
extern float lmp_improving_b;

extern int stable_eval_margin;

void init_search_tables(void);
uint64_t perft(bitboard *board, int depth, int verbose);
uint64_t iterator_perft(thread_context *context, int depth, int ply, int verbose);
int quiescent_search(thread_context *context, int alpha, int beta, int ply, int pv_node);
int principal_variation_search(thread_context *context, int alpha, int beta, int depth, int ply, principal_variation *my_pv, int pv_node, int cut_node);
THREAD_RETURN calculate_best_move(THREAD_PARAM param);
