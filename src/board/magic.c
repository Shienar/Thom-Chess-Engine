#include "board/magic.h"
#include "board/bitboard.h"

magic bishop_magics[64] = {0};
magic rook_magics[64] = {0};
uint64_t rook_table[102400] = {0};
uint64_t bishop_table[20480] = {0};

uint64_t rook_magic_numbers[64] = {
		0x80004000976080ULL,	0x1040400010002000ULL,  0x4880200210000980ULL,  0x5280080010000482ULL,
		0x200040200081020ULL,   0x2100080100020400ULL,  0x4280008001000200ULL,  0x1000a4425820300ULL,
		0x29002100800040ULL,	0x4503400040201004ULL,  0x209002001004018ULL,   0x1131000a10002100ULL,
		0x9000800120500ULL,		0x10e001804820010ULL,   0x29000402000100ULL,	0x2002000d01c40292ULL,
		0x80084000200c40ULL,	0x10004040002002ULL,	0x201030020004014ULL,   0x80012000a420020ULL,
		0x129010008001204ULL,   0x6109010008040002ULL,  0x950010100020004ULL,   0x803a0000c50284ULL,
		0x80004100210080ULL,	0x200240100140ULL,	  	0x20004040100800ULL,	0x4018090300201000ULL,
		0x4802010a00102004ULL,  0x2001000900040002ULL,  0x4a02104001002a8ULL,   0x2188108200204401ULL,
		0x40400020800080ULL,	0x880402000401004ULL,   0x10040800202000ULL,	0x604410a02001020ULL,
		0x200200206a001410ULL,  0x86000400810080ULL,	0x428200040600080bULL,  0x2001000041000082ULL,
		0x80002000484000ULL,	0x210002002c24000ULL,   0x401a200100410014ULL,  0x5021000a30009ULL,
		0x218000509010010ULL,   0x4000400410080120ULL,  0x20801040010ULL,	   	0x29040040820011ULL,
		0x4080400024800280ULL,  0x500200040100440ULL,   0x2880142001004100ULL,  0x412020400a001200ULL,
		0x18c028004080080ULL,   0x884001020080401ULL,   0x210810420400ULL,	  	0x801048745040200ULL,
		0x4401002040120082ULL,  0x408200210012ULL,	  	0x110008200441ULL,	  	0x2010002004100901ULL,
		0x801000800040211ULL,   0x480d000400820801ULL,  0x820104201280084ULL,   0x1001040311802142ULL};

uint64_t bishop_magic_numbers[64] = {
		0x1024b002420160ULL,	0x1008080140420021ULL,  0x2012080041080024ULL,  0xc282601408c0802ULL,
		0x2004042000000002ULL,  0x12021004022080ULL,	0x880414820100000ULL,   0x4501002211044000ULL,
		0x20402222121600ULL,	0x1081088a28022020ULL,  0x1004c2810851064ULL,   0x2040080841004918ULL,
		0x1448020210201017ULL,  0x4808110108400025ULL,  0x10504404054004ULL,	0x800010422092400ULL,
		0x40000870450250ULL,	0x402040408080518ULL,   0x1000980a404108ULL,	0x1020804110080ULL,
		0x8200c02082005ULL,	 0x40802009a0800ULL,	 0x1000201012100ULL,	 0x111080200820180ULL,
		0x904122104101024ULL,   0x4008200405244084ULL,  0x44040002182400ULL,	0x4804080004021002ULL,
		0x6401004024004040ULL,  0x404010001300a20ULL,   0x428020200a20100ULL,   0x300460100420200ULL,
		0x404200c062000ULL,	 0x22101400510141ULL,	0x104044400180031ULL,   0x2040040400280211ULL,
		0x8020400401010ULL,	 0x20100110401a0040ULL,  0x100101005a2080ULL,	0x1a008300042411ULL,
		0x120a025004504000ULL,  0x4001084242101000ULL,  0xa020202010a4200ULL,   0x4000002018000100ULL,
		0x80104000044ULL,	   0x1004009806004043ULL,  0x100401080a000112ULL,  0x1041012101000608ULL,
		0x40400c250100140ULL,   0x80a10460a100002ULL,   0x2210030401240002ULL,  0x6040aa108481b20ULL,
		0x4009004050410002ULL,  0x8106003420200e0ULL,   0x1410500a08206000ULL,  0x92548802004000ULL,
		0x1040041241028ULL,	 0x120042025011ULL,	  0x8060104054400ULL,	 0x20004404020a0a01ULL,
		0x40008010020214ULL,	0x4000050209802c1ULL,   0x208244210400ULL,	  0x10140848044010ULL};

uint64_t naive_bishop(int square, uint64_t occupancy, int use_occupancy)
{
	uint64_t potential_moves = 0;

	int row = get_row(square);
	int column = get_column(square);

	if (use_occupancy) {
		//Topleft
		for (int r = row + 1, c = column - 1; r <= 7 && c >= 0; r++, c--) {
			potential_moves |= (1ull << (8 * r + c));
			if ((1ull << (8 * r + c)) & occupancy)
				break;
		}

		//Topright
		for (int r = row + 1, c = column + 1; r <= 7 && c <= 7; r++, c++) {
			potential_moves |= (1ull << (8 * r + c));
			if ((1ull << (8 * r + c)) & occupancy)
				break;
		}

		//Bottomleft
		for (int r = row - 1, c = column - 1; r >= 0 && c >= 0; r--, c--) {
			potential_moves |= (1ull << (8 * r + c));
			if ((1ull << (8 * r + c)) & occupancy)
				break;
		}

		//Bottomright
		for (int r = row - 1, c = column + 1; r >= 0 && c <= 7; r--, c++) {
			potential_moves |= (1ull << (8 * r + c));
			if ((1ull << (8 * r + c)) & occupancy)
				break;
		}
	} else {
		//Topleft
		for (int r = row + 1, c = column - 1; r <= 6 && c >= 1; r++, c--)
			potential_moves |= (1ull << (8 * r + c));

		//Topright
		for (int r = row + 1, c = column + 1; r <= 6 && c <= 6; r++, c++)
			potential_moves |= (1ull << (8 * r + c));

		//Bottomleft
		for (int r = row - 1, c = column - 1; r >= 1 && c >= 1; r--, c--)
			potential_moves |= (1ull << (8 * r + c));

		//Bottomright
		for (int r = row - 1, c = column + 1; r >= 1 && c <= 6; r--, c++)
			potential_moves |= (1ull << (8 * r + c));
	}

	return potential_moves;
}

uint64_t naive_rook(int square, uint64_t occupancy, int use_occupancy)
{
	uint64_t potential_moves = 0;

	int row = get_row(square);
	int column = get_column(square);

	if (use_occupancy) {
		//Right
		for (int c = column + 1; c <= 7; c++) {
			potential_moves |= (1ull << (8 * row + c));
			if ((1ull << (8 * row + c)) & occupancy)
				break;
		}

		//Left
		for (int c = column - 1; c >= 0; c--) {
			potential_moves |= (1ull << (8 * row + c));
			if ((1ull << (8 * row + c)) & occupancy)
				break;
		}

		//Above
		for (int r = row + 1; r <= 7; r++) {
			potential_moves |= (1ull << (8 * r + column));
			if ((1ull << (8 * r + column)) & occupancy)
				break;
		}

		//Below
		for (int r = row - 1; r >= 0; r--) {
			potential_moves |= (1ull << (8 * r + column));
			if ((1ull << (8 * r + column)) & occupancy)
				break;
		}
	} else {
		//Right
		for (int c = column + 1; c <= 6; c++)
			potential_moves |= (1ull << (8 * row + c));

		//Left
		for (int c = column - 1; c >= 1; c--)
			potential_moves |= (1ull << (8 * row + c));

		//Above
		for (int r = row + 1; r <= 6; r++)
			potential_moves |= (1ull << (8 * r + column));

		//Below
		for (int r = row - 1; r >= 1; r--)
			potential_moves |= (1ull << (8 * r + column));
	}

	return potential_moves;
}

void initMagics(void)
{
	uint64_t *start_index_rook = &rook_table[0];
	uint64_t *start_index_bishop = &bishop_table[0];

	for (int square = 0; square < 64; square++) {
		uint64_t rook_mask = naive_rook(square, 0, 0);
		uint64_t bishop_mask = naive_bishop(square, 0, 0);

		int set_bits_rook = __builtin_popcountll(rook_mask);
		int set_bits_bishop = __builtin_popcountll(bishop_mask);

		magic *rook_magic = &rook_magics[square];
		magic *bishop_magic = &bishop_magics[square];

		rook_magic->attacks = start_index_rook;
		bishop_magic->attacks = start_index_bishop;

		start_index_rook += 1 << set_bits_rook;
		start_index_bishop += 1 << set_bits_bishop;

		rook_magic->mask = rook_mask;
		bishop_magic->mask = bishop_mask;

		rook_magic->magic = rook_magic_numbers[square];
		bishop_magic->magic = bishop_magic_numbers[square];

		rook_magic->shift_offset = 64 - set_bits_rook;
		bishop_magic->shift_offset = 64 - set_bits_bishop;

		//Iterate over submasks of each occupancy
		uint64_t occ = rook_mask;

		while (occ) {
			int magic_index = (occ * rook_magic->magic) >> rook_magic->shift_offset;

			rook_magic->attacks[magic_index] = naive_rook(square, occ, 1);
			occ = (occ - 1) & rook_mask;
		}
		rook_magic->attacks[0] = naive_rook(square, occ, 1); //Zero-occupancy case

		occ = bishop_mask;
		while (occ) {
			int magic_index = (occ * bishop_magic->magic) >> bishop_magic->shift_offset;

			bishop_magic->attacks[magic_index] = naive_bishop(square, occ, 1);
			occ = (occ - 1) & bishop_mask;
		}
		bishop_magic->attacks[0] = naive_bishop(square, occ, 1); //Zero-occupancy case
	}
}

uint64_t pawn_attacks[2][64];
void init_pawn_attacks(void)
{
	for (int square = 0; square < 64; square++) {
		pawn_attacks[WHITE][square] = 0;
		pawn_attacks[BLACK][square] = 0;

		int row = get_row(square);
		int column = get_column(square);

		if (row < 7) {
			if (column > 0)
				pawn_attacks[WHITE][square] |= (1ull << (square+7));
			if (column < 7)
				pawn_attacks[WHITE][square] |= (1ull << (square+9));
		}

		if (row > 0) {
			if (column > 0)
				pawn_attacks[BLACK][square] |= (1ull << (square-9));
			if (column < 7)
				pawn_attacks[BLACK][square] |= (1ull << (square-7));
		}
	}
}

uint64_t knight_attacks[64];
void init_knight_move_table(void)
{
	for (int square = 0; square < 64; square++) {
		knight_attacks[square] = 0;
		int row = get_row(square);
		int column = get_column(square);

		/*
		 *	  - 2 - 3 -
		 *	  1 - - - 4
		 *	  - - N - -
		 *	  8 - - - 5
		 *	  - 7 - 6 -
		 *
		 *  1: end_square = square + 6
		 *  2: end_square = square + 15
		 *  3: end_square = square + 17
		 *  4: end_square = square + 10
		 *  5: end_square = square - 6
		 *  6: end_square = square - 15
		 *  7: end_square = square - 17
		 *  8: end_square = square - 10
		 */

		if (column - 2 >= 0 && row + 1 <= 7)
			knight_attacks[square] |= (1ull << (square+6));
		if (column - 1 >= 0 && row + 2 <= 7)
			knight_attacks[square] |= (1ull << (square+15));
		if (column + 1 <= 7 && row + 2 <= 7)
			knight_attacks[square] |= (1ull << (square+17));
		if (column + 2 <= 7 && row + 1 <= 7)
			knight_attacks[square] |= (1ull << (square+10));
		if (column + 2 <= 7 && row - 1 >= 0)
			knight_attacks[square] |= (1ull << (square-6));
		if (column + 1 <= 7 && row - 2 >= 0)
			knight_attacks[square] |= (1ull << (square-15));
		if (column - 1 >= 0 && row - 2 >= 0)
			knight_attacks[square] |= (1ull << (square-17));
		if (column - 2 >= 0 && row - 1 >= 0)
			knight_attacks[square] |= (1ull << (square-10));
	}
}

uint64_t king_attacks[64];
void init_king_move_table(void)
{

	for (int square = 0; square < 64; square++) {
		king_attacks[square] = 0;
		int row = get_row(square);
		int column = get_column(square);

		/*
		 *
		 *	  1 2 3
		 *	  4 K 5
		 *	  6 7 8
		 *
		 *  1: end_square = square + 7
		 *  2: end_square = square + 8
		 *  3: end_square = square + 9
		 *  4: end_square = square - 1
		 *  5: end_square = square + 1
		 *  6: end_square = square - 9
		 *  7: end_square = square - 8
		 * 8: end_square = square - 7
		 */
		if (column - 1 >= 0 && row + 1 <= 7)
			king_attacks[square] |= (1ull << (square+7));
		if (row + 1 <= 7)
			king_attacks[square] |= (1ull << (square+8));
		if (column + 1 <= 7 && row + 1 <= 7)
			king_attacks[square] |= (1ull << (square+9));
		if (column - 1 >= 0)
			king_attacks[square] |= (1ull << (square-1));
		if (column + 1 <= 7)
			king_attacks[square] |= (1ull << (square+1));
		if (column - 1 >= 0 && row - 1 >= 0)
			king_attacks[square] |= (1ull << (square-9));
		if (row - 1 >= 0)
			king_attacks[square] |= (1ull << (square-8));
		if (column + 1 <= 7 && row - 1 >= 0)
			king_attacks[square] |= (1ull << (square-7));
	}
}
