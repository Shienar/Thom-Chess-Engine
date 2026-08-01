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
		P(   55,  147), P(  269,  346), P(  307,  372), P(  376,  643), P(  810, 1200), P(    0,    0) 
	},
	.rawPieceTables = {
			[PAWN / 2] = {
				P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), 
				P(   12,   67), P(   53,   80), P(   63,   64), P(   85,   24), P(   70,   30), P(   44,   59), P(    6,   93), P(  -17,   85), 
				P(   12,    8), P(   17,   39), P(   42,   -8), P(   29,  -38), P(   38,  -39), P(   49,  -23), P(   11,   33), P(    9,   10), 
				P(   -6,  -15), P(   -2,    5), P(    4,  -29), P(    1,  -40), P(    8,  -43), P(   -4,  -29), P(   -8,    0), P(  -10,  -17), 
				P(  -21,  -27), P(  -17,    4), P(  -15,  -26), P(   -7,  -31), P(   -9,  -30), P(  -15,  -24), P(  -19,   -1), P(  -22,  -26), 
				P(  -25,  -31), P(  -27,    2), P(  -24,  -20), P(  -23,  -16), P(  -24,  -16), P(  -28,  -15), P(  -35,    3), P(  -31,  -26), 
				P(  -24,  -25), P(   -9,   12), P(  -20,   -6), P(  -25,   -2), P(  -27,    3), P(  -18,   -5), P(  -14,    9), P(  -27,  -22), 
				P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0), P(    0,    0) 
			},
			[KNIGHT / 2] = {
				P( -122,  -66), P( -120,   -5), P(  -54,    2), P(  -21,    0), P(   13,   -2), P(  -33,   -6), P(  -65,  -15), P( -109,  -84), 
				P(  -32,  -25), P(  -20,   -9), P(   25,   -9), P(    5,   -5), P(    4,  -11), P(   35,  -20), P(  -23,   -1), P(  -21,  -32), 
				P(   -5,  -14), P(   22,   -6), P(   39,   13), P(   46,   10), P(   55,    9), P(   46,    5), P(   26,   -4), P(   -6,  -20), 
				P(    5,   -3), P(   19,   13), P(   45,   22), P(   39,   27), P(   37,   27), P(   44,   20), P(   19,   13), P(    7,   -9), 
				P(    5,    7), P(   19,   18), P(   30,   35), P(   31,   38), P(   35,   39), P(   27,   32), P(   29,   14), P(    1,    4), 
				P(  -11,   -5), P(   13,    7), P(   17,   14), P(   26,   32), P(   30,   25), P(   15,   15), P(    9,    5), P(  -13,   -6), 
				P(   -7,   -9), P(  -10,   -5), P(    3,    6), P(   10,    8), P(   10,    5), P(   -1,    8), P(  -14,   -2), P(   -7,  -10), 
				P(  -45,  -16), P(  -19,  -17), P(  -10,  -10), P(   -4,    0), P(   -5,    0), P(   -3,  -10), P(  -19,  -21), P(  -44,  -18) 
			},
			[BISHOP / 2] = {
				P(  -40,    5), P(  -44,    5), P(  -54,    0), P(  -76,   14), P(  -75,    7), P(  -51,    5), P(  -33,    8), P(  -34,   -3), 
				P(  -11,   -5), P(    7,    2), P(   11,   -1), P(   10,   -7), P(    1,   -2), P(    3,   -4), P(   10,   -2), P(   -7,   -9), 
				P(   19,    4), P(   24,    5), P(   30,    6), P(   27,    1), P(   30,   -7), P(   36,    3), P(   35,   -3), P(   21,    0), 
				P(   -1,    3), P(    0,    9), P(   21,    7), P(   29,   10), P(   29,   16), P(   18,    7), P(    7,    3), P(  -11,    3), 
				P(   -2,   -9), P(   -1,    8), P(   -4,   12), P(   24,   11), P(   17,   10), P(    3,    8), P(   -8,    5), P(    2,   -9), 
				P(    6,   -8), P(    7,    0), P(    9,    6), P(   -3,   11), P(    5,    8), P(    2,    6), P(   10,    2), P(   -6,   -8), 
				P(   -3,  -16), P(   16,  -13), P(   10,  -18), P(    3,   -3), P(   -7,   -1), P(    7,   -8), P(    8,  -10), P(    1,   -7), 
				P(    8,  -20), P(   12,  -28), P(   -7,   -4), P(   -8,   -4), P(   -6,   -2), P(  -16,    0), P(    1,   18), P(  -12,  -16) 
			},
			[ROOK / 2] = {
				P(   24,    9), P(   10,    8), P(    7,   19), P(   12,    9), P(    9,   13), P(   21,   13), P(    6,   19), P(   15,   11), 
				P(   12,    9), P(   13,   16), P(   29,   17), P(   26,   12), P(   23,   13), P(   25,   13), P(    3,   17), P(   13,   10), 
				P(   -6,    9), P(   19,    6), P(   14,    7), P(    6,    4), P(   13,    4), P(    6,    5), P(   28,    5), P(    0,    7), 
				P(  -10,    8), P(    1,    3), P(    0,    2), P(   -2,    2), P(   -7,    2), P(   -5,    8), P(   -1,    4), P(   -7,    6), 
				P(  -15,    0), P(  -12,    1), P(  -17,    2), P(   -8,   -2), P(  -14,    0), P(  -22,    1), P(  -17,   -1), P(  -22,    2), 
				P(  -16,   -9), P(   -4,  -13), P(  -11,  -11), P(   -9,  -10), P(  -14,   -7), P(  -11,  -10), P(    1,  -21), P(  -12,   -9), 
				P(  -20,   -7), P(  -10,  -15), P(   -6,  -11), P(   -8,  -10), P(   -9,  -10), P(   -9,  -12), P(   -4,  -16), P(  -20,  -12), 
				P(   -6,  -12), P(    0,  -17), P(   -2,  -11), P(    4,  -17), P(    4,  -18), P(    0,  -15), P(   -2,  -14), P(   -3,  -11) 
			},
			[QUEEN / 2] = {
				P(  -14,   -9), P(   10,  -11), P(   -9,   25), P(    4,   12), P(    4,   11), P(  -11,   25), P(   -5,   -9), P(  -20,    7), 
				P(   22,   -7), P(   -5,   17), P(    2,   36), P(  -35,   71), P(  -29,   63), P(  -13,   41), P(  -12,   19), P(   12,    4), 
				P(    5,    0), P(    3,   17), P(   -3,   32), P(   -7,   33), P(  -11,   48), P(   -2,   34), P(   21,  -14), P(   10,   -1), 
				P(  -12,   11), P(  -11,   14), P(   -8,   19), P(   -8,   34), P(   -9,   35), P(  -10,   25), P(   -8,   22), P(   -8,   12), 
				P(    1,   -6), P(    6,    6), P(   -6,   18), P(   -5,   27), P(    1,   18), P(   -5,    4), P(    3,    4), P(   -1,    1), 
				P(    3,  -17), P(    8,   -8), P(    3,    3), P(   -1,    1), P(    2,    1), P(    3,    1), P(    7,   -5), P(    5,  -16), 
				P(    8,  -44), P(   12,  -44), P(   10,  -33), P(   12,  -16), P(   10,  -15), P(   14,  -34), P(   14,  -42), P(    8,  -32), 
				P(    3,  -44), P(    6,  -61), P(    7,  -59), P(   14,  -54), P(   11,  -44), P(    1,  -44), P(    7,  -50), P(   -6,  -31) 
			},
			[KING / 2] = {
				P(  118, -115), P(  123,  -48), P(   45,  -11), P(  -22,    1), P(  -24,    5), P(   39,  -27), P(  -19,  -21), P(   49,  -97), 
				P(    6,   -8), P(   27,   37), P(    4,   51), P(   57,   35), P(   35,   36), P(  -20,   44), P(  -11,   35), P( -100,    5), 
				P(  -34,    9), P(   29,   45), P(   62,   58), P(   12,   67), P(  -12,   72), P(    5,   60), P(   26,   41), P(  -82,   13), 
				P(  -88,    9), P(  -45,   42), P(  -40,   65), P(  -52,   77), P(  -74,   77), P(  -47,   61), P(  -51,   42), P( -110,   12), 
				P( -103,    2), P(  -45,   22), P(  -51,   47), P(  -63,   63), P(  -76,   68), P(  -49,   45), P(  -69,   29), P( -113,    3), 
				P(  -30,  -16), P(    7,    1), P(  -23,   27), P(  -32,   39), P(  -24,   37), P(  -24,   24), P(    0,    0), P(  -32,  -19), 
				P(   74,  -77), P(   70,  -54), P(   54,  -41), P(   47,  -29), P(   46,  -30), P(   47,  -38), P(   60,  -46), P(   64,  -72), 
				P(   62, -111), P(   65,  -81), P(   46,  -68), P(   67,  -76), P(   57,  -71), P(   45,  -65), P(   62,  -76), P(   59, -109) 
			}
	},
	.knightMobilityBonus = {
		P(  -23,  -20), P(   -9,   -7), P(   -2,    0), P(    2,    0), P(    7,    1), P(    7,    7), P(    7,    7), P(    7,    7), 
		P(    7,    7) 
	},
	.bishopMobilityBonus = {
		P(  -26,  -54), P(  -18,  -33), P(  -12,  -25), P(  -10,  -16), P(   -5,   -5), P(    0,    7), P(    5,    8), P(    7,   13), 
		P(    8,   18), P(    9,   18), P(    9,   18), P(   11,   18), P(   11,   18), P(   11,   18) 
	},
	.rookMobilityBonus = {
		P(  -17,  -39), P(   -8,  -23), P(   -4,  -20), P(   -2,  -15), P(   -2,  -13), P(   -1,   -4), P(    1,   -2), P(    3,    1), 
		P(    3,    9), P(    3,   13), P(    4,   14), P(    6,   16), P(    6,   20), P(    6,   22), P(    6,   22) 
	},
	.queenMobilityBonus = {
		P(  -27, -114), P(   -5, -114), P(   -5, -114), P(   -5,  -82), P(   -4,  -62), P(   -2,  -53), P(    2,  -44), P(    2,  -32), 
		P(    2,  -18), P(    2,  -12), P(    2,   -2), P(    2,    2), P(    2,    8), P(    2,   14), P(    2,   20), P(    2,   24), 
		P(    2,   30), P(    2,   36), P(    2,   43), P(    2,   45), P(    3,   49), P(    3,   50), P(    3,   50), P(    3,   51), 
		P(    3,   51), P(    3,   58), P(    3,   58), P(    3,   59) 
	},
	.virtualMobilityBonus = {
		P(   60,   51), P(   60,   23), P(   60,   18), P(   60,    6), P(   58,    6), P(   57,    6), P(   52,    5), P(   48,    2), 
		P(   45,    2), P(   38,    2), P(   36,    2), P(   32,    2), P(   27,    2), P(   19,    2), P(   10,    2), P(   -2,    2), 
		P(  -19,    2), P(  -32,    2), P(  -43,    2), P(  -48,   -2), P(  -53,   -6), P(  -55,   -9), P(  -66,  -12), P(  -66,  -17), 
		P(  -66,  -19), P(  -72,  -25), P(  -72,  -29), P(  -72,  -29) 
	},
	.minorPawnCover = P(    8,    3),
	.passedPawnBonus = {
		P(    0,    0), P(   -1,  -16), P(    1,  -13), P(    6,    2), P(   18,   31), P(   22,   89), P(   22,   89), P(    0,    0) 
	},
	.connectedPawnBonus = {
		P(    0,    0), P(   11,    9), P(    7,    3), P(    9,    5), P(    6,   11), P(   20,   20), P(  120,    5), P(    0,    0) 
	},
	.neighborPawnBonus = {
		P(    0,    0), P(    0,    0), P(    8,    4), P(   12,    9), P(   22,   25), P(   53,   94), P(  -45,  212), P(    0,    0) 
	},
	.doubledPawnBonus = {
		P(    1,  -30), P(   12,  -30), P(   10,  -22), P(   16,  -16), P(   11,   -9), P(   11,  -23), P(   14,  -30), P(    4,  -32) 
	},
	.isolatedPawnBonus = {
		P(   -3,   18), P(   -7,   -9), P(   -9,   -1), P(  -13,   -8), P(  -13,   -8), P(  -12,   -1), P(   -6,   -7), P(   -2,   15) 
	},
	.knightOutpostBonus = P(   33,   12),
	.bishopPairBonus = P(   19,   62),
	.badBishopBonus = P(   -2,   -5),
	.openRookFileBonus = { P(    8,   10), P(   30,    5) },
	.connectedRookBonus = { P(   -4,    8), P(   11,    1) },
	.connectedQueenBonus = { P(   -7,   41), P(    4,   33), P(    7,    7) },
	.kingPawnShieldBonus = {
		P(   31,  -26), P(   25,  -20), P(   29,  -16), P(   -2,   -9), P(   12,  -13), P(   10,   -9), P(   22,  -17), P(   32,  -26) 
	},
	.kingPawnStormBonus = {
		P(    0,    0), P(   16,    1), P(    7,    9), P(   13,   -1), P(    7,    2), P(   15,   -4), P(   12,   -2), P(    0,    0) 
	},
	.openKingFile = { P(   -9,    6), P(   -3,    3) },
	.kingSafety = {
		P(    0,    1), P(    0,    1), P(    5,    1), P(    5,    1), P(    5,    1), P(    5,    1), P(    5,    1), P(    9,    1), P(   10,    1), P(   13,    1), 
		P(   13,    1), P(   22,    1), P(   22,    1), P(   30,    1), P(   30,    1), P(   30,    1), P(   35,    1), P(   39,    1), P(   48,    1), P(   55,    1), 
		P(   55,    1), P(   65,    1), P(   65,    1), P(   74,    1), P(   75,    1), P(   75,    1), P(   77,    1), P(   90,    1), P(   98,    1), P(  115,    1), 
		P(  115,    1), P(  115,    1), P(  126,    1), P(  138,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), 
		P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), 
		P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), 
		P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), 
		P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), 
		P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), 
		P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1), P(  173,    1) 
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

		//Neighboring pawns
		int neighbors = __builtin_popcountll(borderingMask & board_rank[row]);
		if(neighbors && row > 1)
			EVAL_MADD(eval, hce_params.neighborPawnBonus[row], neighbors);

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
			
		//Neighboring pawns
		int neighbors = -1 * __builtin_popcountll(borderingMask & board_rank[row]);
		if(neighbors && mirroredRow > 1)
			EVAL_MADD(eval, hce_params.neighborPawnBonus[mirroredRow], neighbors);

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