// Shared datatypes between board.* and moves.*

#pragma once

#include <stdint.h>

#define MAX_MOVES 218
#define PIECE_COUNT 12
#define PIECE_TYPE_COUNT 6

typedef union move {
	uint16_t raw;
	struct {
		uint16_t start_square : 6;
		uint16_t end_square   : 6;
		uint16_t promote_to   : 4;
	};
} move;

typedef struct move_iterator {
	move move_list[MAX_MOVES];
	int16_t move_scores[MAX_MOVES];
	uint8_t count;

	int16_t selected_index;
	uint8_t quiet_index;
	uint8_t bad_capture_index;
	uint8_t bad_capture_count;

	uint8_t visited_good;
	uint8_t visited_quiet;
	uint8_t visited_bad;

	struct bitboard *board;

	uint8_t stage;
	uint8_t type;

	move excluded_move;
	move tt_move;
	move killer1_move;
	move killer2_move;
	move countermove;
	move followup_move;

	struct thread_context *context;
} move_iterator;

typedef struct repetition_vector {
	uint64_t *hash_codes;
	int capacity;
} repetition_vector;

//sq = 8 * row + column; row, column in [0, 7]
typedef struct bitboard {
	uint64_t hash_code;
	uint64_t pawn_hash;
	uint64_t nonpawn_hash[2];

	uint64_t pieces[PIECE_COUNT];
	uint64_t pieces_side[2];
	uint64_t pieces_all;

	uint16_t half_move_count;
	uint16_t plies_from_null;

	uint8_t piece_arr[64];
	uint8_t king_square[2];
	uint8_t halfmove_clock;
	int8_t ep_square;

	uint8_t can_kingside_castle_w  : 1;
	uint8_t can_queenside_castle_w : 1;
	uint8_t can_kingside_castle_b  : 1;
	uint8_t can_queenside_castle_b : 1;
	uint8_t in_check			 : 1;
	uint8_t turn				 : 1;
} bitboard;
