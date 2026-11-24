#pragma once

#include "compatibility.h"
#include "board/bitboard.h"
#include <stdio.h>
#include <time.h>

//Reusing the format from https://github.com/cosmobobak/viriformat

//White-relative always.
typedef int16_t viri_score;

#define VIRI_PAWN 0
#define VIRI_KNIGHT 1
#define VIRI_BISHOP 2
#define VIRI_ROOK 3
#define VIRI_QUEEN 4
#define VIRI_KING 5
#define VIRI_UNMOVED_ROOK 6
#define VIRI_BLACK_PIECE 8
#define VIRI_BLACK_STM 1
#define VIRI_BLACK_WIN 0
#define VIRI_DRAW 1
#define VIRI_WHITE_WIN 2

#define VIRI_MOVE_TYPE_NONE 0
#define VIRI_MOVE_TYPE_ENPASSANT 1
#define VIRI_MOVE_TYPE_CASTLING 2
#define VIRI_MOVE_TYPE_PROMOTIONS 3 //includes captures
#define VIRI_PROMOTETO_KNIGHT 0
#define VIRI_PROMOTETO_BISHOP 1
#define VIRI_PROMOTETO_ROOK 2
#define VIRI_PROMOTETO_QUEEN 3

#define BOARD_SAVED 0
#define NO_BOARD_END_OF_FILE -1
#define NO_BOARD_CORRUPTED -2

#define CORRUPTED_POSITION -2

#pragma pack(push, 1)
typedef struct viri_packed_board {
	uint64_t occupancy;
	uint8_t pieces[16];
	uint8_t ep : 7;
	uint8_t stm  : 1;
	uint8_t halfmove_clock; //50-move rule tracking
	uint16_t full_move_counter;
	viri_score white_score;
	uint8_t result;
	uint8_t padding;
} viri_packed_board;
typedef union viri_move {
	uint16_t raw;
	struct {
		uint16_t start_square : 6;
		uint16_t end_square : 6;
		uint16_t promote_piece : 2;
		uint16_t move_type : 2;
	};
} viri_move;

typedef union viri_move_score_pair {
	uint32_t raw;
	struct {
		viri_move move;
		viri_score white_score;
	};
} viri_move_score_pair;
#pragma pack(pop)

typedef struct reader_details {
	bitboard *board;
	viri_packed_board *packed_board;
	uint8_t current_game_winner;

	uint8_t *current_ptr;
	uint8_t *game_start_ptr;

	uint64_t position_index;
	uint64_t game_index;

	viri_move_score_pair next_pair;
} reader_details;

typedef struct binpack_details {
	uint8_t is_reader;
	reader_details *reader_info;

	//Reading (Global)
	mmap_handle_t mmap_file;
	uint8_t *start_ptr;
	uint8_t *end_ptr;

	//Writing
	FILE *binpack;
	mutex_t lock;
	int written_this_session;
	clock_t last_print_time;
	clock_t start_time;
} binpack_details;

//Avoids some thread contention for writes after each individual game.
#define MAX_POSITIONS_PER_GAME 100
#define VIRI_WRITEBUFFER_SIZE 32
typedef struct generated_game_buffer {
	viri_packed_board packed_boards[VIRI_WRITEBUFFER_SIZE];
	viri_move_score_pair pair_lists[VIRI_WRITEBUFFER_SIZE][MAX_POSITIONS_PER_GAME];
	int moves_this_game[VIRI_WRITEBUFFER_SIZE];
} generated_game_buffer;

extern uint8_t piece_mappings_from_viri[16];
extern uint8_t piece_mappings_to_viri[16];
extern uint8_t promote_mappings_from_viri[4];
extern uint8_t promote_mappings_to_viri[10];
extern uint8_t rook_sq_to_flag[64];

//Same file pointer for all functions. The assumption is that you will either only read or only write.
void binpack_open(binpack_details *details, const char *file_name, int is_reader);
void binpack_close(binpack_details *details);
int binpack_next(binpack_details *details, bitboard *brd, viri_score *eval, uint8_t *result, int loop, int smart_fen_skip, int minimum_fen_skips);

int read_packed_board(binpack_details *details);
void board_to_packed_board(bitboard *board, viri_packed_board *packed_board);
void binpack_write(binpack_details *details, generated_game_buffer *buffer);

void binpack_print_info(const char *file_name);
void binpack_clean(const char *file_name);
