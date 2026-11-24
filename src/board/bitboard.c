#include "compatibility.h"
#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

uint64_t board_file[COLUMN_COUNT] = {
	(0x0101010101010101ULL << 0),
	(0x0101010101010101ULL << 1),
	(0x0101010101010101ULL << 2),
	(0x0101010101010101ULL << 3),
	(0x0101010101010101ULL << 4),
	(0x0101010101010101ULL << 5),
	(0x0101010101010101ULL << 6),
	(0x0101010101010101ULL << 7)
};

uint64_t board_rank[ROW_COUNT] = {
	(0xFFULL << 0),
	(0xFFULL << 8),
	(0xFFULL << 16),
	(0xFFULL << 24),
	(0xFFULL << 32),
	(0xFFULL << 40),
	(0xFFULL << 48),
	(0xFFULL << 56)
};

uint64_t bordering_files[COLUMN_COUNT] = {
	(0x0101010101010101ULL << 1),
	(0x0101010101010101ULL << 0) | (0x0101010101010101ULL << 2),
	(0x0101010101010101ULL << 1) | (0x0101010101010101ULL << 3),
	(0x0101010101010101ULL << 2) | (0x0101010101010101ULL << 4),
	(0x0101010101010101ULL << 3) | (0x0101010101010101ULL << 5),
	(0x0101010101010101ULL << 4) | (0x0101010101010101ULL << 6),
	(0x0101010101010101ULL << 5) | (0x0101010101010101ULL << 7),
	(0x0101010101010101ULL << 6)
};

void get_square_name(int square, char *target)
{
	assert(target);
	assert(square >= 0 && square <= 63);

	//Output example: e4
	target[0] = COLUMN_NAMES[get_column(square)];
	target[1] = '1' + get_row(square);
	target[2] = '\0';
}

char move_name[5] = {'\0'};
char *null_move_name = "0000";
//Intended to be called as an argument for printf("%s", ...). Can only be safely called once per printf statement.
char *get_move_square_name(move m)
{

	//move 0000
	if (m.start_square == m.end_square)
		return null_move_name;

	get_square_name(m.start_square, move_name);
	get_square_name(m.end_square, &move_name[2]);

	return move_name;
}

int get_square_number(char *square_name)
{
	return ((square_name[0] - 'a') + 8*(square_name[1] - '0' - 1));
}

int pop_lsb(uint64_t *bitboard)
{
	int lsb = __builtin_ctzll(*bitboard);
	*bitboard &= (*bitboard - 1);
	return lsb;
}

int count_repetitions(bitboard *board, repetition_vector *repetitions)
{
	int index = board->half_move_count;
	int min_index = index - board->halfmove_clock;
	int count = 1;
	uint64_t checked_val = repetitions->hash_codes[index];

	for (index = index - 4; index >= min_index; index -= 2) {
		if (checked_val == repetitions->hash_codes[index]) {
			count++;
			if (count >= 3)
				return count;
		}
	}
	return count;
}

int is_draw(bitboard *board, repetition_vector *repetitions)
{
	int repeats = count_repetitions(board, repetitions);
	//Repetition
	if (repeats >= 3)
		return VICTOR_DRAW;
	//50-move rule
	else if (board->halfmove_clock >= 100)
		return VICTOR_DRAW; //Variable stores half-moves
	//Trivial insufficent material
	else if ((board->pieces[BLACK_KING]|board->pieces[WHITE_KING]) == board->pieces_all)
		return VICTOR_DRAW;
	//Other insufficient material
	else {
		//King + Minor Piece vs King
		if (board->pieces_side[BLACK] == board->pieces[BLACK_KING] && board->pieces[WHITE_PAWN] == 0 && board->pieces[WHITE_ROOK] == 0 && board->pieces[WHITE_QUEEN] == 0) {
			if (__builtin_popcountll(board->pieces[WHITE_BISHOP]|board->pieces[WHITE_KNIGHT]) <= 1)
				return VICTOR_DRAW;
		} else if (board->pieces_side[WHITE] == board->pieces[WHITE_KING] && board->pieces[BLACK_PAWN] == 0 && board->pieces[BLACK_ROOK] == 0 && board->pieces[BLACK_QUEEN] == 0) {
			if (__builtin_popcountll(board->pieces[BLACK_BISHOP]|board->pieces[BLACK_KNIGHT]) <= 1)
				return VICTOR_DRAW;
		}
		//King + Bishops vs King + Bishops (Same color bishops)
		else if (board->pieces_all == (board->pieces[WHITE_KING]|board->pieces[WHITE_BISHOP]|board->pieces[BLACK_KING]|board->pieces[BLACK_BISHOP]) &&
				((board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP]) == ((board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP])&LIGHT_SQUARES) ||
				 (board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP]) == ((board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP])&DARK_SQUARES))) {
			return VICTOR_DRAW;
		}
	}

	return VICTOR_NONE;
}

//Only called when it has already been determined that there's no legal moves.
int get_mate_result(bitboard *board)
{
	if (board->turn == WHITE) {
		if (board->in_check)
			return VICTOR_BLACK;
		else
			return VICTOR_DRAW;
	} else {
		if (board->in_check)
			return VICTOR_WHITE;
		else
			return VICTOR_DRAW;
	}
}

void export_fen_from_board(bitboard *board, char *output_fen_string)
{
	assert(board && output_fen_string);

	char rows[8][9] = {'\0'};

	for (int row = 0; row < ROW_COUNT; row++) {
		for (int column = 0; column < COLUMN_COUNT; column++) {
			int column_index = 0;

			for (column_index = 0; column_index < column; column_index++)
				if (rows[row][column_index] == '\0')
					break;

			int piece = (board->piece_arr[row*8 + column]);

			if (piece != EMPTY_PIECE) {
				char piece_char = 0;

				if (ISPAWN(piece))
					piece_char = 'p';
				else if (ISBISHOP(piece))
					piece_char = 'b';
				else if (ISKNIGHT(piece))
					piece_char = 'n';
				else if (ISROOK(piece))
					piece_char = 'r';
				else if (ISQUEEN(piece))
					piece_char = 'q';
				else if (ISKING(piece))
					piece_char = 'k';

				if (ISWHITE(piece))
					piece_char -= 32;
				rows[row][column_index] = piece_char;
			} else {
				if (column_index == 0 || (column_index > 0 && rows[row][column_index - 1] >= 'A' && rows[row][column_index - 1] <= 'z'))
					rows[row][column_index] = '1';
				else if (rows[row][column_index - 1] >= '1' && rows[row][column_index - 1] <= '7')
					rows[row][column_index - 1]++;
			}
		}
	}

	char active_color = (ISWHITE(board->turn)) ? 'w' : 'b';

	char castling_rights[5] = {'\0'};

	castling_rights[0] = '-';
	int write_index = 0;

	if (board->can_kingside_castle_w)
		castling_rights[write_index++] = 'K';
	if (board->can_queenside_castle_w)
		castling_rights[write_index++] = 'Q';
	if (board->can_kingside_castle_b)
		castling_rights[write_index++] = 'k';
	if (board->can_queenside_castle_b)
		castling_rights[write_index] = 'q';

	char ep_square[3] = {'\0'};

	if (board->ep_square == NO_EP_SQUARE)
		ep_square[0] = '-';
	else
		get_square_name(board->ep_square, ep_square);

	sprintf(output_fen_string, "%s/%s/%s/%s/%s/%s/%s/%s %c %s %s %d %d", rows[7], rows[6], rows[5], rows[4], rows[3], rows[2], rows[1], rows[0],
															active_color,
															castling_rights,
															ep_square,
															board->halfmove_clock,
															(int)board->half_move_count/2);
}

void load_fen_string_to_board(bitboard *board, const char *fen_string, repetition_vector *repetitions)
{
	memset(board, 0, sizeof(bitboard));
	assert(board);
	assert(fen_string);

	char full_board_string[80] = {'\0'};
	char *board_strings[8] = {NULL};
	char active_color = '-';
	char castling_availability[5] = {'\0'};
	char en_passant_target_square[3] = {'\0'};
	int half_move_clock, full_move_count = 0;

	sscanf(fen_string, "%s %c %s %s %d %d\n",
		full_board_string,
		&active_color,
		castling_availability,
		en_passant_target_square,
		&half_move_clock,
		&full_move_count);

	char *strtok_ptr = NULL;
	char *cur_board_string = _strtok(full_board_string, "/", &strtok_ptr);
	int cur_index = 7;

	while (cur_board_string && cur_index >= 0) {
		board_strings[cur_index--] = cur_board_string;
		cur_board_string = _strtok(NULL, "/", &strtok_ptr);
	}

	for (int sq = 0; sq < 64; sq++)
		board->piece_arr[sq] = EMPTY_PIECE;
	memset(board->pieces, 0, sizeof(board->pieces));

	for (int pc = 0; pc < PIECE_COUNT; pc++)
		board->pieces[pc] = 0;
	for (int side = 0; side < 2; side++)
		board->pieces_side[side] = 0;

	for (int row = 0; row < ROW_COUNT; row++) {
		if (board_strings[row][0] != '8') {
			int column_offset = 0;
			int index = 0;
			char current_char;

			while ((current_char = board_strings[row][index++]) != '\0' && column_offset < 8) {
				//ASCII values for digits 0-9 (48 to 57 in decimal)
				if (current_char >= '0'  && current_char <= '9') {
					column_offset += current_char - '0';
				} else {
					int square = column_offset + 8*row;

					if (current_char == 'K') {
						board->pieces[WHITE_KING] |= (1ull << (square));
						board->king_square[WHITE] = square;
						board->piece_arr[square] = KING|WHITE;
					} else if (current_char == 'k') {
						board->pieces[BLACK_KING] |= (1ull << (square));
						board->king_square[BLACK] = square;
						board->piece_arr[square] = KING|BLACK;
					} else if (current_char == 'Q') {
						board->pieces[WHITE_QUEEN] |= (1ull << (square));
						board->piece_arr[square] = QUEEN|WHITE;
					} else if (current_char == 'q') {
						board->pieces[BLACK_QUEEN] |= (1ull << (square));
						board->piece_arr[square] = QUEEN|BLACK;
					} else if (current_char == 'R') {
						board->pieces[WHITE_ROOK] |= (1ull << (square));
						board->piece_arr[square] = ROOK|WHITE;
					} else if (current_char == 'r') {
						board->pieces[BLACK_ROOK] |= (1ull << (square));
						board->piece_arr[square] = ROOK|BLACK;
					} else if (current_char == 'N') {
						board->pieces[WHITE_KNIGHT] |= (1ull << (square));
						board->piece_arr[square] = KNIGHT|WHITE;
					} else if (current_char == 'n') {
						board->pieces[BLACK_KNIGHT] |= (1ull << (square));
						board->piece_arr[square] = KNIGHT|BLACK;
					} else if (current_char == 'B') {
						board->pieces[WHITE_BISHOP] |= (1ull << (square));
						board->piece_arr[square] = BISHOP|WHITE;
					} else if (current_char == 'b') {
						board->pieces[BLACK_BISHOP] |= (1ull << (square));
						board->piece_arr[square] = BISHOP|BLACK;
					} else if (current_char == 'P') {
						board->pieces[WHITE_PAWN] |= (1ull << (square));
						board->piece_arr[square] = PAWN|WHITE;
					} else if (current_char == 'p') {
						board->pieces[BLACK_PAWN] |= (1ull << (square));
						board->piece_arr[square] = PAWN|BLACK;
					}
					column_offset++;
				}
			}
		}
	}
	for (int pc = 0; pc < PIECE_COUNT; pc += 2)
		board->pieces_side[WHITE] |= board->pieces[pc];
	for (int pc = 1; pc < PIECE_COUNT; pc += 2)
		board->pieces_side[BLACK] |= board->pieces[pc];
	board->pieces_all = board->pieces_side[BLACK]|board->pieces_side[WHITE];

	//Castling Rights
	if (castling_availability[0] != '-') {
		for (int i = 0; i < 4; i++) {
			if (castling_availability[i] == 'K')
				board->can_kingside_castle_w = 1;
			else if (castling_availability[i] == 'Q')
				board->can_queenside_castle_w = 1;
			else if (castling_availability[i] == 'k')
				board->can_kingside_castle_b = 1;
			else if (castling_availability[i] == 'q')
				board->can_queenside_castle_b = 1;
			else
				break;
		}
	}

	//Turn
	if (active_color == 'w')
		board->turn = WHITE;
	else
		board->turn = BLACK;

	//Check
	if (is_threatened(board, board->king_square[board->turn], board->turn))
		board->in_check = 1;

	//50 move rule
	board->halfmove_clock = half_move_clock;
	board->half_move_count = full_move_count * 2 + board->turn;

	if (en_passant_target_square[0] != '-')
		board->ep_square = get_square_number(en_passant_target_square);
	else
		board->ep_square = NO_EP_SQUARE;

	generate_hash_code(board);

	 //3-fold
	if (repetitions) {
		if (repetitions->hash_codes)
			free(repetitions->hash_codes);

		repetitions->capacity = 16;
		while (repetitions->capacity <= board->half_move_count)
			repetitions->capacity *= 2;

		repetitions->hash_codes = calloc(repetitions->capacity, sizeof(uint64_t));
		repetitions->hash_codes[board->half_move_count] = board->hash_code;
	}
}

void piece_print(char board_array[8][9], uint64_t piece, char print_char)
{
	while (piece) {
		int square = __builtin_ctzll(piece);

		board_array[get_row(square)][get_column(square)] = print_char;
		piece &= (piece - 1);
	}
}

void board_print(bitboard *board)
{
	assert(board);

	int last_move_start_square = -1;
	int last_move_end_square = -1;

	//8 rows x (8 columns of spaces + null terminator)
	char board_array[8][9] = {"		\0", "		\0", "		\0", "		\0", "		\0", "		\0", "		\0", "		\0"};

	piece_print(board_array, board->pieces[WHITE_PAWN], 'p');
	piece_print(board_array, board->pieces[BLACK_PAWN], 'P');

	piece_print(board_array, board->pieces[WHITE_ROOK], 'r');
	piece_print(board_array, board->pieces[BLACK_ROOK], 'R');

	piece_print(board_array, board->pieces[WHITE_KNIGHT], 'n');
	piece_print(board_array, board->pieces[BLACK_KNIGHT], 'N');

	piece_print(board_array, board->pieces[WHITE_BISHOP], 'b');
	piece_print(board_array, board->pieces[BLACK_BISHOP], 'B');

	piece_print(board_array, board->pieces[WHITE_QUEEN], 'q');
	piece_print(board_array, board->pieces[BLACK_QUEEN], 'Q');

	piece_print(board_array, board->pieces[WHITE_KING], 'k');
	piece_print(board_array, board->pieces[BLACK_KING], 'K');

	printf(TEXT_BOLD "\t\t%% - - - - - - - - - - %%\n\t\t|					 |\n" TEXT_NONE);
	for (int row = 7; row >= 0; row--) {
		printf(TEXT_BOLD "\t\t%d   ", row+1);
		for (int column = 0; column <= 7; column++) {
			if (board_array[row][column] < 91 && board_array[row][column] > 64) {
				if (8 * row + column == last_move_start_square)
					printf(TEXT_COLOR_SOURCE_BLACK_PIECE);
				else if (8 * row + column == last_move_end_square)
					printf(TEXT_COLOR_DESTINATION_BLACK_PIECE);
				else {
					if ((column + row)&1)
						printf(TEXT_COLOR_LIGHT_SQUARE_BLACK_PIECE);
					else
						printf(TEXT_COLOR_DARK_SQUARE_BLACK_PIECE);
				}


				board_array[row][column] = board_array[row][column]+32;
				printf("%c ", board_array[row][column]);
			} else {
				if (8 * row + column == last_move_start_square)
					printf(TEXT_COLOR_SOURCE_WHITE_PIECE);
				else if (8 * row + column == last_move_end_square)
					printf(TEXT_COLOR_DESTINATION_WHITE_PIECE);
				else {
					if ((column + row)&1)
						printf(TEXT_COLOR_LIGHT_SQUARE_WHITE_PIECE);
					else
						printf(TEXT_COLOR_DARK_SQUARE_WHITE_PIECE);
				}

				printf("%c ", board_array[row][column]);
			}
		}
		printf(TEXT_NONE TEXT_BOLD"  |" TEXT_NONE "\n ");
	}
	printf(TEXT_BOLD "\t\t|					 |\n \t\t%% - a b c d e f g h - %%\n" TEXT_NONE);

	char fen[128] = {'\0'};

	export_fen_from_board(board, fen);
	printf("fen: %s\n", fen);
	fflush(stdout);
}

void bitmask_print(uint64_t mask, char fill)
{
	char board_array[8][9] = {"		\0", "		\0", "		\0", "		\0", "		\0", "		\0", "		\0", "		\0"};

	piece_print(board_array, mask, fill);
	printf("\nMask: 0x%016" PRIx64 "\n", mask);
	printf(TEXT_BOLD "\t\t%% - - - - - - - - - - %%\n\t\t|					 |\n" TEXT_NONE);
	for (int row = 7; row >= 0; row--) {
		printf(TEXT_BOLD "\t\t%d   ", row+1);
		for (int column = 0; column <= 7; column++) {
			if (board_array[row][column] < 91 && board_array[row][column] > 64) {
				if ((column + row)&1)
					printf(TEXT_COLOR_LIGHT_SQUARE_BLACK_PIECE);
				else
					printf(TEXT_COLOR_DARK_SQUARE_BLACK_PIECE);

				board_array[row][column] = board_array[row][column]+32;
				printf("%c ", board_array[row][column]);
			} else {
				if ((column + row)&1)
					printf(TEXT_COLOR_LIGHT_SQUARE_WHITE_PIECE);
				else
					printf(TEXT_COLOR_DARK_SQUARE_WHITE_PIECE);

				printf("%c ", board_array[row][column]);
			}
		}
		printf(TEXT_NONE TEXT_BOLD"  |" TEXT_NONE "\n ");
	}
	printf(TEXT_BOLD "\t\t|					 |\n \t\t%% - a b c d e f g h - %%\n" TEXT_NONE);
}
