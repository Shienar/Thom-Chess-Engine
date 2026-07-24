#include "analyze/hce/hce.h"


/**
 * For simplicity, this represents white's view of the board when
 * viewed in a text editor.
 * 
 * White Piece square tables receive a rank-wise FLIP_SQUARE. (xor 56)
 * Black piece square tables receive a column-wise MIRROR_SQUARE (xor 7)
 * Black column-based parameters receive the column-wise MIRROR_SQUARE.
 * Black row-based parameters also received MIRROR_SQUARE, since the math works out there as well given row=[0, 7].
 * 
 * The tuner is intended to bootstrap an eval from the following start:
 * K = 3.612
evalParameters hce_params = {
	.genericPieceValues = {
		P(   77,  141), P(  263,  346), P(  302,  374), P(  373,  642), P(  841, 1182), P(    0,    0) 
	},
	.tempo = P(20, 20)
};
 * 
 */
evalParameters hce_params = {
	.genericPieceValues = {
		P(   77,  141), P(  263,  346), P(  302,  374), P(  373,  642), P(  841, 1182), P(    0,    0) 
	},
	.rawPieceTables = {
			[PAWN / 2] = {
				P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), 
				P(    4,   66), P(   34,   83), P(   52,   61), P(   71,   23), P(   56,   26), P(   25,   57), P(  -14,   94), P(  -14,   78), 
				P(    8,    6), P(    9,   39), P(   38,  -11), P(   26,  -39), P(   34,  -41), P(   43,  -25), P(    0,   34), P(    5,    9), 
				P(   -7,  -12), P(   -7,    9), P(    7,  -26), P(    5,  -38), P(   11,  -39), P(   -3,  -26), P(  -14,    6), P(  -11,  -14), 
				P(  -19,  -23), P(  -15,    8), P(   -4,  -24), P(    3,  -28), P(    1,  -28), P(   -7,  -21), P(  -19,    6), P(  -22,  -21), 
				P(  -20,  -28), P(  -22,    4), P(  -10,  -20), P(  -11,  -16), P(  -12,  -17), P(  -13,  -18), P(  -27,    4), P(  -23,  -26), 
				P(  -21,  -27), P(  -11,    7), P(   -9,  -13), P(  -20,   -5), P(  -20,   -4), P(  -11,  -14), P(  -17,    6), P(  -26,  -24), 
				P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0) 
			},
			[KNIGHT / 2] = {
				P( -112,  -66), P( -114,   -2), P(  -48,    1), P(  -21,    1), P(   19,   -2), P(  -30,   -7), P(  -61,  -14), P(  -99,  -80), 
				P(  -29,  -20), P(  -21,   -6), P(   23,  -10), P(    7,   -5), P(    5,  -12), P(   35,  -22), P(  -24,    2), P(  -17,  -27), 
				P(    2,  -11), P(   21,   -6), P(   33,   11), P(   45,    6), P(   55,    6), P(   43,    2), P(   24,   -4), P(    0,  -18), 
				P(    7,    0), P(   19,   12), P(   42,   19), P(   38,   24), P(   36,   24), P(   40,   18), P(   19,   13), P(   10,   -6), 
				P(    5,   10), P(   18,   17), P(   28,   30), P(   28,   35), P(   33,   35), P(   25,   28), P(   30,   12), P(    1,    7), 
				P(  -13,   -3), P(   10,    5), P(   14,    9), P(   23,   28), P(   27,   21), P(   13,   10), P(    6,    3), P(  -14,   -3), 
				P(  -13,   -2), P(  -15,   -3), P(   -1,    4), P(    9,    5), P(    9,    2), P(   -5,    6), P(  -16,    0), P(  -11,   -5), 
				P(  -44,  -13), P(  -16,  -14), P(  -10,   -8), P(   -3,    2), P(   -3,    1), P(   -4,   -9), P(  -15,  -18), P(  -43,  -13) 
			},
			[BISHOP / 2] = {
				P(  -39,    5), P(  -42,    6), P(  -53,    0), P(  -76,   14), P(  -68,    6), P(  -48,    5), P(  -33,    8), P(  -36,   -2), 
				P(  -10,   -5), P(    6,    0), P(    9,   -1), P(   11,   -8), P(    1,   -2), P(   -1,   -3), P(   10,   -4), P(   -4,  -10), 
				P(   26,    2), P(   26,    4), P(   35,    3), P(   27,    0), P(   27,   -8), P(   38,    0), P(   39,   -5), P(   25,   -1), 
				P(    4,    2), P(    1,    8), P(   22,    6), P(   30,    8), P(   30,   14), P(   18,    6), P(    6,    4), P(   -9,    2), 
				P(   -3,   -7), P(   -1,    8), P(   -5,   12), P(   25,   11), P(   19,    9), P(    1,    9), P(   -9,    5), P(    3,   -8), 
				P(    4,   -6), P(    4,    1), P(    9,    7), P(   -3,   12), P(    4,    9), P(    1,    6), P(    7,    4), P(   -7,   -7), 
				P(   -8,  -14), P(   11,  -10), P(   10,  -19), P(    3,   -4), P(   -8,   -2), P(    5,   -8), P(    5,   -9), P(   -5,   -4), 
				P(    6,  -17), P(   15,  -28), P(   -8,   -2), P(  -11,   -3), P(   -8,   -2), P(  -17,    2), P(    3,   18), P(  -15,  -14) 
			},
			[ROOK / 2] = {
				P(   28,    8), P(   10,   10), P(    8,   19), P(   14,    9), P(   13,   13), P(   23,   13), P(    6,   19), P(   17,   10), 
				P(   13,    9), P(   14,   17), P(   25,   19), P(   26,   11), P(   23,   13), P(   23,   14), P(    3,   18), P(   13,   10), 
				P(    2,    6), P(   22,    6), P(   18,    5), P(   11,    2), P(   20,    2), P(   10,    4), P(   29,    4), P(    9,    4), 
				P(   -5,    7), P(    1,    3), P(   -2,    3), P(   -5,    3), P(   -9,    3), P(  -10,   10), P(   -2,    4), P(   -3,    5), 
				P(  -13,    0), P(  -11,    1), P(  -20,    4), P(  -10,   -2), P(  -16,    1), P(  -25,    3), P(  -15,   -1), P(  -21,    2), 
				P(  -15,   -9), P(   -2,  -12), P(  -14,  -10), P(  -11,  -10), P(  -16,   -6), P(  -13,   -9), P(    3,  -21), P(  -11,   -9), 
				P(  -22,   -6), P(  -15,  -12), P(  -11,   -9), P(  -11,   -9), P(  -10,   -9), P(  -14,   -9), P(   -9,  -13), P(  -22,  -12), 
				P(  -12,  -12), P(    0,  -18), P(   -3,  -12), P(    4,  -21), P(    4,  -21), P(   -3,  -18), P(   -3,  -14), P(   -9,  -10) 
			},
			[QUEEN / 2] = {
				P(  -10,   -8), P(   16,  -10), P(    0,   26), P(   21,   15), P(   22,   12), P(   -2,   27), P(   -4,   -3), P(  -16,    8), 
				P(   20,   -8), P(  -16,   21), P(    2,   39), P(  -30,   71), P(  -23,   63), P(  -15,   43), P(  -22,   22), P(    9,    4), 
				P(   36,  -18), P(   29,    3), P(    5,   36), P(    2,   37), P(    3,   46), P(   12,   36), P(   52,  -30), P(   43,  -15), 
				P(   13,   -3), P(   -6,   16), P(   -1,   20), P(  -12,   40), P(  -12,   39), P(   -4,   28), P(   -1,   25), P(   13,    7), 
				P(   -6,    1), P(    1,   10), P(  -13,   22), P(  -13,   30), P(   -6,   19), P(  -10,    8), P(   -3,    9), P(   -9,   12), 
				P(   -3,  -16), P(   -2,   -7), P(   -6,    5), P(  -11,    0), P(   -9,    1), P(   -6,    3), P(   -3,   -1), P(   -1,  -15), 
				P(    2,  -48), P(    1,  -45), P(    2,  -37), P(    0,  -17), P(   -2,  -16), P(    5,  -38), P(    3,  -42), P(    1,  -34), 
				P(   -7,  -44), P(    0,  -60), P(   -4,  -57), P(    2,  -61), P(   -2,  -47), P(   -9,  -44), P(    1,  -52), P(  -16,  -30) 
			},
			[KING / 2] = {
				P(  113, -123), P(  135,  -59), P(   70,  -23), P(  -10,   -9), P(  -17,   -4), P(   51,  -37), P(    4,  -33), P(   55, -106), 
				P(   21,  -18), P(   31,   28), P(    6,   41), P(   54,   25), P(   41,   23), P(  -19,   34), P(   -3,   25), P(  -77,   -5), 
				P(  -20,    0), P(   32,   35), P(   61,   48), P(   16,   55), P(   -8,   61), P(   12,   49), P(   28,   33), P(  -71,    4), 
				P(  -72,   -2), P(  -42,   32), P(  -39,   55), P(  -54,   68), P(  -79,   68), P(  -48,   53), P(  -48,   34), P(  -92,    3), 
				P(  -86,   -9), P(  -44,   15), P(  -54,   41), P(  -70,   58), P(  -83,   62), P(  -52,   40), P(  -64,   21), P(  -95,   -7), 
				P(  -20,  -22), P(    7,   -4), P(  -30,   25), P(  -40,   36), P(  -33,   34), P(  -30,   21), P(   -2,   -4), P(  -23,  -27), 
				P(   65,  -54), P(   56,  -26), P(   17,   -2), P(   -2,    9), P(   -4,    7), P(   14,   -5), P(   50,  -26), P(   60,  -59), 
				P(   68,  -93), P(   77,  -62), P(   41,  -41), P(   39,  -41), P(   24,  -38), P(   45,  -39), P(   71,  -68), P(   64, -101) 
			}
	},
	.knightMobilityBonus = {
		P(  -33,  -31), P(  -13,  -10), P(   -5,   -3), P(    2,   -2), P(    9,    1), 
		P(   10,   10), P(   11,   11), P(   12,   12), P(   13,   13) 
	},
	.bishopMobilityBonus = {
		P(  -30,  -56), P(  -19,  -37), P(  -13,  -28), P(  -11,  -17), P(   -6,   -6), 
		P(    0,    6), P(    5,    8), P(    8,   12), P(    9,   18), P(   10,   19), 
		P(   11,   20), P(   12,   21), P(   13,   22), P(   14,   23) 
	},
	.rookMobilityBonus = {
		P(  -23,  -42), P(  -11,  -26), P(   -9,  -20), P(   -6,  -16), P(   -5,  -14), 
		P(   -3,   -5), P(   -1,   -3), P(    2,    1), P(    3,    9), P(    4,   13), 
		P(    6,   15), P(    8,   18), P(    9,   22), P(   13,   24), P(   14,   25) 
	},
	.queenMobilityBonus = {
		P(  -46, -110), P(  -18, -109), P(  -17, -108), P(  -16,  -77), P(  -13,  -62), 
		P(   -9,  -55), P(   -4,  -47), P(   -3,  -36), P(   -2,  -24), P(   -1,  -16), 
		P(    0,   -6), P(    1,   -1), P(    2,    6), P(    3,   12), P(    4,   18), 
		P(    5,   22), P(    6,   28), P(    7,   34), P(    8,   41), P(    9,   44), 
		P(   10,   49), P(   11,   50), P(   12,   51), P(   13,   53), P(   14,   54), 
		P(   15,   62), P(   16,   63), P(   17,   65) 
	},
	.virtualMobilityBonus = {
		P(   55,   78), P(   54,   36), P(   53,   25), P(   52,   11), P(   50,   10), 
		P(   49,    9), P(   43,    8), P(   37,    6), P(   35,    5), P(   29,    4), 
		P(   28,    3), P(   24,    2), P(   19,    1), P(   13,    0), P(    6,   -1), 
		P(   -3,   -2), P(  -15,   -3), P(  -24,   -4), P(  -33,   -5), P(  -43,   -6), 
		P(  -48,   -9), P(  -49,  -12), P(  -54,  -16), P(  -55,  -21), P(  -56,  -24), 
		P(  -57,  -33), P(  -58,  -34), P(  -59,  -35) 
	},
	.pawnAttacks = {
		P(   39,   22), P(   53,   45), P(   54,   19), P(   63,  -42) 
	},
	.minorPawnCover = P(    8,    3),
	.passedPawnBonus = {
		P(    0,    0), P(    1,  -19), P(    2,  -16), P(    6,   -1), P(   17,   30), P(   25,   92), P(   26,   93), P(    0,    0) 
	},
	.connectedPawnBonus = {
		P(    0,    0), P(   10,    8), P(    1,    1), P(    3,    1), P(    4,    7), P(   19,   15), P(  112,    2), P(    0,    0) 
	},
	.doubledPawnBonus = {
		P(   -7,  -26), P(    5,  -26), P(   -2,  -16), P(    0,   -8), P(   -3,   -3), P(   -1,  -17), P(    6,  -26), P(   -5,  -29) 
	},
	.isolatedPawnBonus = {
		P(  -11,   17), P(   -7,  -13), P(  -15,   -3), P(  -17,  -13), P(  -18,  -12), P(  -18,   -3), P(   -7,  -11), P(  -11,   15) 
	},
	.knightOutpostBonus = P(   33,    9),
	.bishopPairBonus = P(   17,   64),
	.badBishopBonus = P(   -2,   -5),
	.openRookFileBonus = { P(    8,   11), P(   29,    4) },
	.connectedRookBonus = { P(   -4,   10), P(   11,    2) },
	.connectedQueenBonus = { P(   -6,   43), P(    4,   31), P(    8,    6) },
	.tempo = P(   21,   22),
};


int initHCE = 0;

eval_t pieceBonusTable[PIECE_COUNT][64] = {0};

void init_HCE_tables()
{
    if(initHCE) return;
    initHCE = 1;

	//PSQT
	for(int piece = 0; piece < PIECE_COUNT; piece+=2)
	{
		for(int square = 0; square < 64; square++)
		{
			EVAL_ADD(pieceBonusTable[piece][square], hce_params.rawPieceTables[piece / 2][FLIP_SQUARE(square)]);
			EVAL_ADD(pieceBonusTable[piece][square], hce_params.genericPieceValues[piece / 2]);
			
			EVAL_ADD(pieceBonusTable[piece + 1][square], hce_params.rawPieceTables[piece / 2][MIRROR_SQUARE(square)]);
			EVAL_ADD(pieceBonusTable[piece + 1][square], hce_params.genericPieceValues[piece / 2]);
		}
	}
}

void evaluatePawns(bitboard* board, eval_t* score)
{
	eval_t eval = P(0, 0);

	uint64_t mask = board->pieces[WHITE_PAWN];
	int protectedCount = __builtin_popcountll((mask >> 8) & (board->pieces[WHITE_BISHOP] | board->pieces[WHITE_KNIGHT]));

	int pawnThreats = (~(board->pieces[BLACK_PAWN] | board->pieces[BLACK_KING])) & (WHITE_PAWN_LEFTATTACKS(board) | WHITE_PAWN_RIGHTATTACKS(board));
	while(pawnThreats)
	{
		int pc = findPieceOnSquare(board, __builtin_ctzll(pawnThreats));

		if(ISKNIGHT(pc))
			EVAL_ADD(eval, hce_params.pawnAttacks[ATTACKING_KNIGHT]);
		else if(ISBISHOP(pc))
			EVAL_ADD(eval, hce_params.pawnAttacks[ATTACKING_BISHOP]);
		else if(ISROOK(pc))
			EVAL_ADD(eval, hce_params.pawnAttacks[ATTACKING_ROOK]);
		else if(ISQUEEN(pc))
			EVAL_ADD(eval, hce_params.pawnAttacks[ATTACKING_QUEEN]);

		pawnThreats &= pawnThreats - 1;
	}

	while(mask)
	{
        int sq = __builtin_ctzll(mask);
        int column = getColumn(sq);
		int row = getRow(sq);

		//piece/square value
		EVAL_ADD(eval, pieceBonusTable[WHITE_PAWN][sq]);

		//Passed Pawns
        if((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
			EVAL_ADD(eval, hce_params.passedPawnBonus[row]);

        //Doubled pawns
        if(__builtin_popcountll(board->pieces[WHITE_PAWN] & board_file[column]) > 1)
			EVAL_ADD(eval, hce_params.doubledPawnBonus[row]);

		uint64_t borderingMask = board->pieces[WHITE_PAWN] & bordering_files[column];
        //Isolated Pawns
        if(!borderingMask)
			EVAL_ADD(eval, hce_params.isolatedPawnBonus[row]);
		//connected pawns
		else if(borderingMask & (board_rank[row - 1] | board_rank[row + 1]))
			EVAL_ADD(eval, hce_params.connectedPawnBonus[row]);

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_PAWN];

	protectedCount -= __builtin_popcountll((mask << 8) & (board->pieces[BLACK_BISHOP] | board->pieces[BLACK_KNIGHT]));

	pawnThreats = (~(board->pieces[WHITE_PAWN] | board->pieces[WHITE_KING])) & (BLACK_PAWN_LEFTATTACKS(board) | BLACK_PAWN_RIGHTATTACKS(board));
	while(pawnThreats)
	{
		int pc = findPieceOnSquare(board, __builtin_ctzll(pawnThreats));

		if(ISKNIGHT(pc))
			EVAL_SUB(eval, hce_params.pawnAttacks[ATTACKING_KNIGHT]);
		else if(ISBISHOP(pc))
			EVAL_SUB(eval, hce_params.pawnAttacks[ATTACKING_BISHOP]);
		else if(ISROOK(pc))
			EVAL_SUB(eval, hce_params.pawnAttacks[ATTACKING_ROOK]);
		else if(ISQUEEN(pc))
			EVAL_SUB(eval, hce_params.pawnAttacks[ATTACKING_QUEEN]);

		pawnThreats &= pawnThreats - 1;
	}

	while(mask)
	{
        int sq = __builtin_ctzll(mask);
        int column = getColumn(sq);
		int row = getRow(sq);
		
		int mirroredColumn = MIRROR_SQUARE(column);
		int mirroredRow = MIRROR_SQUARE(row);

		//piece/square value
		EVAL_SUB(eval, pieceBonusTable[BLACK_PAWN][sq]);

		//Passed Pawns
        if((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
			EVAL_SUB(eval, hce_params.passedPawnBonus[mirroredRow]);
        
        //Doubled pawns
        if(__builtin_popcountll(board->pieces[BLACK_PAWN] & board_file[column]) > 1)
			EVAL_SUB(eval, hce_params.doubledPawnBonus[mirroredColumn]);

		uint64_t borderingMask = board->pieces[BLACK_PAWN] & bordering_files[column];
        //Isolated Pawns
        if(!borderingMask)
			EVAL_SUB(eval, hce_params.isolatedPawnBonus[mirroredColumn]);
		//connected pawns
		else if(borderingMask & (board_rank[row - 1] | board_rank[row + 1]))
			EVAL_SUB(eval, hce_params.connectedPawnBonus[mirroredRow]);

		mask &= mask - 1;
	}
	
	EVAL_MADD(eval, hce_params.minorPawnCover, protectedCount);

	EVAL_ADD(*score, eval);
}

void evaluateKnights(bitboard* board, eval_t* score)
{
	eval_t eval = P(0, 0);

	uint64_t mask = board->pieces[WHITE_KNIGHT];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		int row = getRow(sq);

		//piece/square value
		EVAL_ADD(eval, pieceBonusTable[WHITE_KNIGHT][sq]);

		//mobility
		uint64_t moves = knightMoves(board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		EVAL_ADD(eval, hce_params.knightMobilityBonus[moveCount]);

		//outpost
		if(row >= 4 && row <= 6 && 
		   board->pieces[WHITE_PAWN] && board_rank[row - 1] &&
		   (bordering_files[column] & board->pieces[BLACK_PAWN] & (~(singleBitMask(sq + 7) - 1))) == 0)
				EVAL_ADD(eval, hce_params.knightOutpostBonus);

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_KNIGHT];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		int row = getRow(sq);
		
		//piece/square value
		EVAL_SUB(eval, pieceBonusTable[BLACK_KNIGHT][sq]);

		//mobility
		uint64_t moves = knightMoves(board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);

		EVAL_SUB(eval, hce_params.knightMobilityBonus[moveCount]);
		
		//outpost
		if(row >= 1 && row <= 3 &&
		   board->pieces[BLACK_PAWN] && board_rank[row + 1] &&
		   (bordering_files[column] & board->pieces[WHITE_PAWN] & ((singleBitMask(sq - 6) - 1))) == 0)
				EVAL_SUB(eval, hce_params.knightOutpostBonus);

		mask &= mask - 1;
	}
	
	EVAL_ADD(*score, eval);
}

void evaluateBishops(bitboard* board, eval_t* score)
{
	eval_t eval = P(0, 0);

	uint64_t mask = board->pieces[WHITE_BISHOP];

	//bad bishop-pawns
	int badPawns = 0;
	if(mask & LIGHT_SQUARES)
	 	badPawns += __builtin_popcountll(board->pieces[WHITE_PAWN] & LIGHT_SQUARES);
	if(mask & DARK_SQUARES)
	 	badPawns += __builtin_popcountll(board->pieces[WHITE_PAWN] & DARK_SQUARES);

	//bishop pair
	if(__builtin_popcountll(mask) >= 2)
		EVAL_ADD(eval, hce_params.bishopPairBonus);

	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
		EVAL_ADD(eval, pieceBonusTable[WHITE_BISHOP][sq]);

		//mobility
		uint64_t moves = bishopMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);
		EVAL_ADD(eval, hce_params.bishopMobilityBonus[moveCount]);

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_BISHOP];

	//bad bishop-pawns
	if(mask & LIGHT_SQUARES)
	 	badPawns -= __builtin_popcountll(board->pieces[BLACK_PAWN] & LIGHT_SQUARES);
	if(mask & DARK_SQUARES)
	 	badPawns -= __builtin_popcountll(board->pieces[BLACK_PAWN] & DARK_SQUARES);

	//bishop pair
	if(__builtin_popcountll(mask) >= 2)
		EVAL_SUB(eval, hce_params.bishopPairBonus);

	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/sq value
		EVAL_SUB(eval, pieceBonusTable[BLACK_BISHOP][sq]);
		
		//mobility
		uint64_t moves = bishopMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		EVAL_SUB(eval, hce_params.bishopMobilityBonus[moveCount]);

		mask &= mask - 1;
	}

	EVAL_MADD(eval, hce_params.badBishopBonus, badPawns);

	EVAL_ADD(*score, eval);
}

void evaluateRooks(bitboard* board, eval_t* score)
{
	eval_t eval = P(0, 0);
	int connectedRooksByRow = 0;
	int connectedRooksByColumn = 0;

	uint64_t mask = board->pieces[WHITE_ROOK];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		int row = getRow(sq);

		//piece/sq value
		EVAL_ADD(eval, pieceBonusTable[WHITE_ROOK][sq]);
		
		//open rook file
        if((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
        {
        	if((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
				EVAL_ADD(eval, hce_params.openRookFileBonus[OPEN_FILE]);
			else
				EVAL_ADD(eval, hce_params.openRookFileBonus[SEMI_OPEN_FILE]);
        }

		//mobility
		uint64_t moves = rookMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);
		EVAL_ADD(eval, hce_params.rookMobilityBonus[moveCount]);

        //rook rams
		uint64_t connections = rookMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		uint64_t connectedRooks = connections & board->pieces[WHITE_ROOK];
		connectedRooksByRow += __builtin_popcountll(connectedRooks & board_rank[row]);
		connectedRooksByColumn += __builtin_popcountll(connectedRooks & board_file[column]);

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_ROOK];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		int row = getRow(sq);
		
		//piece/sq value
		EVAL_SUB(eval, pieceBonusTable[BLACK_ROOK][sq]);
		
		//open rook file
        if((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
        {
            if((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
				EVAL_SUB(eval, hce_params.openRookFileBonus[OPEN_FILE]);
			else
				EVAL_SUB(eval, hce_params.openRookFileBonus[SEMI_OPEN_FILE]);
        }
		
		//mobility
		uint64_t moves = rookMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		EVAL_SUB(eval, hce_params.rookMobilityBonus[moveCount]);

        //rook rams
		uint64_t connections = rookMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		uint64_t connectedRooks = connections & board->pieces[BLACK_ROOK];
		connectedRooksByRow -= __builtin_popcountll(connectedRooks & board_rank[row]);
		connectedRooksByColumn -= __builtin_popcountll(connectedRooks & board_file[column]);

		mask &= mask - 1;
	}

	EVAL_MADD(eval, hce_params.connectedRookBonus[CONNECTED_ROW], connectedRooksByRow);
	EVAL_MADD(eval, hce_params.connectedRookBonus[CONNECTED_COLUMN], connectedRooksByColumn);
	
	EVAL_ADD(*score, eval);
}

void evaluateQueens(bitboard* board, eval_t* score)
{
	eval_t eval = P(0, 0);
	int connectedSlidersByRow = 0;
	int connectedSlidersByColumn = 0;
	int connectedSlidersByDiagonal = 0;

	uint64_t mask = board->pieces[WHITE_QUEEN];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		int row = getRow(sq);

		//piece/square value
		EVAL_ADD(eval, pieceBonusTable[WHITE_QUEEN][sq]);

		//mobility
		uint64_t moves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);
		EVAL_ADD(eval, hce_params.queenMobilityBonus[moveCount]);
		
        //queen & slider rams
		uint64_t rookConnections = rookMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		rookConnections &= (board->pieces[WHITE_QUEEN] | board->pieces[WHITE_ROOK]);
		connectedSlidersByRow += __builtin_popcountll(rookConnections & board_rank[row]);
		connectedSlidersByColumn += __builtin_popcountll(rookConnections & board_file[column]);

		uint64_t bishopConnections = bishopMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		bishopConnections &= (board->pieces[WHITE_QUEEN] | board->pieces[WHITE_BISHOP]);
		connectedSlidersByDiagonal += __builtin_popcountll(bishopConnections);

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_QUEEN];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		int row = getRow(sq);
		
		//piece/square value
		EVAL_SUB(eval, pieceBonusTable[BLACK_QUEEN][sq]);

		//mobility
		uint64_t moves = queenMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		EVAL_SUB(eval, hce_params.queenMobilityBonus[moveCount]);
		
        //queen & slider rams
		uint64_t rookConnections = rookMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		rookConnections &= (board->pieces[BLACK_QUEEN] | board->pieces[BLACK_ROOK]);
		connectedSlidersByRow -= __builtin_popcountll(rookConnections & board_rank[row]);
		connectedSlidersByColumn -= __builtin_popcountll(rookConnections & board_file[column]);

		uint64_t bishopConnections = bishopMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		bishopConnections &= (board->pieces[BLACK_QUEEN] | board->pieces[BLACK_BISHOP]);
		connectedSlidersByDiagonal -= __builtin_popcountll(bishopConnections);

		mask &= mask - 1;
	}
	
	EVAL_MADD(eval, hce_params.connectedQueenBonus[CONNECTED_ROW], connectedSlidersByRow);
	EVAL_MADD(eval, hce_params.connectedQueenBonus[CONNECTED_COLUMN], connectedSlidersByColumn);
	EVAL_MADD(eval, hce_params.connectedQueenBonus[CONNECTED_DIAGONAL], connectedSlidersByDiagonal);

	EVAL_ADD(*score, eval);
}

void evaluateKings(bitboard* board, eval_t* score)
{
	eval_t eval = P(0, 0);

	uint64_t mask = board->pieces[WHITE_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
		EVAL_ADD(eval, pieceBonusTable[WHITE_KING][sq]);

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
		EVAL_ADD(eval, hce_params.virtualMobilityBonus[virtualMoveCount]);

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/square value
		EVAL_SUB(eval, pieceBonusTable[BLACK_KING][sq]);

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
		EVAL_SUB(eval, hce_params.virtualMobilityBonus[virtualMoveCount]);

		mask &= mask - 1;
	}

	EVAL_ADD(*score, eval);
}

int gamephasePieceValues[PIECE_COUNT] = {0,0,1,1,1,1,2,2,4,4,0,0};
int evaluatePhasedScore(bitboard* board, eval_t eval)
{
    int phase = 24;
    for(int pc = 2; pc < PIECE_COUNT - 2; pc++)
    {
        phase -= gamephasePieceValues[pc] * __builtin_popcountll(board->pieces[pc]);
    }
	phase = clamp(phase, 0, 24);

	int eg_phase = (phase * 256 + 12) / 24;
	int mg_phase = 256 - eg_phase;
	return (mg_phase * eval.mg + eg_phase * eval.eg) / 256;
}

int hce_eval(bitboard* board)
{
	eval_t eval = hce_params.tempo;

    evaluatePawns(board, &eval);
    evaluateKnights(board, &eval);
    evaluateBishops(board, &eval);
    evaluateRooks(board, &eval);
    evaluateQueens(board, &eval);
    evaluateKings(board, &eval);

    int phasedEval = evaluatePhasedScore(board, eval);
	phasedEval = clamp(phasedEval, -(MIN_MATE_SCORE - 1), MIN_MATE_SCORE - 1);
	return (ISWHITE(board->turn)) ? phasedEval : -phasedEval;
}