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
		P(   100,  100), P(  300,  300), P(  300,  300), P(  500,  500), P(  900, 900), P(    0,    0) 
	},
	.tempo = P(20, 20)
};
 * 
 */
evalParameters hce_params = {
	.genericPieceValues = {
		P(   71,  145), P(  273,  339), P(  314,  365), P(  389,  630), P(  907, 1133), P(    0,    0) 
	},
	.rawPieceTables = {
			[PAWN / 2] = {
				P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), 
				P(   -1,   68), P(   38,   82), P(   55,   61), P(   77,   23), P(   63,   27), P(   32,   56), P(  -14,   94), P(  -27,   83), 
				P(    3,   10), P(    9,   41), P(   36,   -7), P(   26,  -36), P(   33,  -37), P(   43,  -23), P(    1,   35), P(    0,   11), 
				P(  -10,  -13), P(    0,    6), P(   10,  -29), P(    7,  -39), P(   14,  -42), P(    0,  -29), P(   -7,    2), P(  -11,  -15), 
				P(  -20,  -24), P(  -10,    5), P(   -3,  -26), P(    6,  -31), P(    4,  -30), P(   -3,  -24), P(  -12,    2), P(  -22,  -23), 
				P(  -23,  -28), P(  -18,    1), P(  -12,  -21), P(  -10,  -18), P(  -12,  -17), P(  -17,  -16), P(  -26,    4), P(  -29,  -24), 
				P(  -28,  -25), P(  -10,    7), P(  -18,  -11), P(  -21,   -6), P(  -24,   -2), P(  -18,   -9), P(  -15,    6), P(  -32,  -22), 
				P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0) 
			},
			[KNIGHT / 2] = {
				P( -124,  -59), P( -113,   -2), P(  -53,    3), P(  -21,    1), P(   17,   -2), P(  -36,   -5), P(  -62,  -14), P( -111,  -74), 
				P(  -30,  -20), P(  -22,   -6), P(   24,  -10), P(    8,   -6), P(    7,  -13), P(   37,  -21), P(  -24,    2), P(  -17,  -27), 
				P(    0,  -11), P(   20,   -7), P(   36,   10), P(   47,    6), P(   55,    5), P(   45,    1), P(   23,   -4), P(   -2,  -18), 
				P(    8,    0), P(   19,   11), P(   44,   18), P(   40,   23), P(   38,   23), P(   42,   17), P(   19,   12), P(   11,   -7), 
				P(    6,   10), P(   21,   16), P(   29,   31), P(   30,   34), P(   34,   34), P(   25,   27), P(   33,   11), P(    1,    7), 
				P(  -14,   -2), P(   12,    4), P(   14,    9), P(   23,   27), P(   28,   21), P(   12,   11), P(    7,    2), P(  -14,   -4), 
				P(   -8,   -4), P(  -12,   -3), P(    1,    3), P(    9,    5), P(    8,    2), P(   -4,    5), P(  -15,   -1), P(   -7,   -6), 
				P(  -44,  -11), P(  -19,  -13), P(  -10,   -8), P(   -2,    1), P(   -5,    2), P(   -3,   -8), P(  -18,  -16), P(  -45,  -12) 
			},
			[BISHOP / 2] = {
				P(  -41,    5), P(  -45,    7), P(  -56,    1), P(  -76,   16), P(  -72,    8), P(  -52,    7), P(  -32,    8), P(  -38,    0), 
				P(  -12,   -5), P(    7,   -1), P(   10,   -2), P(   11,   -8), P(    2,   -2), P(    5,   -5), P(   10,   -3), P(   -7,  -10), 
				P(   24,    3), P(   23,    5), P(   34,    3), P(   28,    0), P(   31,   -8), P(   38,    1), P(   35,   -4), P(   22,    0), 
				P(    0,    3), P(    2,    8), P(   23,    6), P(   32,    7), P(   32,   13), P(   19,    5), P(    7,    3), P(  -10,    2), 
				P(   -2,   -8), P(   -1,    9), P(   -3,   12), P(   27,   10), P(   20,    8), P(    3,    8), P(   -8,    5), P(    3,   -8), 
				P(    4,   -6), P(    6,    0), P(    9,    6), P(   -2,   11), P(    5,    8), P(    2,    6), P(    8,    3), P(   -8,   -6), 
				P(   -6,  -14), P(   14,  -12), P(   11,  -19), P(    2,   -3), P(   -9,   -2), P(    6,   -9), P(    6,   -9), P(   -3,   -4), 
				P(    6,  -17), P(   14,  -27), P(   -9,   -3), P(  -10,   -4), P(   -8,   -2), P(  -18,    1), P(    2,   18), P(  -14,  -14) 
			},
			[ROOK / 2] = {
				P(   27,    8), P(   10,   10), P(    8,   19), P(   16,    9), P(   15,   12), P(   25,   12), P(    8,   19), P(   18,    9), 
				P(   12,    9), P(   15,   16), P(   28,   17), P(   27,   11), P(   25,   12), P(   27,   13), P(    4,   17), P(   13,    9), 
				P(   -2,    8), P(   21,    6), P(   18,    5), P(   10,    3), P(   18,    3), P(   10,    4), P(   28,    4), P(    5,    5), 
				P(  -11,    8), P(   -1,    4), P(   -1,    3), P(   -5,    3), P(  -10,    4), P(   -9,    9), P(   -4,    5), P(   -7,    6), 
				P(  -17,    1), P(  -12,    2), P(  -18,    3), P(  -11,   -1), P(  -17,    1), P(  -23,    2), P(  -16,   -1), P(  -22,    2), 
				P(  -18,   -8), P(   -3,  -12), P(  -13,  -10), P(  -12,   -9), P(  -17,   -6), P(  -11,  -10), P(    4,  -21), P(  -12,   -9), 
				P(  -23,   -6), P(  -12,  -13), P(   -8,  -10), P(  -11,   -9), P(  -12,   -9), P(  -10,  -11), P(   -6,  -15), P(  -22,  -12), 
				P(  -10,  -12), P(   -1,  -18), P(   -2,  -13), P(    3,  -20), P(    2,  -20), P(   -2,  -16), P(   -3,  -14), P(   -6,  -12) 
			},
			[QUEEN / 2] = {
				P(  -15,   -3), P(   13,   -7), P(    0,   26), P(   19,   16), P(   19,   17), P(   -2,   28), P(   -1,   -4), P(  -16,    9), 
				P(   20,   -8), P(  -15,   20), P(    5,   37), P(  -30,   71), P(  -22,   63), P(   -9,   41), P(  -19,   21), P(   12,    2), 
				P(   32,  -13), P(   24,    5), P(    6,   35), P(    4,   35), P(    6,   45), P(   11,   36), P(   48,  -26), P(   37,  -11), 
				P(   10,   -2), P(   -6,   16), P(    0,   19), P(  -10,   38), P(  -10,   37), P(   -3,   28), P(   -2,   25), P(   12,    7), 
				P(   -6,    2), P(    2,    9), P(  -13,   22), P(  -12,   28), P(   -6,   19), P(   -9,    7), P(   -1,    7), P(  -10,   13), 
				P(   -2,  -17), P(   -3,   -4), P(   -5,    3), P(  -11,    0), P(   -8,   -1), P(   -4,    2), P(   -4,    0), P(   -1,  -15), 
				P(   -1,  -46), P(    2,  -43), P(    0,  -34), P(    1,  -19), P(   -2,  -17), P(    3,  -35), P(    3,  -40), P(   -2,  -32), 
				P(   -8,  -45), P(    0,  -63), P(   -2,  -60), P(    5,  -62), P(    1,  -52), P(   -8,  -46), P(    0,  -52), P(  -16,  -31) 
			},
			[KING / 2] = {
				P(   85, -118), P(  114,  -57), P(   66,  -26), P(    0,  -14), P(   -1,   -9), P(   53,  -38), P(   10,  -36), P(   46, -103), 
				P(   17,  -18), P(   29,   26), P(   17,   36), P(   66,   21), P(   53,   20), P(  -10,   32), P(   -1,   24), P(  -69,   -8), 
				P(  -17,   -2), P(   31,   34), P(   68,   45), P(   30,   52), P(    8,   58), P(   18,   48), P(   30,   33), P(  -65,    3), 
				P(  -70,   -1), P(  -47,   33), P(  -34,   53), P(  -43,   65), P(  -67,   66), P(  -43,   51), P(  -51,   35), P(  -88,    3), 
				P(  -86,   -8), P(  -54,   16), P(  -52,   39), P(  -58,   55), P(  -69,   59), P(  -49,   40), P(  -70,   24), P(  -95,   -5), 
				P(  -20,  -22), P(   -5,   -3), P(  -29,   21), P(  -26,   29), P(  -19,   28), P(  -29,   20), P(  -13,    1), P(  -20,  -23), 
				P(   59,  -50), P(   32,  -21), P(   17,   -6), P(    9,    3), P(   11,    1), P(   15,   -5), P(   29,  -16), P(   54,  -48), 
				P(   55,  -85), P(   44,  -52), P(   27,  -39), P(   44,  -46), P(   39,  -44), P(   26,  -35), P(   45,  -51), P(   54,  -85) 
			}
	},
	.knightMobilityBonus = {
		P(  -31,  -27), P(  -13,  -11), P(   -4,   -3), P(    2,   -2), P(    8,    1), P(    9,   10), P(   10,   11), P(   11,   12), 
		P(   12,   13) 
	},
	.bishopMobilityBonus = {
		P(  -28,  -58), P(  -19,  -37), P(  -13,  -28), P(  -11,  -17), P(   -6,   -5), P(    0,    6), P(    5,    8), P(    8,   13), 
		P(    9,   18), P(   10,   19), P(   11,   20), P(   12,   21), P(   13,   22), P(   14,   23) 
	},
	.rookMobilityBonus = {
		P(  -22,  -39), P(  -11,  -24), P(   -7,  -21), P(   -4,  -17), P(   -3,  -15), P(   -2,   -5), P(    0,   -3), P(    2,    1), 
		P(    3,    9), P(    4,   13), P(    5,   15), P(    8,   18), P(    9,   21), P(   12,   23), P(   13,   24) 
	},
	.queenMobilityBonus = {
		P(  -45, -105), P(  -15, -104), P(  -14, -103), P(  -13,  -79), P(  -12,  -60), P(   -9,  -51), P(   -5,  -45), P(   -4,  -35), 
		P(   -3,  -23), P(   -2,  -17), P(   -1,   -7), P(    0,   -2), P(    1,    4), P(    2,   10), P(    3,   16), P(    4,   21), 
		P(    5,   27), P(    6,   33), P(    7,   39), P(    8,   42), P(    9,   48), P(   10,   49), P(   11,   50), P(   12,   52), 
		P(   13,   53), P(   14,   61), P(   15,   62), P(   16,   63) 
	},
	.virtualMobilityBonus = {
		P(   50,   60), P(   49,   32), P(   48,   28), P(   47,   18), P(   46,   17), P(   45,   16), P(   42,   10), P(   38,    8), 
		P(   37,    7), P(   30,    6), P(   29,    5), P(   26,    4), P(   22,    3), P(   15,    2), P(    8,    1), P(   -2,    0), 
		P(  -15,   -1), P(  -25,   -2), P(  -35,   -3), P(  -41,   -6), P(  -46,  -10), P(  -47,  -13), P(  -52,  -18), P(  -53,  -24), 
		P(  -54,  -27), P(  -55,  -37), P(  -56,  -41), P(  -57,  -42) 
	},
	.pawnAttacks = {
		P(   41,   21), P(   55,   44), P(   55,   17), P(   64,  -42) 
	},
	.minorPawnCover = P(    8,    2),
	.passedPawnBonus = {
		P(    0,    0), P(    6,  -21), P(    7,  -19), P(    9,   -4), P(   21,   26), P(   30,   85), P(   31,   86), P(    0,    0) 
	},
	.connectedPawnBonus = {
		P(    0,    0), P(    8,    8), P(    2,    0), P(    3,    1), P(    5,    7), P(   22,   13), P(   99,    6), P(    0,    0) 
	},
	.doubledPawnBonus = {
		P(   -6,  -27), P(    3,  -26), P(   -1,  -17), P(    3,  -10), P(   -2,   -3), P(    1,  -18), P(    5,  -25), P(   -3,  -29) 
	},
	.isolatedPawnBonus = {
		P(   -7,   16), P(   -9,  -12), P(  -13,   -3), P(  -18,  -13), P(  -18,  -12), P(  -16,   -3), P(   -9,  -10), P(   -7,   14) 
	},
	.knightOutpostBonus = P(   32,   10),
	.bishopPairBonus = P(   18,   63),
	.badBishopBonus = P(   -2,   -5),
	.openRookFileBonus = { P(    9,   10), P(   29,    4) },
	.connectedRookBonus = { P(   -5,   10), P(   11,    2) },
	.connectedQueenBonus = { P(   -8,   44), P(    4,   32), P(    7,    7) },
	.kingPawnShieldBonus = {
		P(   26,  -17), P(   27,  -17), P(   33,  -17), P(    2,  -10), P(   15,  -14), P(   12,  -12), P(   25,  -17), P(   32,  -25) 
	},
	.kingPawnStormBonus = {
		P(    0,    0), P(   18,    0), P(   10,    9), P(   16,    3), P(   11,    5), P(   17,   -2), P(   15,   -3), P(    0,    0) 
	},
	.tempo = P(   21,   21),
};

int initHCE = 0;

eval_t pieceBonusTable[PIECE_COUNT][64] = {0};

uint64_t kingPawnShieldMask[2][COLUMN_COUNT];
uint64_t kingPawnStormMask[COLUMN_COUNT];

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

	for(int column = 0; column < 8; column++) 
	{
        for(int column_offset = -1; column_offset <= 1; column_offset++) 
		{
            int target_column = column + column_offset;
            if(target_column >= 0 && target_column < 8) 
			{
                // Ranks 2 & 3 
                kingPawnShieldMask[WHITE][column] |= singleBitMask(1 * 8 + target_column);
                kingPawnShieldMask[WHITE][column] |= singleBitMask(2 * 8 + target_column);
                
				//Ranks 7 & 6
                kingPawnShieldMask[BLACK][column] |= singleBitMask(6 * 8 + target_column);
                kingPawnShieldMask[BLACK][column] |= singleBitMask(5 * 8 + target_column);
            }
        }

		kingPawnStormMask[column] = bordering_files[column] | board_file[column];
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
		int column = getColumn(sq);

		//piece/square value
		EVAL_ADD(eval, pieceBonusTable[WHITE_KING][sq]);

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
		EVAL_ADD(eval, hce_params.virtualMobilityBonus[virtualMoveCount]);

		int pawnShieldCount = __builtin_popcountll(kingPawnShieldMask[WHITE][column] & board->pieces[WHITE_PAWN]);
		EVAL_MADD(eval, hce_params.kingPawnShieldBonus[column], pawnShieldCount);

		for(uint64_t stormMask = kingPawnStormMask[column] & board->pieces[BLACK_PAWN]; stormMask > 0; stormMask &= stormMask - 1)
			EVAL_ADD(eval, hce_params.kingPawnStormBonus[	getRow(	__builtin_ctzll(stormMask)	)	]);

		mask &= mask - 1;
	}
	
	//Kingside/Queenside castles are symmetrical so don't mirror column.
	mask = board->pieces[BLACK_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);

		//piece/square value
		EVAL_SUB(eval, pieceBonusTable[BLACK_KING][sq]);

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
		EVAL_SUB(eval, hce_params.virtualMobilityBonus[virtualMoveCount]);
		
		int pawnShieldCount = -1 * __builtin_popcountll(kingPawnShieldMask[BLACK][column] & board->pieces[BLACK_PAWN]);
		EVAL_MADD(eval, hce_params.kingPawnShieldBonus[column], pawnShieldCount);

		for(uint64_t stormMask = kingPawnStormMask[column] & board->pieces[WHITE_PAWN]; stormMask > 0; stormMask &= stormMask - 1)
			EVAL_SUB(eval, hce_params.kingPawnStormBonus[	MIRROR_SQUARE(	getRow(	__builtin_ctzll(stormMask)	)	)	]);

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