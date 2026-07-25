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
		P(   54,  152), P(  266,  346), P(  304,  374), P(  373,  645), P(  794, 1211), P(    0,    0) 
	},
	.rawPieceTables = {
			[PAWN / 2] = {
				P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), 
				P(    7,   65), P(   44,   81), P(   57,   62), P(   80,   22), P(   65,   27), P(   37,   56), P(   -3,   92), P(  -20,   81), 
				P(    9,    6), P(   12,   39), P(   38,   -7), P(   26,  -38), P(   34,  -39), P(   45,  -24), P(    6,   32), P(    6,    8), 
				P(   -7,  -14), P(   -1,    7), P(    7,  -27), P(    4,  -37), P(   10,  -40), P(   -2,  -27), P(   -7,    2), P(   -8,  -16), 
				P(  -18,  -25), P(  -11,    7), P(   -8,  -23), P(    0,  -28), P(   -2,  -27), P(   -7,  -22), P(  -14,    2), P(  -19,  -24), 
				P(  -22,  -30), P(  -21,    2), P(  -18,  -19), P(  -15,  -16), P(  -17,  -15), P(  -21,  -15), P(  -29,    3), P(  -26,  -26), 
				P(  -27,  -26), P(  -13,    8), P(  -24,   -8), P(  -27,   -4), P(  -30,    1), P(  -23,   -7), P(  -18,    5), P(  -29,  -24), 
				P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0) 
			},
			[KNIGHT / 2] = {
				P( -121,  -68), P( -117,   -5), P(  -54,    2), P(  -20,    0), P(   13,   -2), P(  -32,   -7), P(  -63,  -16), P( -107,  -85), 
				P(  -32,  -26), P(  -21,   -9), P(   24,   -9), P(    4,   -5), P(    2,  -10), P(   35,  -20), P(  -24,   -1), P(  -21,  -33), 
				P(   -7,  -13), P(   21,   -5), P(   38,   14), P(   46,   10), P(   55,    9), P(   46,    5), P(   25,   -3), P(   -7,  -20), 
				P(    5,   -3), P(   19,   13), P(   45,   22), P(   39,   27), P(   37,   27), P(   44,   20), P(   19,   13), P(    7,   -9), 
				P(    5,    7), P(   18,   20), P(   31,   36), P(   32,   39), P(   36,   39), P(   28,   32), P(   29,   15), P(    1,    4), 
				P(  -12,   -5), P(   14,    8), P(   17,   14), P(   26,   32), P(   31,   26), P(   15,   15), P(    9,    6), P(  -13,   -6), 
				P(   -7,   -8), P(  -10,   -5), P(    3,    7), P(   10,    8), P(   10,    5), P(   -2,    8), P(  -14,   -3), P(   -7,  -10), 
				P(  -43,  -16), P(  -19,  -17), P(  -10,  -10), P(   -3,   -1), P(   -5,    0), P(   -3,  -10), P(  -18,  -21), P(  -45,  -17) 
			},
			[BISHOP / 2] = {
				P(  -40,    4), P(  -43,    5), P(  -54,    0), P(  -76,   14), P(  -74,    7), P(  -51,    5), P(  -32,    8), P(  -34,   -3), 
				P(  -12,   -5), P(    7,    1), P(   11,   -1), P(   11,   -8), P(    1,   -3), P(    4,   -4), P(    9,   -3), P(   -8,   -9), 
				P(   19,    5), P(   24,    5), P(   30,    5), P(   27,    0), P(   31,   -8), P(   36,    2), P(   36,   -4), P(   21,    0), 
				P(   -2,    3), P(    0,    9), P(   22,    7), P(   30,    9), P(   30,   16), P(   18,    7), P(    6,    3), P(  -11,    2), 
				P(   -3,   -8), P(   -1,    8), P(   -4,   12), P(   24,   11), P(   17,   10), P(    3,    8), P(   -8,    5), P(    1,   -8), 
				P(    6,   -8), P(    7,    0), P(    9,    6), P(   -2,   12), P(    5,    8), P(    3,    6), P(   10,    3), P(   -5,   -8), 
				P(   -4,  -15), P(   16,  -13), P(   11,  -19), P(    3,   -3), P(   -7,   -1), P(    7,   -8), P(    8,  -10), P(   -1,   -6), 
				P(    7,  -19), P(   12,  -28), P(   -8,   -4), P(   -9,   -3), P(   -7,   -2), P(  -16,    0), P(    2,   18), P(  -12,  -16) 
			},
			[ROOK / 2] = {
				P(   22,    9), P(    9,    9), P(    6,   19), P(   11,   10), P(    9,   13), P(   21,   13), P(    6,   20), P(   15,   11), 
				P(   13,    9), P(   13,   17), P(   29,   17), P(   25,   12), P(   23,   13), P(   26,   13), P(    3,   18), P(   13,   10), 
				P(   -6,    9), P(   19,    6), P(   14,    6), P(    6,    4), P(   14,    3), P(    6,    5), P(   28,    5), P(    1,    7), 
				P(  -10,    8), P(    0,    3), P(    0,    2), P(   -2,    2), P(   -6,    2), P(   -6,    8), P(   -2,    3), P(   -7,    5), 
				P(  -15,    0), P(  -13,    1), P(  -18,    2), P(   -8,   -2), P(  -13,    0), P(  -22,    1), P(  -17,   -1), P(  -22,    2), 
				P(  -16,   -9), P(   -3,  -13), P(  -12,  -11), P(   -9,  -10), P(  -14,   -7), P(  -11,  -10), P(    1,  -21), P(  -11,   -9), 
				P(  -20,   -7), P(  -10,  -15), P(   -6,  -11), P(   -9,   -9), P(   -9,  -10), P(   -9,  -11), P(   -4,  -15), P(  -19,  -12), 
				P(   -6,  -12), P(    0,  -17), P(   -2,  -12), P(    4,  -17), P(    4,  -18), P(    0,  -15), P(   -2,  -14), P(   -3,  -12) 
			},
			[QUEEN / 2] = {
				P(  -14,  -10), P(   12,  -12), P(   -8,   23), P(    4,   12), P(    5,   11), P(  -11,   25), P(   -6,   -8), P(  -20,    7), 
				P(   22,   -7), P(   -6,   17), P(    3,   36), P(  -35,   71), P(  -29,   63), P(  -12,   40), P(  -13,   20), P(   12,    4), 
				P(    5,    1), P(    3,   17), P(   -3,   32), P(   -6,   32), P(  -10,   48), P(   -2,   34), P(   21,  -14), P(   10,    0), 
				P(  -12,   10), P(  -11,   14), P(   -8,   18), P(   -8,   34), P(   -9,   34), P(  -10,   25), P(   -8,   22), P(   -8,   12), 
				P(    0,   -6), P(    6,    5), P(   -6,   17), P(   -5,   27), P(    2,   17), P(   -5,    4), P(    3,    3), P(   -1,    1), 
				P(    3,  -17), P(    8,   -8), P(    3,    3), P(   -1,    1), P(    2,    1), P(    4,    1), P(    7,   -4), P(    4,  -15), 
				P(    8,  -46), P(   11,  -44), P(   10,  -33), P(   12,  -14), P(    9,  -14), P(   14,  -34), P(   14,  -41), P(    8,  -31), 
				P(    2,  -44), P(    6,  -60), P(    7,  -59), P(   15,  -55), P(   10,  -43), P(    0,  -43), P(    7,  -50), P(   -7,  -30) 
			},
			[KING / 2] = {
				P(  137, -118), P(  123,  -49), P(   44,  -11), P(  -30,    4), P(  -30,    7), P(   37,  -26), P(  -20,  -20), P(   55,  -99), 
				P(    0,   -8), P(   26,   37), P(   -1,   52), P(   49,   38), P(   34,   37), P(  -24,   46), P(  -10,   36), P(  -97,    6), 
				P(  -30,    9), P(   28,   46), P(   57,   59), P(    7,   69), P(  -17,   75), P(    3,   61), P(   25,   43), P(  -83,   14), 
				P(  -87,    9), P(  -46,   43), P(  -44,   66), P(  -58,   79), P(  -82,   80), P(  -50,   63), P(  -51,   43), P( -108,   12), 
				P( -102,    2), P(  -47,   23), P(  -53,   49), P(  -67,   65), P(  -80,   70), P(  -50,   47), P(  -70,   30), P( -112,    3), 
				P(  -29,  -15), P(    7,    1), P(  -23,   28), P(  -32,   40), P(  -25,   38), P(  -24,   25), P(    2,    0), P(  -31,  -19), 
				P(   78,  -80), P(   72,  -57), P(   57,  -43), P(   49,  -31), P(   49,  -32), P(   51,  -40), P(   63,  -49), P(   70,  -75), 
				P(   66, -114), P(   67,  -83), P(   51,  -71), P(   70,  -79), P(   61,  -74), P(   50,  -68), P(   65,  -79), P(   64, -112) 
			}
	},
	.knightMobilityBonus = {
		P(  -25,  -25), P(   -9,   -6), P(   -2,    1), P(    3,    1), P(    7,    2), P(    7,    7), P(    7,    7), P(    7,    7), 
		P(    7,    7) 
	},
	.bishopMobilityBonus = {
		P(  -26,  -55), P(  -17,  -35), P(  -11,  -26), P(  -10,  -16), P(   -5,   -5), P(    1,    7), P(    5,    8), P(    8,   13), 
		P(    8,   18), P(    9,   18), P(    9,   18), P(   10,   18), P(   10,   18), P(   10,   18) 
	},
	.rookMobilityBonus = {
		P(  -17,  -40), P(   -8,  -24), P(   -5,  -20), P(   -2,  -15), P(   -2,  -13), P(   -1,   -4), P(    1,   -2), P(    3,    1), 
		P(    3,    8), P(    3,   13), P(    4,   14), P(    6,   16), P(    6,   20), P(    6,   22), P(    6,   22) 
	},
	.queenMobilityBonus = {
		P(  -31, -105), P(   -5, -105), P(   -5, -105), P(   -5,  -78), P(   -4,  -64), P(   -2,  -54), P(    2,  -46), P(    2,  -34), 
		P(    2,  -21), P(    2,  -14), P(    2,   -3), P(    2,    1), P(    2,    8), P(    2,   13), P(    2,   19), P(    2,   23), 
		P(    2,   30), P(    2,   35), P(    2,   42), P(    2,   44), P(    4,   47), P(    4,   48), P(    4,   48), P(    4,   49), 
		P(    4,   49), P(    4,   56), P(    4,   56), P(    4,   58) 
	},
	.virtualMobilityBonus = {
		P(   55,   54), P(   55,   23), P(   55,   19), P(   55,    8), P(   54,    8), P(   54,    8), P(   48,    6), P(   44,    3), 
		P(   42,    3), P(   35,    3), P(   33,    3), P(   29,    3), P(   24,    3), P(   16,    3), P(    8,    3), P(   -4,    3), 
		P(  -21,    3), P(  -34,    3), P(  -44,    2), P(  -47,   -3), P(  -51,   -6), P(  -53,   -9), P(  -59,  -13), P(  -59,  -18), 
		P(  -59,  -20), P(  -59,  -28), P(  -59,  -32), P(  -59,  -32) 
	},
	.minorPawnCover = P(    8,    3),
	.passedPawnBonus = {
		P(    0,    0), P(    1,  -16), P(    2,  -15), P(    5,    0), P(   18,   32), P(   25,   93), P(   25,   93), P(    0,    0) 
	},
	.connectedPawnBonus = {
		P(    0,    0), P(    9,    9), P(    2,    1), P(    3,    1), P(    4,    7), P(   18,   15), P(  116,    2), P(    0,    0) 
	},
	.doubledPawnBonus = {
		P(    2,  -30), P(   13,  -30), P(   11,  -22), P(   15,  -16), P(   11,   -9), P(   12,  -23), P(   14,  -29), P(    4,  -32) 
	},
	.isolatedPawnBonus = {
		P(   -5,   15), P(   -9,  -12), P(  -13,   -4), P(  -18,  -13), P(  -18,  -13), P(  -16,   -4), P(   -9,  -10), P(   -5,   12) 
	},
	.knightOutpostBonus = P(   35,   12),
	.bishopPairBonus = P(   19,   62),
	.badBishopBonus = P(   -2,   -5),
	.openRookFileBonus = { P(    8,   10), P(   30,    5) },
	.connectedRookBonus = { P(   -4,    8), P(   11,    1) },
	.connectedQueenBonus = { P(   -7,   42), P(    4,   33), P(    7,    7) },
	.kingPawnShieldBonus = {
		P(   31,  -26), P(   27,  -19), P(   30,  -15), P(   -1,   -9), P(   12,  -12), P(   11,   -9), P(   23,  -17), P(   32,  -25) 
	},
	.kingPawnStormBonus = {
		P(    0,    0), P(   15,    1), P(    7,    9), P(   12,   -1), P(    7,    2), P(   14,   -4), P(   12,   -2), P(    0,    0) 
	},
	.openKingFile = { P(   -9,    6), P(   -3,    3) },
	.kingSafety = {
		P(    0,    1), P(    0,    1), P(    4,    1), P(    4,    1), P(    4,    1), P(    4,    1), P(    4,    1), P(    8,    1), P(    9,    1), P(   12,    1), 
		P(   12,    1), P(   21,    1), P(   21,    1), P(   29,    1), P(   29,    1), P(   29,    1), P(   34,    1), P(   38,    1), P(   47,    1), P(   54,    1), 
		P(   54,    1), P(   63,    1), P(   63,    1), P(   72,    1), P(   74,    1), P(   74,    1), P(   75,    1), P(   88,    1), P(   95,    1), P(  113,    1), 
		P(  113,    1), P(  113,    1), P(  122,    1), P(  132,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), 
		P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), 
		P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), 
		P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), 
		P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), 
		P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), 
		P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1), P(  170,    1) 
	},
	.tempo = P(   19,   22),
};



eval_t pieceBonusTable[PIECE_COUNT][64] = {0};
uint64_t kingPawnShieldMask[2][COLUMN_COUNT];
uint64_t kingPawnStormMask[COLUMN_COUNT];
uint64_t kingZone[2][64];
int32_t gamephasePieceValues[PIECE_COUNT] = {0,0,1,1,1,1,2,2,4,4,0,0};
int attackWeight[8] = {0, 0, 50, 75, 88, 94, 97, 99};

int initHCE = 0;
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
            if(target_column >= 0 && target_column <= 7) 
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

	for(int sq = 0; sq < 64; sq++)
	{
		kingZone[WHITE][sq] = singleBitMask(sq) | kingAttacks[sq];
		kingZone[BLACK][sq] = singleBitMask(sq) | kingAttacks[sq];

		if(sq < 16)
			kingZone[WHITE][sq] |= kingAttacks[sq + 8];

		if(sq > 47)
			kingZone[BLACK][sq] |= kingAttacks[sq - 8];
	}
}

void evaluatePawns(bitboard* board, eval_t* score, evalContext* context)
{
	eval_t eval = P(0, 0);

	uint64_t mask = board->pieces[WHITE_PAWN];
	int protectedCount = __builtin_popcountll((mask >> 8) & (board->pieces[WHITE_BISHOP] | board->pieces[WHITE_KNIGHT]));

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

void evaluateKnights(bitboard* board, eval_t* score, evalContext* context)
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

		//king safety
		context->attackWeight[WHITE] += KNIGHT_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[BLACK]);

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
		
		//king safety
		context->attackWeight[BLACK] += KNIGHT_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[WHITE]);
		
		//outpost
		if(row >= 1 && row <= 3 &&
		   board->pieces[BLACK_PAWN] && board_rank[row + 1] &&
		   (bordering_files[column] & board->pieces[WHITE_PAWN] & ((singleBitMask(sq - 6) - 1))) == 0)
				EVAL_SUB(eval, hce_params.knightOutpostBonus);

		mask &= mask - 1;
	}
	
	EVAL_ADD(*score, eval);
}

void evaluateBishops(bitboard* board, eval_t* score, evalContext* context)
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

		//king safety
		context->attackWeight[WHITE] += BISHOP_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[BLACK]);

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

		//king safety
		context->attackWeight[BLACK] += BISHOP_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[WHITE]);

		mask &= mask - 1;
	}

	EVAL_MADD(eval, hce_params.badBishopBonus, badPawns);

	EVAL_ADD(*score, eval);
}

void evaluateRooks(bitboard* board, eval_t* score, evalContext* context)
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

		//king safety
		context->attackWeight[WHITE] += ROOK_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[BLACK]);

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

		//king safety
		context->attackWeight[BLACK] += ROOK_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[WHITE]);

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

void evaluateQueens(bitboard* board, eval_t* score, evalContext* context)
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
		
		//king safety
		context->attackWeight[WHITE] += QUEEN_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[BLACK]);
		
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
		
		//king safety
		context->attackWeight[BLACK] += QUEEN_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[WHITE]);
		
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

void evaluateKings(bitboard* board, eval_t* score, evalContext* context)
{
	eval_t eval = P(0, 0);

	int semiOpenFileCount = 0;
	int openFileCount = 0;

	uint64_t mask = board->pieces[WHITE_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int row = getRow(sq);
		int column = getColumn(sq);

		//piece/square value
		EVAL_ADD(eval, pieceBonusTable[WHITE_KING][sq]);

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		EVAL_ADD(eval, hce_params.virtualMobilityBonus[virtualMoveCount]);

		if(row <= 1)
		{
			//pawn shield
			int pawnShieldCount = __builtin_popcountll(kingPawnShieldMask[WHITE][column] & board->pieces[WHITE_PAWN]);
			EVAL_MADD(eval, hce_params.kingPawnShieldBonus[column], pawnShieldCount);

			//pawn storm
			for(uint64_t stormMask = kingPawnStormMask[column] & board->pieces[BLACK_PAWN]; stormMask > 0; stormMask &= stormMask - 1)
				EVAL_ADD(eval, hce_params.kingPawnStormBonus[	getRow(	__builtin_ctzll(stormMask)	)	]);

			//(semi) open file
			for(int column = 0; column < 8; column++) 
			{
				for(int column_offset = -1; column_offset <= 1; column_offset++) 
				{
					int target_column = column + column_offset;
					if(target_column >= 0 && target_column <= 7) 
					{
						if((board->pieces[WHITE_PAWN] & board_file[target_column]) == 0)
						{
							if((board->pieces[BLACK_PAWN] & board_file[target_column]) == 0)
								openFileCount++;
							else
								semiOpenFileCount++;
						}
					}
				}
			}
		}

		
		mask &= mask - 1;
	}
	
	//Kingside/Queenside castles are symmetrical so don't mirror column.
	mask = board->pieces[BLACK_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int row = getRow(sq);
		int column = getColumn(sq);

		int mirroredRow = MIRROR_SQUARE(row);

		//piece/square value
		EVAL_SUB(eval, pieceBonusTable[BLACK_KING][sq]);

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		EVAL_SUB(eval, hce_params.virtualMobilityBonus[virtualMoveCount]);
		
		if(mirroredRow <= 1)
		{
			//pawn shield
			int pawnShieldCount = -1 * __builtin_popcountll(kingPawnShieldMask[BLACK][column] & board->pieces[BLACK_PAWN]);
			EVAL_MADD(eval, hce_params.kingPawnShieldBonus[column], pawnShieldCount);

			//pawn storm
			for(uint64_t stormMask = kingPawnStormMask[column] & board->pieces[WHITE_PAWN]; stormMask > 0; stormMask &= stormMask - 1)
				EVAL_SUB(eval, hce_params.kingPawnStormBonus[	MIRROR_SQUARE(	getRow(	__builtin_ctzll(stormMask)	)	)	]);
			
			//(semi) open file
			for(int column = 0; column < 8; column++) 
			{
				for(int column_offset = -1; column_offset <= 1; column_offset++) 
				{
					int target_column = column + column_offset;
					if(target_column >= 0 && target_column <= 7)
					{
						if((board->pieces[BLACK_PAWN] & board_file[target_column]) == 0)
						{
							if((board->pieces[WHITE_PAWN] & board_file[target_column]) == 0)
								openFileCount--;
							else
								semiOpenFileCount--;
						}
					}
				}
			}
		}

		mask &= mask - 1;
	}

	EVAL_MADD(eval, hce_params.openKingFile[SEMI_OPEN_FILE], semiOpenFileCount);
	EVAL_MADD(eval, hce_params.openKingFile[OPEN_FILE], openFileCount);

	//king safety
	context->attackWeight[WHITE] = _min(context->attackWeight[WHITE], 99);
	context->attackWeight[BLACK] = _min(context->attackWeight[BLACK], 99);
	eval_t kingSafety = hce_params.kingSafety[context->attackWeight[WHITE]];
	EVAL_SUB(kingSafety, hce_params.kingSafety[context->attackWeight[BLACK]]);
	EVAL_ADD(eval, kingSafety);

	EVAL_ADD(*score, eval);
}

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
	evalContext context = {
		.kingZone = {
			[WHITE] = kingZone[WHITE][board->kingSquare[WHITE]],
			[BLACK] = kingZone[BLACK][board->kingSquare[BLACK]]
		}
	};

    evaluatePawns(board, &eval, &context);
    evaluateKnights(board, &eval, &context);
    evaluateBishops(board, &eval, &context);
    evaluateRooks(board, &eval, &context);
    evaluateQueens(board, &eval, &context);
    evaluateKings(board, &eval, &context);

    int phasedEval = evaluatePhasedScore(board, eval);
	phasedEval = clamp(phasedEval, -(MIN_MATE_SCORE - 1), MIN_MATE_SCORE - 1);
	return (ISWHITE(board->turn)) ? phasedEval : -phasedEval;
}