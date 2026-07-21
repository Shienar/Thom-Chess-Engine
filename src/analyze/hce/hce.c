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
			[MIDDLEGAME] = {   100,  300,  300,  500,  900,    0},
			[ENDGAME] = {  100,  300,  300,  500, 900,    0}
		},
		.tempo = {   20,   20}
	};
 * 
 */
evalParameters hce_params = {
	.genericPieceValues = {
		[MIDDLEGAME] = {   75,  259,  293,  371,  826,    0},
		[ENDGAME] = {  140,  347,  365,  646, 1200,    0}
	},
	.rawPieceTables = {
		[MIDDLEGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				    6,   39,   54,   76,   60,   30,   -9,  -13,
				    9,    8,   40,   27,   35,   45,    0,    4,
				   -8,   -7,    5,    6,   10,   -2,  -12,  -12,
				  -20,  -18,   -6,    2,    0,   -9,  -20,  -22,
				  -22,  -23,  -14,  -15,  -14,  -16,  -29,  -23,
				  -21,  -13,  -10,  -20,  -18,  -12,  -21,  -26,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				 -110, -112,  -47,  -15,   20,  -26,  -59,  -99,
				  -29,  -21,   22,    5,    3,   34,  -25,  -18,
				   -2,   19,   32,   44,   53,   41,   23,   -2,
				    7,   18,   42,   37,   36,   41,   18,   10,
				    6,   18,   28,   28,   33,   24,   29,    1,
				  -13,   10,   13,   23,   26,   13,    6,  -14,
				  -13,  -15,   -2,    9,    9,   -5,  -17,  -11,
				  -44,  -13,  -10,   -1,   -2,   -3,  -12,  -43
			},
			[BISHOP / 2] = {
				  -38,  -41,  -48,  -73,  -66,  -44,  -30,  -35,
				   -9,    7,   11,   10,    2,   -1,   10,   -3,
				   21,   24,   31,   25,   27,   37,   37,   25,
				    4,   -1,   21,   28,   29,   17,    5,   -9,
				   -4,    0,   -6,   24,   18,    0,  -10,    1,
				    4,    4,    9,   -4,    3,    1,    6,   -8,
				   -9,   11,   12,    5,   -8,    6,    4,   -5,
				    5,   16,   -7,  -11,   -7,  -16,    5,  -16
			},
			[ROOK / 2] = {
				   28,   13,   11,   18,   16,   25,    8,   16,
				   13,   15,   28,   28,   25,   25,    3,   14,
				    2,   23,   18,   13,   22,   11,   30,    9,
				   -6,    1,    0,   -4,   -7,   -8,   -2,   -4,
				  -13,  -13,  -20,   -9,  -16,  -25,  -16,  -22,
				  -16,   -3,  -13,  -10,  -16,  -13,    2,  -12,
				  -24,  -15,  -10,   -8,   -9,  -13,  -10,  -24,
				  -15,   -4,   -7,    0,    0,   -7,   -7,  -12
			},
			[QUEEN / 2] = {
				  -11,   16,    0,   23,   23,   -2,   -4,  -19,
				   19,  -16,    1,  -31,  -24,  -14,  -23,    8,
				   36,   28,    5,    2,    3,   12,   50,   41,
				   10,   -6,   -1,  -12,  -12,   -4,   -1,   12,
				   -7,   -1,  -14,  -12,   -6,  -10,   -5,   -9,
				   -4,   -2,   -5,  -11,   -8,   -6,   -3,   -2,
				    4,    2,    4,    4,    0,    7,    4,    3,
				   -3,    0,   -2,    0,   -3,   -8,   -1,  -13
			},
			[KING / 2] = {
				  119,  137,   65,  -16,  -20,   52,    4,   63,
				   20,   30,    4,   53,   41,  -20,   -3,  -76,
				  -20,   32,   61,   15,   -9,   11,   28,  -69,
				  -72,  -41,  -38,  -55,  -79,  -47,  -47,  -90,
				  -87,  -44,  -54,  -70,  -82,  -51,  -64,  -94,
				  -20,    9,  -29,  -38,  -31,  -29,   -2,  -24,
				   64,   57,   18,   -1,   -3,   14,   48,   57,
				   67,   76,   41,   39,   24,   44,   70,   62
			}
		},
		[ENDGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				   66,   82,   61,   22,   25,   56,   92,   80,
				    6,   39,  -11,  -40,  -42,  -26,   33,   10,
				  -12,   10,  -26,  -38,  -39,  -27,    6,  -13,
				  -24,    8,  -24,  -28,  -30,  -22,    5,  -20,
				  -28,    5,  -19,  -16,  -18,  -18,    5,  -25,
				  -25,    8,  -12,   -5,   -4,  -12,    6,  -22,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				  -66,   -3,    1,    0,   -2,   -8,  -14,  -81,
				  -20,   -6,   -9,   -6,  -12,  -22,    3,  -27,
				  -10,   -6,   12,    7,    7,    3,   -4,  -18,
				    0,   12,   20,   25,   24,   19,   13,   -5,
				   12,   17,   31,   35,   36,   29,   12,    8,
				   -2,    5,   10,   28,   21,   11,    3,   -2,
				   -2,   -3,    4,    6,    3,    5,    0,   -5,
				  -13,  -18,   -9,    0,   -1,  -10,  -22,  -12
			},
			[BISHOP / 2] = {
				    8,    8,    0,   16,    7,    5,    8,    1,
				   -4,    0,   -1,   -8,   -2,   -4,   -3,  -11,
				    4,    5,    3,    0,   -9,    1,   -6,    1,
				    2,    8,    6,    7,   13,    4,    4,    1,
				   -6,    8,   11,   10,    6,    8,    3,   -6,
				   -4,    1,    5,   10,    8,    4,    4,   -7,
				  -10,   -8,  -20,   -6,   -4,   -9,   -9,   -1,
				  -14,  -26,   -6,   -4,   -2,   -4,   18,  -14
			},
			[ROOK / 2] = {
				    8,    9,   18,    8,   12,   12,   19,   10,
				   10,   17,   19,   12,   13,   14,   18,   10,
				    6,    5,    5,    2,    2,    4,    3,    4,
				    6,    3,    2,    2,    3,   10,    4,    5,
				   -1,    1,    3,   -3,    1,    2,   -2,    1,
				  -10,  -13,  -11,  -11,   -7,  -10,  -21,  -10,
				   -7,  -14,  -10,  -11,  -10,  -10,  -14,  -13,
				   -9,  -15,   -8,  -16,  -17,  -13,  -11,   -8
			},
			[QUEEN / 2] = {
				   -8,  -11,   25,   13,   12,   27,   -4,    8,
				  -10,   19,   40,   72,   65,   43,   23,    3,
				  -21,    2,   37,   39,   47,   37,  -29,  -17,
				   -4,   16,   21,   41,   41,   30,   24,    5,
				   -1,   10,   22,   30,   20,    7,    8,    8,
				  -19,  -10,    4,   -1,   -1,    3,   -5,  -19,
				  -54,  -48,  -41,  -23,  -21,  -41,  -45,  -39,
				  -44,  -54,  -52,  -42,  -28,  -37,  -42,  -29
			},
			[KING / 2] = {
				 -126,  -60,  -23,   -8,   -4,  -37,  -33, -108,
				  -18,   28,   42,   26,   24,   35,   26,   -5,
				    0,   36,   48,   56,   62,   50,   34,    3,
				   -2,   33,   55,   69,   69,   53,   34,    2,
				   -9,   14,   42,   59,   63,   41,   22,   -7,
				  -24,   -4,   24,   36,   34,   22,   -3,  -27,
				  -54,  -27,   -3,    9,    7,   -4,  -25,  -58,
				  -94,  -63,  -42,  -45,  -38,  -40,  -67, -101
			}
		}
	},
	.knightMobilityBonus = {
		[MIDDLEGAME] = {  -32,  -13,   -4,    2,    8,    9,   10,   11,   12},
		[ENDGAME] = {  -33,  -11,   -3,   -2,    1,   11,   12,   13,   14}
	},
	.bishopMobilityBonus = {
		[MIDDLEGAME] = {  -29,  -20,  -14,  -11,   -6,    0,    5,    8,    9,   10,   11,   12,   13,   14},
		[ENDGAME] = {  -61,  -42,  -31,  -20,   -7,    6,    9,   14,   20,   21,   22,   23,   24,   25}
	},
	.rookMobilityBonus = {
		[MIDDLEGAME] = {  -22,   -8,   -6,   -3,   -2,   -1,    0,    2,    3,    4,    5,    6,    7,    8,    9},
		[ENDGAME] = {  -40,  -25,  -20,  -15,  -13,   -5,   -2,    1,    8,   12,   14,   17,   20,   23,   24}
	},
	.queenMobilityBonus = {
		[MIDDLEGAME] = {  -50,  -17,  -16,  -15,  -13,   -9,   -4,   -3,   -2,   -1,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,   16,   17},
		[ENDGAME] = { -100,  -99,  -98,  -66,  -50,  -45,  -40,  -31,  -20,  -14,   -5,   -1,    5,   10,   15,   19,   25,   30,   36,   38,   43,   44,   45,   47,   48,   53,   54,   57}
	},
	.virtualMobilityBonus = {
		[MIDDLEGAME] = {   56,   55,   54,   53,   51,   50,   45,   39,   37,   30,   29,   25,   19,   13,    6,   -3,  -15,  -25,  -33,  -44,  -49,  -50,  -55,  -56,  -57,  -58,  -59,  -60},
		[ENDGAME] = {   80,   39,   29,   14,   12,   11,    8,    5,    4,    3,    2,    1,    0,   -1,   -2,   -3,   -4,   -5,   -6,   -7,  -10,  -13,  -18,  -23,  -26,  -35,  -36,  -37}
	},
	.minorPawnCover = {    8,    5},
	.passedPawnBonus = {
		[MIDDLEGAME] = {    0,    1,    3,    6,   19,   26,   27,    0},
		[ENDGAME] = {    0,  -18,  -16,    0,   30,   93,   94,    0}
	},
	.doubledPawnBonus = {
		[MIDDLEGAME] = {   -7,    5,   -2,    1,   -3,   -1,    7,   -5},
		[ENDGAME] = {  -26,  -26,  -16,   -9,   -3,  -17,  -26,  -29}
	},
	.isolatedPawnBonus = {
		[MIDDLEGAME] = {  -12,   -5,  -15,  -17,  -18,  -18,   -7,  -12},
		[ENDGAME] = {   17,  -13,   -4,  -13,  -12,   -3,  -10,   14}
	},
	.connectedPawnBonus = {
		[MIDDLEGAME] = {    0,    0,   11,    4,    3,   17,  124,    0},
		[ENDGAME] = {    0,    0,    7,    2,    8,   22,   19,    0}
	},
	.backwardPawnBonus = {
		[MIDDLEGAME] = {    6,    6,    7,    7,    3,    6,    7,   -9},
		[ENDGAME] = {    2,    2,   -4,   -1,    5,    5,   -3,    0}
	},
	.knightOutputBonus = {   35,    9},
	.bishopPairBonus = {   19,   63},
	.badBishopBonus = {    0,   -1},
	.openRookFileBonus = {
		[MIDDLEGAME] = {   10,   32},
		[ENDGAME] = {   11,    5}
	},
	.tempo = {   20,   22}
};

evalParameters is_param_eg = {
    .genericPieceValues[ENDGAME] = { [0 ... PIECE_TYPE_COUNT - 1] = 1 },
    .rawPieceTables[ENDGAME] = { [0 ... 5] = { [0 ... 63] = 1 } },

    .knightMobilityBonus[ENDGAME] = { [0 ... 8] = 1 },
    .bishopMobilityBonus[ENDGAME] = { [0 ... 13] = 1 },
    .rookMobilityBonus[ENDGAME] = { [0 ... 14] = 1 },
    .queenMobilityBonus[ENDGAME] = { [0 ... 27] = 1 },
    .virtualMobilityBonus[ENDGAME] = { [0 ... 27] = 1 },

    .minorPawnCover[ENDGAME] = 1,
    .passedPawnBonus[ENDGAME] = { [0 ... 7] = 1 },
    .doubledPawnBonus[ENDGAME] = { [0 ... 7] = 1 },
    .isolatedPawnBonus[ENDGAME] = { [0 ... 7] = 1 },
    .connectedPawnBonus[ENDGAME] = { [0 ... 7] = 1 },
    .backwardPawnBonus[ENDGAME] = { [0 ... 7] = 1 },

	.knightOutputBonus[ENDGAME] = 1,

	.bishopPairBonus[ENDGAME] = 1,
	.badBishopBonus[ENDGAME] = 1,

    .openRookFileBonus[ENDGAME] = { [0 ... 1] = 1 },

    .tempo[ENDGAME] = 1
};

int initHCE = 0;

int pieceBonusTable[PHASE_COUNT][PIECE_COUNT][64];

//Other tables
uint64_t bordering_files[COLUMN_COUNT];

void init_HCE_tables()
{
    if(initHCE) return;
    initHCE = 1;

	//PSQT
    for(int gamephase = MIDDLEGAME; gamephase <= ENDGAME; gamephase++)
    {
        for(int piece = 0; piece < PIECE_COUNT; piece+=2)
        {
            for(int square = 0; square < 64; square++)
            {
                pieceBonusTable[gamephase][piece][square] = hce_params.rawPieceTables[gamephase][piece / 2][FLIP_SQUARE(square)] + hce_params.genericPieceValues[gamephase][piece / 2];
                pieceBonusTable[gamephase][piece + 1][square] = hce_params.rawPieceTables[gamephase][piece / 2][MIRROR_SQUARE(square)] + hce_params.genericPieceValues[gamephase][piece / 2];
            }
        }
    }

    bordering_files[0] = board_file[1];
    bordering_files[1] = board_file[0] | board_file[2];
    bordering_files[2] = board_file[1] | board_file[3];
    bordering_files[3] = board_file[2] | board_file[4];
    bordering_files[4] = board_file[3] | board_file[5];
    bordering_files[5] = board_file[4] | board_file[6];
    bordering_files[6] = board_file[5] | board_file[7];
    bordering_files[7] = board_file[6];
}

void evaluatePawns(bitboard* board, int* mgScore, int* egScore)
{
	int eval[PHASE_COUNT] = {0};

	uint64_t mask = board->pieces[WHITE_PAWN];

	int protectedCount = __builtin_popcountll((mask >> 8) & (board->pieces[WHITE_BISHOP] | board->pieces[WHITE_KNIGHT]));

	while(mask)
	{
        int sq = __builtin_ctzll(mask);
        int column = getColumn(sq);
		int row = getRow(sq);

		//piece/square value
        eval[MIDDLEGAME] += pieceBonusTable[MIDDLEGAME][WHITE_PAWN][sq];
        eval[ENDGAME] += pieceBonusTable[ENDGAME][WHITE_PAWN][sq];

		 //Passed Pawns
        if((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
        {
            eval[MIDDLEGAME] += hce_params.passedPawnBonus[MIDDLEGAME][row];
            eval[ENDGAME] += hce_params.passedPawnBonus[ENDGAME][row];
        }

        //Doubled pawns
        if(__builtin_popcountll(board->pieces[WHITE_PAWN] & board_file[column]) > 1)
        {
            eval[MIDDLEGAME] += hce_params.doubledPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.doubledPawnBonus[ENDGAME][column];
        }

		uint64_t borderingMask = board->pieces[WHITE_PAWN] & bordering_files[column];
        //Isolated Pawns
        if(!borderingMask)
        {
            eval[MIDDLEGAME] += hce_params.isolatedPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.isolatedPawnBonus[ENDGAME][column];
        }
		//Backward pawns
		else if(row > 1 && (borderingMask & (sq - 1)) == 0 && borderingMask & board_rank[row + 1])
		{
			eval[MIDDLEGAME] += hce_params.backwardPawnBonus[MIDDLEGAME][column];
			eval[ENDGAME] += hce_params.backwardPawnBonus[ENDGAME][column];
		}
		//connected pawns
		else if(borderingMask & board_rank[row - 1])
		{
			eval[MIDDLEGAME] += hce_params.connectedPawnBonus[MIDDLEGAME][row];
			eval[ENDGAME] += hce_params.connectedPawnBonus[ENDGAME][row];
		}

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
        eval[MIDDLEGAME] -= pieceBonusTable[MIDDLEGAME][BLACK_PAWN][sq];
        eval[ENDGAME] -= pieceBonusTable[ENDGAME][BLACK_PAWN][sq];

		//Passed Pawns
        if((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
        {
            eval[MIDDLEGAME] -= hce_params.passedPawnBonus[MIDDLEGAME][mirroredRow];
            eval[ENDGAME] -= hce_params.passedPawnBonus[ENDGAME][mirroredRow];
        }
        
        //Doubled pawns
        if(__builtin_popcountll(board->pieces[BLACK_PAWN] & board_file[column]) > 1)
        {
            eval[MIDDLEGAME] -= hce_params.doubledPawnBonus[MIDDLEGAME][mirroredColumn];
            eval[ENDGAME] -= hce_params.doubledPawnBonus[ENDGAME][mirroredColumn];
        }

		uint64_t borderingMask = board->pieces[BLACK_PAWN] & bordering_files[column];
        //Isolated Pawns
        if(!borderingMask)
        {
            eval[MIDDLEGAME] -= hce_params.isolatedPawnBonus[MIDDLEGAME][mirroredColumn];
            eval[ENDGAME] -= hce_params.isolatedPawnBonus[ENDGAME][mirroredColumn];
        }
		//Backward pawns
		else if(row < 6 && (borderingMask & ~(sq - 1)) == 0 && borderingMask & board_rank[row - 1])
		{
			eval[MIDDLEGAME] -= hce_params.backwardPawnBonus[MIDDLEGAME][mirroredColumn];
			eval[ENDGAME] -= hce_params.backwardPawnBonus[ENDGAME][mirroredColumn];
		}
		//connected pawns
		else if(borderingMask & board_rank[row + 1])
		{
			eval[MIDDLEGAME] -= hce_params.connectedPawnBonus[MIDDLEGAME][mirroredRow];
			eval[ENDGAME] -= hce_params.connectedPawnBonus[ENDGAME][mirroredRow];
		}

		mask &= mask - 1;
	}
	
	eval[MIDDLEGAME] += protectedCount * hce_params.minorPawnCover[MIDDLEGAME];
	eval[ENDGAME] += protectedCount * hce_params.minorPawnCover[ENDGAME];

	*mgScore += eval[MIDDLEGAME];
	*egScore += eval[ENDGAME];
}

void evaluateKnights(bitboard* board, int* mgScore, int* egScore)
{
	int eval[PHASE_COUNT] = {0};

	uint64_t mask = board->pieces[WHITE_KNIGHT];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		int row = getRow(sq);

		//piece/square value
        eval[MIDDLEGAME] += pieceBonusTable[MIDDLEGAME][WHITE_KNIGHT][sq];
        eval[ENDGAME] += pieceBonusTable[ENDGAME][WHITE_KNIGHT][sq];

		//mobility
		uint64_t moves = knightMoves(board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);

		eval[MIDDLEGAME] += hce_params.knightMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] += hce_params.knightMobilityBonus[ENDGAME][moveCount];

		//outpost
		if(row >= 4 && row <= 6 && 
		   board->pieces[WHITE_PAWN] && board_rank[row - 1] &&
		   (bordering_files[column] & board->pieces[BLACK_PAWN] & (~(singleBitMask(sq + 7) - 1))) == 0)
			{	
				eval[MIDDLEGAME] += hce_params.knightOutputBonus[MIDDLEGAME];
				eval[ENDGAME] += hce_params.knightOutputBonus[ENDGAME];
			}

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_KNIGHT];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		int row = getRow(sq);
		
		//piece/square value
        eval[MIDDLEGAME] -= pieceBonusTable[MIDDLEGAME][BLACK_KNIGHT][sq];
        eval[ENDGAME] -= pieceBonusTable[ENDGAME][BLACK_KNIGHT][sq];

		//mobility
		uint64_t moves = knightMoves(board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);

		eval[MIDDLEGAME] -= hce_params.knightMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] -= hce_params.knightMobilityBonus[ENDGAME][moveCount];
		
		//outpost
		if(row >= 1 && row <= 3 &&
		   board->pieces[BLACK_PAWN] && board_rank[row + 1] &&
		   (bordering_files[column] & board->pieces[WHITE_PAWN] & ((singleBitMask(sq - 6) - 1))) == 0)
			{	
				eval[MIDDLEGAME] -= hce_params.knightOutputBonus[MIDDLEGAME];
				eval[ENDGAME] -= hce_params.knightOutputBonus[ENDGAME];
			}

		mask &= mask - 1;
	}
	
	*mgScore += eval[MIDDLEGAME];
	*egScore += eval[ENDGAME];
}

void evaluateBishops(bitboard* board, int* mgScore, int* egScore)
{
	int eval[PHASE_COUNT] = {0};

	uint64_t mask = board->pieces[WHITE_BISHOP];

	//bad bishop-pawns
	int badPawns = 0;
	if(mask & LIGHT_SQUARES)
	 	badPawns += __builtin_popcountll(board->pieces[WHITE_PAWN] & LIGHT_SQUARES);
	if(mask & DARK_SQUARES)
	 	badPawns += __builtin_popcountll(board->pieces[WHITE_PAWN] & DARK_SQUARES);

	//bishop pair
	if(__builtin_popcountll(mask) >= 2)
	{
		eval[MIDDLEGAME] += hce_params.bishopPairBonus[MIDDLEGAME];
		eval[ENDGAME] += hce_params.bishopPairBonus[ENDGAME];
	}

	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
        eval[MIDDLEGAME] += pieceBonusTable[MIDDLEGAME][WHITE_BISHOP][sq];
        eval[ENDGAME] += pieceBonusTable[ENDGAME][WHITE_BISHOP][sq];

		//mobility
		uint64_t moves = bishopMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);

		eval[MIDDLEGAME] += hce_params.bishopMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] += hce_params.bishopMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_BISHOP];

	//bad bishop-pawns
	if(mask & LIGHT_SQUARES)
	 	badPawns += __builtin_popcountll(board->pieces[BLACK_PAWN] & LIGHT_SQUARES);
	if(mask & DARK_SQUARES)
	 	badPawns += __builtin_popcountll(board->pieces[BLACK_PAWN] & DARK_SQUARES);

	//bishop pair
	if(__builtin_popcountll(mask) >= 2)
	{
		eval[MIDDLEGAME] -= hce_params.bishopPairBonus[MIDDLEGAME];
		eval[ENDGAME] -= hce_params.bishopPairBonus[ENDGAME];
	}

	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/sq value
        eval[MIDDLEGAME] -= pieceBonusTable[MIDDLEGAME][BLACK_BISHOP][sq];
        eval[ENDGAME] -= pieceBonusTable[ENDGAME][BLACK_BISHOP][sq];
		
		//mobility
		uint64_t moves = bishopMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);

		eval[MIDDLEGAME] -= hce_params.bishopMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] -= hce_params.bishopMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	eval[MIDDLEGAME] += badPawns * hce_params.badBishopBonus[MIDDLEGAME];
	eval[ENDGAME] += badPawns * hce_params.badBishopBonus[ENDGAME];

	*mgScore += eval[MIDDLEGAME];
	*egScore += eval[ENDGAME];
}

void evaluateRooks(bitboard* board, int* mgScore, int* egScore)
{
	int eval[PHASE_COUNT] = {0};

	uint64_t mask = board->pieces[WHITE_ROOK];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);

		//piece/sq value
        eval[MIDDLEGAME] += pieceBonusTable[MIDDLEGAME][WHITE_ROOK][sq];
        eval[ENDGAME] += pieceBonusTable[ENDGAME][WHITE_ROOK][sq];
		
		//open rook file
        if((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
        {
        	if((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
			{
				eval[MIDDLEGAME] += hce_params.openRookFileBonus[MIDDLEGAME][OPEN_FILE];
				eval[ENDGAME] += hce_params.openRookFileBonus[ENDGAME][OPEN_FILE];
			}
			else
			{
				eval[MIDDLEGAME] += hce_params.openRookFileBonus[MIDDLEGAME][HALF_OPEN_FILE];
				eval[ENDGAME] += hce_params.openRookFileBonus[ENDGAME][HALF_OPEN_FILE];
			}
        }

		//mobility
		uint64_t moves = rookMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);
		
		eval[MIDDLEGAME] += hce_params.rookMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] += hce_params.rookMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_ROOK];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		
		//piece/sq value
        eval[MIDDLEGAME] -= pieceBonusTable[MIDDLEGAME][BLACK_ROOK][sq];
        eval[ENDGAME] -= pieceBonusTable[ENDGAME][BLACK_ROOK][sq];
		
		//open rook file
        if((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
        {
            if((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
			{
				eval[MIDDLEGAME] -= hce_params.openRookFileBonus[MIDDLEGAME][OPEN_FILE];
				eval[ENDGAME] -= hce_params.openRookFileBonus[ENDGAME][OPEN_FILE];
			}
			else
			{
				eval[MIDDLEGAME] -= hce_params.openRookFileBonus[MIDDLEGAME][HALF_OPEN_FILE];
				eval[ENDGAME] -= hce_params.openRookFileBonus[ENDGAME][HALF_OPEN_FILE];
			}
        }
		
		//mobility
		uint64_t moves = rookMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		
		eval[MIDDLEGAME] -= hce_params.rookMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] -= hce_params.rookMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	*mgScore += eval[MIDDLEGAME];
	*egScore += eval[ENDGAME];
}

void evaluateQueens(bitboard* board, int* mgScore, int* egScore)
{
	int eval[PHASE_COUNT] = {0};

	uint64_t mask = board->pieces[WHITE_QUEEN];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
        eval[MIDDLEGAME] += pieceBonusTable[MIDDLEGAME][WHITE_QUEEN][sq];
        eval[ENDGAME] += pieceBonusTable[ENDGAME][WHITE_QUEEN][sq];

		//mobility
		uint64_t moves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);
		
		eval[MIDDLEGAME] += hce_params.queenMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] += hce_params.queenMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_QUEEN];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/square value
        eval[MIDDLEGAME] -= pieceBonusTable[MIDDLEGAME][BLACK_QUEEN][sq];
        eval[ENDGAME] -= pieceBonusTable[ENDGAME][BLACK_QUEEN][sq];

		//mobility
		uint64_t moves = queenMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		
		eval[MIDDLEGAME] -= hce_params.queenMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] -= hce_params.queenMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	*mgScore += eval[MIDDLEGAME];
	*egScore += eval[ENDGAME];
}

void evaluateKings(bitboard* board, int* mgScore, int* egScore)
{
	int eval[PHASE_COUNT] = {0};

	uint64_t mask = board->pieces[WHITE_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
        eval[MIDDLEGAME] += pieceBonusTable[MIDDLEGAME][WHITE_KING][sq];
        eval[ENDGAME] += pieceBonusTable[ENDGAME][WHITE_KING][sq];

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
		eval[MIDDLEGAME] += hce_params.virtualMobilityBonus[MIDDLEGAME][virtualMoveCount];
		eval[ENDGAME] += hce_params.virtualMobilityBonus[ENDGAME][virtualMoveCount];

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/square value
        eval[MIDDLEGAME] -= pieceBonusTable[MIDDLEGAME][WHITE_KING][sq];
        eval[ENDGAME] -= pieceBonusTable[ENDGAME][WHITE_KING][sq];

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
		eval[MIDDLEGAME] -= hce_params.virtualMobilityBonus[MIDDLEGAME][virtualMoveCount];
		eval[ENDGAME] -= hce_params.virtualMobilityBonus[ENDGAME][virtualMoveCount];

		mask &= mask - 1;
	}

	*mgScore += eval[MIDDLEGAME];
	*egScore += eval[ENDGAME];
}

int gamephasePieceValues[PIECE_COUNT] = {0,0,1,1,1,1,2,2,4,4,0,0};
int evaluatePhasedScore(bitboard* board, int middlegameScore, int endgameScore)
{
    int phase = 24;
    for(int pc = 2; pc < PIECE_COUNT - 2; pc++)
    {
        phase -= gamephasePieceValues[pc] * __builtin_popcountll(board->pieces[pc]);
    }
	phase = clamp(phase, 0, 24);

	int eg_phase = (phase * 256 + 12) / 24;
	int mg_phase = 256 - eg_phase;
	return (mg_phase * middlegameScore + eg_phase * endgameScore) / 256;
}

int hce_eval(bitboard* board)
{
    int midgame_eval = hce_params.tempo[MIDDLEGAME];
    int endgame_eval = hce_params.tempo[ENDGAME];

    evaluatePawns(board, &midgame_eval, &endgame_eval);
    evaluateKnights(board, &midgame_eval, &endgame_eval);
    evaluateBishops(board, &midgame_eval, &endgame_eval);
    evaluateRooks(board, &midgame_eval, &endgame_eval);
    evaluateQueens(board, &midgame_eval, &endgame_eval);
    evaluateKings(board, &midgame_eval, &endgame_eval);

    int eval = evaluatePhasedScore(board, midgame_eval, endgame_eval);

	return (ISWHITE(board->turn)) ? eval : -eval;
}