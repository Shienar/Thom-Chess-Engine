#include "compatibility.h"
#include "binpack/viri_binpack.h"
#include "debug.h"
#include "compatibility.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
//Readers only.
int mmap_open(const char *filename, mmap_handle_t *handle)
{
	handle->data = NULL;
	handle->size = 0;

	handle->file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
	if (handle->file_handle == INVALID_HANDLE_VALUE)
		return -1;

	LARGE_INTEGER file_size;

	if (!GetFileSizeEx(handle->file_handle, &file_size)) {
		CloseHandle(handle->file_handle);
		return -1;
	}
	handle->size = (size_t)file_size.QuadPart;

	handle->mapping_handle = CreateFileMappingA(handle->file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
	if (!handle->mapping_handle) {
		CloseHandle(handle->file_handle);
		return -1;
	}

	handle->data = MapViewOfFile(handle->mapping_handle, FILE_MAP_READ, 0, 0, 0);
	if (!handle->data) {
		CloseHandle(handle->mapping_handle);
		CloseHandle(handle->file_handle);
		return -1;
	}
	return 0;
}

void mmap_close(mmap_handle_t *handle)
{
	if (!handle->data)
		return;
	UnmapViewOfFile(handle->data);
	CloseHandle(handle->mapping_handle);
	CloseHandle(handle->file_handle);
	handle->data = NULL;
}
#else
int mmap_open(const char *filename, mmap_handle_t *handle)
{
	handle->data = NULL;
	handle->size = 0;

	handle->fd = open(filename, O_RDONLY);
	if (handle->fd == -1)
		return -1;

	struct stat sb;

	if (fstat(handle->fd, &sb) == -1) {
		close(handle->fd);
		return -1;
	}
	handle->size = (size_t)sb.st_size;

	handle->data = mmap(NULL, handle->size, PROT_READ, MAP_SHARED, handle->fd, 0);
	if (handle->data == MAP_FAILED) {
		close(handle->fd);
		return -1;
	}
	madvise(handle->data, handle->size, MADV_RANDOM);

	return 0;
}

void mmap_close(mmap_handle_t *handle)
{
	if (!handle->data)
		return;
	munmap(handle->data, handle->size);
	close(handle->fd);
	handle->data = NULL;
}

#endif

uint8_t piece_mappings_from_viri[16];
uint8_t piece_mappings_to_viri[16];
uint8_t promote_mappings_from_viri[4];
uint8_t promote_mappings_to_viri[10];
int init = 0;

void init_viri_tables(void)
{
	if (init)
		return;
	init = 1;

	piece_mappings_from_viri[VIRI_PAWN] = WHITE_PAWN;
	piece_mappings_from_viri[VIRI_PAWN | VIRI_BLACK_PIECE] = BLACK_PAWN;

	piece_mappings_from_viri[VIRI_KNIGHT] = WHITE_KNIGHT;
	piece_mappings_from_viri[VIRI_KNIGHT | VIRI_BLACK_PIECE] = BLACK_KNIGHT;

	piece_mappings_from_viri[VIRI_BISHOP] = WHITE_BISHOP;
	piece_mappings_from_viri[VIRI_BISHOP | VIRI_BLACK_PIECE] = BLACK_BISHOP;

	piece_mappings_from_viri[VIRI_ROOK] = WHITE_ROOK;
	piece_mappings_from_viri[VIRI_ROOK | VIRI_BLACK_PIECE] = BLACK_ROOK;

	piece_mappings_from_viri[VIRI_UNMOVED_ROOK] = WHITE_ROOK;
	piece_mappings_from_viri[VIRI_UNMOVED_ROOK | VIRI_BLACK_PIECE] = BLACK_ROOK;

	piece_mappings_from_viri[VIRI_QUEEN] = WHITE_QUEEN;
	piece_mappings_from_viri[VIRI_QUEEN | VIRI_BLACK_PIECE] = BLACK_QUEEN;

	piece_mappings_from_viri[VIRI_KING] = WHITE_KING;
	piece_mappings_from_viri[VIRI_KING | VIRI_BLACK_PIECE] = BLACK_KING;

	piece_mappings_to_viri[WHITE_PAWN] = VIRI_PAWN;
	piece_mappings_to_viri[BLACK_PAWN] = VIRI_PAWN | VIRI_BLACK_PIECE;

	piece_mappings_to_viri[WHITE_KNIGHT] = VIRI_KNIGHT;
	piece_mappings_to_viri[BLACK_KNIGHT] = VIRI_KNIGHT | VIRI_BLACK_PIECE;

	piece_mappings_to_viri[WHITE_BISHOP] = VIRI_BISHOP;
	piece_mappings_to_viri[BLACK_BISHOP] = VIRI_BISHOP | VIRI_BLACK_PIECE;

	piece_mappings_to_viri[WHITE_ROOK] = VIRI_ROOK;
	piece_mappings_to_viri[BLACK_ROOK] = VIRI_ROOK | VIRI_BLACK_PIECE;

	piece_mappings_to_viri[WHITE_QUEEN] = VIRI_QUEEN;
	piece_mappings_to_viri[BLACK_QUEEN] = VIRI_QUEEN | VIRI_BLACK_PIECE;

	piece_mappings_to_viri[WHITE_KING] = VIRI_KING;
	piece_mappings_to_viri[BLACK_KING] = VIRI_KING | VIRI_BLACK_PIECE;

	promote_mappings_from_viri[VIRI_PROMOTETO_KNIGHT] = KNIGHT;
	promote_mappings_from_viri[VIRI_PROMOTETO_BISHOP] = BISHOP;
	promote_mappings_from_viri[VIRI_PROMOTETO_ROOK] = ROOK;
	promote_mappings_from_viri[VIRI_PROMOTETO_QUEEN] = QUEEN;

	promote_mappings_to_viri[0] = 0;
	promote_mappings_to_viri[WHITE_KNIGHT] = VIRI_PROMOTETO_KNIGHT;
	promote_mappings_to_viri[BLACK_KNIGHT] = VIRI_PROMOTETO_KNIGHT;
	promote_mappings_to_viri[WHITE_BISHOP] = VIRI_PROMOTETO_BISHOP;
	promote_mappings_to_viri[BLACK_BISHOP] = VIRI_PROMOTETO_BISHOP;
	promote_mappings_to_viri[WHITE_ROOK] = VIRI_PROMOTETO_ROOK;
	promote_mappings_to_viri[BLACK_ROOK] = VIRI_PROMOTETO_ROOK;
	promote_mappings_to_viri[WHITE_QUEEN] = VIRI_PROMOTETO_QUEEN;
	promote_mappings_to_viri[BLACK_QUEEN] = VIRI_PROMOTETO_QUEEN;
}

//Doesn't set white_score or game result, that can be done later by the caller.
void board_to_packed_board(bitboard *board, viri_packed_board *packed_board)
{
	packed_board->occupancy = board->pieces_all;

	memset(packed_board->pieces, 0, 16 * sizeof(uint8_t));
	uint64_t mask = packed_board->occupancy;
	int insert_index = 0;

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int pc = (board->piece_arr[sq]);
		int piece = piece_mappings_to_viri[pc];

		if (ISROOK(pc)) {
			if (ISWHITE(pc) &&
				get_row(sq) == get_row(board->king_square[WHITE]) &&
				((board->can_kingside_castle_w && sq > board->king_square[WHITE]) ||
				 (board->can_queenside_castle_w && sq < board->king_square[WHITE])))
					piece = VIRI_UNMOVED_ROOK;
			else if (ISBLACK(pc) &&
					get_row(sq) == get_row(board->king_square[BLACK]) &&
					((board->can_kingside_castle_b && sq > board->king_square[BLACK]) ||
					 (board->can_queenside_castle_b && sq < board->king_square[BLACK])))
						piece = VIRI_UNMOVED_ROOK | VIRI_BLACK_PIECE;
		}

		if (insert_index&1)
			piece <<= 4;

		packed_board->pieces[insert_index / 2] |= piece;
		insert_index++;

		mask &= (mask-1);
	}

	packed_board->stm = board->turn;
	packed_board->ep = board->ep_square;

	packed_board->halfmove_clock = board->halfmove_clock;
	packed_board->full_move_counter = board->half_move_count / 2;

	packed_board->occupancy = little_endian_64(packed_board->occupancy);
	packed_board->full_move_counter = little_endian_16(packed_board->full_move_counter);
	packed_board->white_score = little_endian_16(packed_board->white_score);
}

int packed_board_to_board(bitboard *board, viri_packed_board *packed_board)
{
	memset(board, 0, sizeof(bitboard));
	memset(&board->piece_arr, EMPTY_PIECE, 64 * sizeof(uint8_t));

	packed_board->occupancy = little_endian_64(packed_board->occupancy);
	packed_board->full_move_counter = little_endian_16(packed_board->full_move_counter);
	packed_board->white_score = little_endian_16(packed_board->white_score);

	board->turn = packed_board->stm == VIRI_BLACK_STM;
	board->ep_square = packed_board->ep;

	board->half_move_count = 2 * (packed_board->full_move_counter) + board->turn;
	board->halfmove_clock = packed_board->halfmove_clock;

	board->king_square[WHITE] = 64;
	board->king_square[BLACK] = 64;

	uint64_t mask = packed_board->occupancy;
	int offset = 0;

	while (mask) {
		int sq = __builtin_ctzll(mask);
		int byte_index = offset / 2;
		int piece = packed_board->pieces[byte_index];

		if (!(offset % 2))
			piece = piece&0xF;
		else
			piece >>= 4;

		if (piece&6) {
			switch (sq) {
			case 0:
				board->can_queenside_castle_w = 1;
				break;
			case 7:
				board->can_kingside_castle_w = 1;
				break;
			case 56:
				board->can_queenside_castle_b = 1;
				break;
			case 63:
				board->can_kingside_castle_b = 1;
				break;
			default:
				break;
			}
		}

		piece = piece_mappings_from_viri[piece];

		uint64_t sq_mask = (1ull << (sq));

		board->pieces_all |= sq_mask;
		board->pieces_side[COLOR(piece)] |= sq_mask;
		board->pieces[piece] |= sq_mask;
		board->piece_arr[sq] = piece;

		if (ISKING(piece))
			board->king_square[COLOR(piece)] = sq;

		offset++;
		mask &= (mask - 1);
	}

	//IsThreatened will segfault on the invalid square without this.
	if (board->king_square[WHITE] == 64 || board->king_square[BLACK] == 64)
		return NO_BOARD_CORRUPTED;

	if (is_threatened(board, board->king_square[board->turn], board->turn))
		board->in_check = 1;

	generate_hash_code(board);
	return BOARD_SAVED;
}

int read_packed_board(binpack_details *details)
{
	reader_details *r_details = details->reader_info;

	r_details->game_start_ptr = r_details->current_ptr;

	if (r_details->current_ptr + sizeof(r_details->packed_board->occupancy) > details->end_ptr)
		return NO_BOARD_END_OF_FILE;

	memcpy(&r_details->packed_board->occupancy, r_details->current_ptr, sizeof(r_details->packed_board->occupancy));

	if (r_details->packed_board->occupancy) {
		if (r_details->current_ptr + sizeof(*r_details->packed_board) > details->end_ptr)
			return NO_BOARD_END_OF_FILE;

		memcpy((uint8_t *)r_details->packed_board + sizeof(r_details->packed_board->occupancy),
			   r_details->current_ptr + sizeof(r_details->packed_board->occupancy),
			   sizeof(*r_details->packed_board) - sizeof(r_details->packed_board->occupancy));

		r_details->current_ptr += sizeof(*r_details->packed_board);
	} else {
		uint16_t extension_id = 0;
		uint16_t payload_length = 0;

		memcpy(&extension_id, r_details->current_ptr + sizeof(r_details->packed_board->occupancy), sizeof(extension_id));
		memcpy(&payload_length, r_details->current_ptr + sizeof(r_details->packed_board->occupancy) + sizeof(extension_id), sizeof(payload_length));

		extension_id = little_endian_16(extension_id);
		payload_length = little_endian_16(payload_length);

		uint64_t total_extension_bytes = sizeof(r_details->packed_board->occupancy) + 4 + payload_length;

		if (r_details->current_ptr + total_extension_bytes + sizeof(viri_packed_board) > details->end_ptr)
			return NO_BOARD_END_OF_FILE;

		r_details->current_ptr += total_extension_bytes;
		memcpy(r_details->packed_board, r_details->current_ptr, sizeof(viri_packed_board));
		r_details->current_ptr += sizeof(viri_packed_board);
	}

	r_details->packed_board->occupancy = little_endian_64(r_details->packed_board->occupancy);
	r_details->packed_board->full_move_counter = little_endian_16(r_details->packed_board->full_move_counter);
	r_details->packed_board->white_score = little_endian_16(r_details->packed_board->white_score);

	if (r_details->packed_board->result == VIRI_WHITE_WIN)
		r_details->current_game_winner = VICTOR_WHITE;
	else if (r_details->packed_board->result == VIRI_BLACK_WIN)
		r_details->current_game_winner = VICTOR_BLACK;
	else
		r_details->current_game_winner = VICTOR_DRAW;

	r_details->game_index++;
	r_details->position_index++;

	memcpy(&r_details->next_pair, r_details->current_ptr, sizeof(viri_move_score_pair));
	r_details->current_ptr += sizeof(viri_move_score_pair);
	r_details->next_pair.move.raw = little_endian_16(r_details->next_pair.move.raw);
	r_details->next_pair.white_score = little_endian_16(r_details->next_pair.white_score);

	return packed_board_to_board(r_details->board, r_details->packed_board);
}

//Truncate the newly opened file if it doesn't end in four zero bytes.
void clear_corrupted_game(const char *file_name)
{
	FILE *file = fopen(file_name, "rb+");

	if (!file)
		return;

	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);

	if (file_size < 4)
		return;

	uint32_t tail;

	fseek(file, -4, SEEK_END);
	if (!fread(&tail, sizeof(uint32_t), 1, file))
		return;

	//File terminates correctly
	if (!tail)
		return;

	long search_pos = file_size - 4;
	long clean_offset = 0;
	uint32_t window = 0xFFFFFFFF;

	while (search_pos >= 0) {
		fseek(file, search_pos, SEEK_SET);
		uint8_t byte;

		if (!fread(&byte, 1, 1, file))
			return;

		window = (window << 8) | byte;

		if (!window) {
			clean_offset = search_pos + 4;
			break;
		}
		search_pos--;
	}

	if (resize_file(file, clean_offset))
		DEBUG_ERROR("Failed to resize file %s", file_name);

	fclose(file);
}

void binpack_open(binpack_details *details, const char *file_name, int is_reader)
{
	init_viri_tables();

	//Assume we aren't opening twice on the same variable.
	memset(details, 0, sizeof(binpack_details));

	details->is_reader = is_reader;

	clear_corrupted_game(file_name);

	if (!is_reader) {
		details->binpack = fopen(file_name, "ab+");
		if (!details->binpack) {
			printf("Failed to open binpack file for writing: %s\n", file_name);
			exit(1);
		}

		details->reader_info = NULL;
	} else {
		if (mmap_open(file_name, &details->mmap_file)) {
			printf("Failed to memory map binpack file: %s\n", file_name);
			exit(1);
		}
		details->start_ptr = (uint8_t *)details->mmap_file.data;
		details->end_ptr = details->start_ptr + details->mmap_file.size;

		details->reader_info = calloc(1, sizeof(reader_details));

		details->reader_info->board = calloc(1, sizeof(bitboard));
		details->reader_info->packed_board = calloc(1, sizeof(viri_packed_board));
		details->reader_info->current_ptr = details->start_ptr;

		read_packed_board(details);
	}

	CREATE_MUTEX(details->lock);
}

void binpack_close(binpack_details *details)
{
	if (!details->is_reader && details->binpack) {
		fflush(details->binpack);
		fclose(details->binpack);
		details->binpack = NULL;
	} else {
		mmap_close(&details->mmap_file);
		free(details->reader_info->board);
		free(details->reader_info->packed_board);
		free(details->reader_info);
	}

	DESTROY_MUTEX(details->lock);

	if (details->written_this_session) {
		details->last_print_time = clock();
		double duration = (double) ((details->last_print_time - details->start_time) / CLOCKS_PER_SEC);

		printf("\rGenerated %d positions this session (%.2f pos/sec)\n", details->written_this_session, details->written_this_session / duration);
		details->written_this_session = 0;
	}
}

int binpack_next(binpack_details *details, bitboard *brd, viri_score *eval, uint8_t *result, int loop, int is_cleaner, int minimum_fen_skips)
{
	reader_details *r_details = details->reader_info;
	viri_move_score_pair pair;
	int skipped_unusable = 0;
	int skipped_usable = 0;

	//Break once we find a good move.
	while (1) {
		pair = details->reader_info->next_pair;
		if (r_details->current_ptr + sizeof(viri_move_score_pair) > details->end_ptr) {
			if (loop) {
				r_details->current_ptr = details->start_ptr;
				r_details->position_index = 0;
				r_details->game_index = 0;
				if (read_packed_board(details) == NO_BOARD_CORRUPTED)
					continue;
				pair.move.raw = 0;
				pair.white_score = r_details->packed_board->white_score;
			} else
				return (skipped_unusable + skipped_usable > 0) ? skipped_unusable + skipped_usable : NO_BOARD_END_OF_FILE;
		} else {
			if (pair.raw == 0) {
				int result = read_packed_board(details);

				if (result == NO_BOARD_END_OF_FILE)
					return (skipped_unusable + skipped_usable > 0) ? skipped_unusable + skipped_usable : NO_BOARD_END_OF_FILE;
				else if (result == NO_BOARD_CORRUPTED)
					continue;
			} else {
				memcpy(&details->reader_info->next_pair, r_details->current_ptr, sizeof(viri_move_score_pair));
				r_details->current_ptr += sizeof(viri_move_score_pair);

				//For the purpose of speed, bypass the legal-move checks in move_from_struct
				//The cleaner can double-check the legality on its own, but avoid the cost during trainer.
				move m = {
					.start_square = pair.move.start_square,
					.end_square = pair.move.end_square
				};

				if (pair.move.move_type == VIRI_MOVE_TYPE_PROMOTIONS)
					m.promote_to = promote_mappings_from_viri[pair.move.promote_piece];

				int to_pc = (r_details->board->piece_arr[m.end_square]);
				int from_pc = (r_details->board->piece_arr[m.start_square]);

				if (to_pc != EMPTY_PIECE) {
					//Viriformat castling is king takes rook
					//This assumes standard chess variant.
					if (ISKING(from_pc) && ISROOK(to_pc) && COLOR(from_pc) == COLOR(to_pc)) {
						if ((m.end_square & 7) == 7)
							m.end_square = m.start_square + 2;
						else
							m.end_square = m.start_square - 2;
					}
				}

				if (move_piece(r_details->board, m, NULL)) {
					if (!is_cleaner) {
						board_print(r_details->board);
						printf("Move error detected within binpack:\n");
						printf("Position #%" PRId64 " | Game #%" PRId64 "\n", r_details->position_index, r_details->game_index);
						printf("\tOffset: 0x%" PRIx64 "\n", r_details->current_ptr - details->start_ptr);
						printf("\tMove: %d->%d | Type=%d | Promote=%d", pair.move.start_square, pair.move.end_square, pair.move.move_type, pair.move.promote_piece);
						exit(1);
					} else
						return CORRUPTED_POSITION;
				}
				r_details->position_index++;
			}
		}

		if (!is_cleaner) {
			int is_best_capture = 0;
			int to_pc = (r_details->board->piece_arr[r_details->next_pair.move.end_square]);
			int from_pc = (r_details->board->piece_arr[r_details->next_pair.move.start_square]);

			if (to_pc != EMPTY_PIECE && !(ISKING(from_pc) && ISROOK(to_pc) && COLOR(from_pc) == COLOR(to_pc)))
				is_best_capture = 1;

			if (!is_best_capture &&
				abs(pair.white_score) <= 2000 &&
				r_details->next_pair.move.move_type == 0 &&
				!r_details->board->in_check &&
				r_details->board->half_move_count >= 16) {
					if (skipped_usable >= minimum_fen_skips)
						break;

					skipped_usable++;
				} else
					skipped_unusable++;
		} else
			break;
	}

	*result = r_details->current_game_winner;
	*eval = pair.white_score;
	*brd =  *r_details->board;
	return skipped_unusable + skipped_usable;
}

void binpack_write(binpack_details *details, generated_game_buffer *buffer)
{
	for (int i = 0; i < VIRI_WRITEBUFFER_SIZE; i++) {
		buffer->packed_boards[i].occupancy = little_endian_64(buffer->packed_boards[i].occupancy);
		buffer->packed_boards[i].full_move_counter = little_endian_16(buffer->packed_boards[i].full_move_counter);
		buffer->packed_boards[i].white_score = little_endian_16(buffer->packed_boards[i].white_score);

		for (int j = 0; j < buffer->moves_this_game[i]; j++) {
			buffer->pair_lists[i][j].move = little_endian_16(buffer->pair_lists[i][j].move);
			buffer->pair_lists[i][j].white_score = little_endian_16(buffer->pair_lists[i][j].white_score);
		}
	}

	uint32_t zero = 0;

	LOCK_MUTEX(details->lock);

	for (int i = 0; i < VIRI_WRITEBUFFER_SIZE; i++) {
		if (buffer->moves_this_game[i] <= 0)
			break;
		fwrite(&buffer->packed_boards[i], sizeof(viri_packed_board), 1, details->binpack);
		fwrite(buffer->pair_lists[i], sizeof(viri_move_score_pair), buffer->moves_this_game[i], details->binpack);
		fwrite(&zero, 4, 1, details->binpack);
		details->written_this_session += buffer->moves_this_game[i] + 1;
	}

	if ((clock() - details->last_print_time) / CLOCKS_PER_SEC > 10) {
		details->last_print_time = clock();
		double duration = (double) ((details->last_print_time - details->start_time) / CLOCKS_PER_SEC);

		printf("\rGenerated %d positions this session (%.2f pos/sec)", details->written_this_session, details->written_this_session / duration);
	}

	UNLOCK_MUTEX(details->lock);

	memset(buffer, 0, sizeof(generated_game_buffer));
}

void binpack_print_info(const char *file_name)
{
	binpack_details details =  {0};

	binpack_open(&details, file_name, 1);
	uint64_t used_count = 1;
	uint64_t skipped_count = 0;

	int temp;
	bitboard b;
	viri_score s;
	uint8_t r;

	char buf[32];

	printf("Binpack statistics:\n");
	printf("\tTotal Games: %s\n", comma_delimit_number(buf, details.reader_info->game_index));
	printf("\tTotal Positions: %s\n", comma_delimit_number(buf, details.reader_info->position_index));
	printf("\tUsable Positions: %s (%.1f%%)\n", comma_delimit_number(buf, used_count), (100.0 * used_count) / details.reader_info->position_index);
	printf("\tSkipped Positions: %s (%.1f%%)\n", comma_delimit_number(buf, skipped_count), (100.0 * skipped_count) / details.reader_info->position_index);
	fflush(stdout);

	uint64_t recently_read = 0;

	clock_t read_start_time = clock();

	while ((temp = binpack_next(&details, &b, &s, &r, 0, 0, 0)) != NO_BOARD_END_OF_FILE) {
		skipped_count += temp;
		used_count++;

		recently_read += 1 + temp;

		//Printout roughly every 10,000,000 entries.
		if (recently_read > 10000000) {
			recently_read -= 10000000;

			printf("\033[5A");
			printf("\033[2K\rBinpack statistics:\n");
			printf("\033[2K\r\tTotal Games: %s\n", comma_delimit_number(buf, details.reader_info->game_index));
			printf("\033[2K\r\tTotal Positions: %s\n", comma_delimit_number(buf, details.reader_info->position_index));
			printf("\033[2K\r\tUsable Positions: %s (%.1f%%)\n", comma_delimit_number(buf, used_count), (100.0 * used_count) / details.reader_info->position_index);
			printf("\033[2K\r\tSkipped Positions: %s (%.1f%%)\n", comma_delimit_number(buf, skipped_count), (100.0 * skipped_count) / details.reader_info->position_index);
			fflush(stdout);
		}
	}
	clock_t read_end_time = clock();
	double duration = (read_end_time - read_start_time) /  (double) CLOCKS_PER_SEC;
	double positions_per_second = details.reader_info->position_index / duration;

	uint64_t byte_length = details.mmap_file.size;
	float bytes_per_position = (float) byte_length / details.reader_info->position_index;

	printf("\033[5A");
	printf("\033[2K\rBinpack statistics:\n");
	printf("\033[2K\r\tTotal Games: %s\n", comma_delimit_number(buf, details.reader_info->game_index));
	printf("\033[2K\r\tTotal Positions: %s\n", comma_delimit_number(buf, details.reader_info->position_index));
	printf("\033[2K\r\tUsable Positions: %s (%.1f%%)\n", comma_delimit_number(buf, used_count), (100.0 * used_count) / details.reader_info->position_index);
	printf("\033[2K\r\tSkipped Positions: %s (%.1f%%)\n", comma_delimit_number(buf, skipped_count), (100.0 * skipped_count) / details.reader_info->position_index);
	printf("\033[2K\r\tDuration: %.3f seconds\n", duration);
	printf("\033[2K\r\tPositions per second: %s\n", comma_delimit_number(buf, (uint64_t) positions_per_second));
	printf("\033[2K\r\tByte Size: %s\n", comma_delimit_number(buf, byte_length));
	printf("\033[2K\r\tBytes per Position: %.4f\n", bytes_per_position);

	binpack_close(&details);
}

//The binpack contains hundreds of millions of positions from many games.
//A couple of games were written wrong and can throw errors.
//We must clean them out while preserving as many games as possible.
//
// 1 in a million games seem to get corrupted somehow.
//
void binpack_clean(const char *file_name)
{
	binpack_details details =  {0};

	binpack_open(&details, file_name, 1);
	reader_details *r_details = details.reader_info;

	uint8_t *last_print_ptr = details.start_ptr;
	uint8_t *invalid_game_ptr = NULL;
	int temp;
	bitboard b;
	viri_score s;
	uint8_t r;

	const char *extension = strrchr(file_name, '.');

	assert(extension);
	int new_length = strlen(file_name) + 4 + 1; // "_new", '\0'
	char *new_file_name = malloc(new_length);

	size_t len = extension - file_name;

	strncpy(new_file_name, file_name, len);
	strncpy((char *) new_file_name + len, "_new", 5);
	strncpy((char *) new_file_name + len + 4, extension, new_length - len - 4);
	new_file_name[new_length - 1] = '\0';

	FILE *output = fopen(new_file_name, "ab+");

	uint64_t last_print_positions = r_details->position_index;

	char buf[32];

	printf("Started cleaning binpack - %s\n", file_name);
	while ((temp = binpack_next(&details, &b, &s, &r, 0, 1, 0)) != NO_BOARD_END_OF_FILE) {
		if (temp == CORRUPTED_POSITION || !b.pieces[BLACK_KING] || !b.pieces[WHITE_KING] || is_threatened(&b, b.king_square[FLIP_COLOR(b.turn)], FLIP_COLOR(b.turn)))
			invalid_game_ptr = r_details->game_start_ptr;
		else if (invalid_game_ptr && r_details->game_start_ptr != last_print_ptr) {
			last_print_ptr = r_details->game_start_ptr;
			invalid_game_ptr = NULL;
		}

		size_t buffer_size = r_details->game_start_ptr - last_print_ptr;

		if ((buffer_size && invalid_game_ptr) ||
			buffer_size >= 8096 - (sizeof(viri_packed_board) + MAX_POSITIONS_PER_GAME * sizeof(viri_move_score_pair))) {
			fwrite(last_print_ptr, buffer_size, 1, output);
			last_print_ptr = r_details->game_start_ptr;

			if ((r_details->position_index - last_print_positions) > 10000000) {
				last_print_positions += 10000000;
				printf("\r%s positions Processed", comma_delimit_number(buf, r_details->position_index));
			}
		}

	}

	size_t buffer_size = r_details->game_start_ptr - last_print_ptr;

	fwrite(last_print_ptr, buffer_size, 1, output);
	last_print_ptr = r_details->game_start_ptr;

	printf("\r%s positions Processed\n", comma_delimit_number(buf, r_details->position_index));
	printf("Finished cleaning binpack - %s\n", file_name);
	fclose(output);
	binpack_close(&details);
	free(new_file_name);
}
