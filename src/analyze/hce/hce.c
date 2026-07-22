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
		[MIDDLEGAME] = {   74,  257,  290,  363,  784,    0},
		[ENDGAME] = {  141,  332,  369,  652, 1235,    0}
	},
	.rawPieceTables = {
		[MIDDLEGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				    3,   37,   51,   73,   56,   29,   -8,  -15,
				    8,    9,   40,   27,   35,   45,    1,    4,
				   -8,   -6,    7,    6,   11,   -2,  -13,  -12,
				  -20,  -15,   -4,    2,    1,   -8,  -19,  -24,
				  -21,  -23,  -12,  -13,  -13,  -14,  -28,  -24,
				  -22,  -12,  -11,  -22,  -21,  -12,  -18,  -28,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				 -106, -113,  -46,  -17,   19,  -27,  -59,  -95,
				  -30,  -21,   22,    5,    3,   34,  -25,  -18,
				   -2,   18,   30,   44,   53,   40,   23,   -1,
				    6,   18,   41,   37,   36,   40,   18,   10,
				    5,   18,   28,   28,   33,   25,   29,    1,
				  -13,   10,   14,   23,   27,   13,    7,  -14,
				  -12,  -15,   -1,   10,   10,   -4,  -16,  -11,
				  -41,  -15,   -9,   -2,   -3,   -3,  -14,  -42
			},
			[BISHOP / 2] = {
				  -38,  -41,  -49,  -73,  -66,  -45,  -31,  -35,
				   -8,    6,    9,   11,    2,   -1,    9,   -2,
				   22,   23,   29,   25,   27,   36,   38,   25,
				    3,   -1,   21,   28,   29,   17,    5,   -8,
				   -3,    0,   -6,   24,   18,    1,   -9,    2,
				    4,    4,   10,   -3,    4,    2,    7,   -7,
				   -9,   11,   12,    5,   -7,    6,    5,   -5,
				    5,   16,   -8,  -11,   -7,  -17,    3,  -15
			},
			[ROOK / 2] = {
				   28,   12,    9,   15,   14,   23,    6,   17,
				   13,   15,   26,   26,   23,   23,    3,   13,
				    2,   22,   17,   11,   21,   10,   29,    8,
				   -6,    1,   -2,   -5,   -8,  -10,   -2,   -3,
				  -13,  -12,  -21,  -10,  -17,  -25,  -16,  -21,
				  -15,   -3,  -14,  -12,  -16,  -13,    2,  -11,
				  -22,  -15,  -11,  -10,  -11,  -14,  -10,  -22,
				  -12,   -1,   -2,    4,    4,   -3,   -3,   -9
			},
			[QUEEN / 2] = {
				   -8,   18,    1,   23,   24,   -3,   -2,  -15,
				   20,  -16,    1,  -30,  -23,  -14,  -22,    9,
				   36,   27,    5,    1,    3,   12,   52,   42,
				   12,   -6,   -2,  -13,  -13,   -5,   -1,   13,
				   -7,    0,  -14,  -14,   -8,  -11,   -4,   -9,
				   -5,   -3,   -7,  -12,   -9,   -7,   -5,   -1,
				    1,    1,    2,    2,   -1,    6,    3,    1,
				   -6,    0,   -2,    3,    0,   -7,    0,  -13
			},
			[KING / 2] = {
				  188,  155,   70,  -21,  -23,   55,    9,   81,
				   25,   28,    0,   44,   36,  -24,   -4,  -76,
				  -16,   30,   50,    2,  -24,    1,   25,  -67,
				  -71,  -44,  -48,  -69,  -95,  -57,  -51,  -89,
				  -85,  -46,  -61,  -82,  -93,  -57,  -65,  -91,
				  -18,    8,  -33,  -43,  -35,  -30,    1,  -18,
				   67,   57,   19,    0,   -2,   17,   53,   64,
				   70,   79,   45,   41,   28,   47,   75,   67
			}
		},
		[ENDGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				   67,   83,   61,   22,   26,   56,   93,   79,
				    7,   40,  -11,  -40,  -42,  -26,   34,    9,
				  -11,    9,  -26,  -38,  -39,  -27,    6,  -14,
				  -23,    8,  -24,  -28,  -28,  -21,    6,  -21,
				  -28,    5,  -20,  -16,  -17,  -17,    4,  -26,
				  -27,    8,  -13,   -6,   -4,  -14,    6,  -24,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				  -70,   -5,    0,    0,   -3,   -8,  -16,  -85,
				  -23,   -7,   -9,   -5,  -11,  -21,    2,  -29,
				  -12,   -5,   13,    7,    8,    4,   -3,  -19,
				   -1,   13,   21,   26,   25,   20,   14,   -7,
				   10,   18,   32,   36,   37,   30,   13,    6,
				   -4,    6,   11,   29,   23,   12,    4,   -4,
				   -4,   -4,    4,    6,    3,    5,   -1,   -6,
				  -13,  -14,   -9,    1,    0,  -10,  -19,  -12
			},
			[BISHOP / 2] = {
				    7,    8,    0,   16,    7,    5,    8,    1,
				   -4,    0,   -1,   -9,   -3,   -5,   -3,  -11,
				    4,    5,    3,   -1,  -10,    0,   -7,    1,
				    2,    7,    5,    6,   13,    3,    3,    1,
				   -6,    7,   10,    9,    6,    8,    2,   -6,
				   -4,    1,    5,   10,    8,    4,    4,   -7,
				  -10,   -9,  -19,   -6,   -4,   -9,   -9,   -1,
				  -14,  -25,   -1,   -3,   -1,    1,   18,  -14
			},
			[ROOK / 2] = {
				    8,    9,   19,    9,   13,   13,   19,   10,
				    9,   16,   19,   11,   13,   14,   18,   10,
				    6,    6,    5,    2,    2,    5,    4,    4,
				    7,    3,    3,    3,    3,   10,    5,    5,
				   -1,    1,    4,   -2,    1,    3,   -1,    2,
				   -9,  -12,   -9,   -9,   -6,   -8,  -21,   -9,
				   -6,  -12,   -8,   -9,   -9,   -9,  -13,  -12,
				  -12,  -18,  -13,  -21,  -21,  -18,  -15,  -10
			},
			[QUEEN / 2] = {
				  -10,  -12,   25,   13,   11,   27,   -6,    7,
				   -8,   21,   40,   71,   63,   42,   23,    4,
				  -18,    4,   37,   38,   47,   37,  -29,  -14,
				   -1,   17,   21,   40,   40,   30,   26,    9,
				    3,   11,   22,   30,   20,    8,   11,   13,
				  -15,   -6,    5,    1,    1,    5,    0,  -15,
				  -48,  -45,  -35,  -18,  -17,  -38,  -43,  -35,
				  -45,  -60,  -61,  -59,  -51,  -48,  -54,  -34
			},
			[KING / 2] = {
				 -139,  -63,  -23,   -7,   -2,  -37,  -34, -112,
				  -19,   29,   43,   27,   25,   36,   26,   -6,
				   -1,   36,   50,   58,   64,   52,   34,    3,
				   -2,   33,   57,   71,   71,   55,   35,    2,
				   -9,   15,   43,   60,   65,   42,   22,   -8,
				  -23,   -4,   25,   36,   35,   21,   -4,  -27,
				  -55,  -27,   -3,    8,    7,   -5,  -26,  -59,
				  -94,  -63,  -42,  -43,  -38,  -39,  -68, -102
			}
		}
	},
	.knightMobilityBonus = {
		[MIDDLEGAME] = {  -32,  -13,   -5,    2,    8,    9,   10,   11,   12},
		[ENDGAME] = { -185,    3,   19,   20,   21,   29,   30,   31,   32}
	},
	.bishopMobilityBonus = {
		[MIDDLEGAME] = {  -29,  -19,  -13,  -11,   -6,   -1,    5,    8,    9,   10,   11,   12,   13,   14},
		[ENDGAME] = {  -65,  -43,  -31,  -20,   -8,    6,    9,   14,   21,   22,   23,   24,   25,   26}
	},
	.rookMobilityBonus = {
		[MIDDLEGAME] = {  -21,  -10,   -8,   -5,   -4,   -3,   -1,    2,    3,    4,    6,    8,    9,   10,   11},
		[ENDGAME] = {  -45,  -26,  -21,  -17,  -15,   -5,   -2,    1,    9,   13,   15,   18,   22,   25,   26}
	},
	.queenMobilityBonus = {
		[MIDDLEGAME] = { -104,  -22,  -21,  -16,  -11,   -7,   -2,   -1,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,   16,   17,   18,   19},
		[ENDGAME] = {  -58,  -57,  -56,  -55,  -54,  -53,  -52,  -42,  -30,  -23,  -13,   -8,   -1,    4,   10,   14,   20,   26,   33,   35,   40,   41,   42,   43,   44,   50,   51,   52}
	},
	.virtualMobilityBonus = {
		[MIDDLEGAME] = {   44,   43,   42,   41,   40,   39,   34,   29,   27,   20,   19,   15,   11,    5,   -1,   -9,  -19,  -26,  -31,  -32,  -33,  -34,  -35,  -36,  -37,  -38,  -39,  -40},
		[ENDGAME] = {  172,   41,   28,   13,   11,   10,    6,    3,    2,    1,    0,   -1,   -2,   -3,   -4,   -5,   -6,   -7,   -8,  -12,  -16,  -19,  -25,  -29,  -31,  -39,  -40,  -41}
	},
	.minorPawnCover = {    8,    5},
	.passedPawnBonus = {
		[MIDDLEGAME] = {    0,    1,    3,    6,   18,   24,   25,    0},
		[ENDGAME] = {    0,  -18,  -15,    0,   31,   94,   95,    0}
	},
	.connectedPawnBonus = {
		[MIDDLEGAME] = {    0,   10,    2,    3,    3,   17,  112,    0},
		[ENDGAME] = {    0,    8,    1,    1,    7,   16,    5,    0}
	},
	.doubledPawnBonus = {
		[MIDDLEGAME] = {   -7,    5,   -2,    1,   -2,   -1,    7,   -4},
		[ENDGAME] = {  -26,  -26,  -16,   -9,   -3,  -17,  -26,  -29}
	},
	.isolatedPawnBonus = {
		[MIDDLEGAME] = {  -11,   -7,  -15,  -17,  -18,  -17,   -7,  -10},
		[ENDGAME] = {   17,  -13,   -3,  -13,  -13,   -3,  -11,   15}
	},
	.knightOutpostBonus = {   35,   10},
	.bishopPairBonus = {   18,   63},
	.badBishopBonus = {    0,   -1},
	.openRookFileBonus = {
		[MIDDLEGAME] = {    8,   29},
		[ENDGAME] = {   11,    4}
	},
	.connectedRookBonus = {
		[MIDDLEGAME] = {   -4,   11},
		[ENDGAME] = {   10,    2}
	},
	.connectedQueenBonus = {
		[MIDDLEGAME] = {   -6,    4,    2},
		[ENDGAME] = {   42,   30,    5}
	},
	.tempo = {   20,   20}
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

	.knightOutpostBonus[ENDGAME] = 1,

	.bishopPairBonus[ENDGAME] = 1,
	.badBishopBonus[ENDGAME] = 1,

    .openRookFileBonus[ENDGAME] = { [0 ... 1] = 1 },
	.connectedRookBonus[ENDGAME] = { [0 ... 1] = 1},

	.connectedQueenBonus[ENDGAME] = { [0 ... 2] = 1},

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
		//connected pawns
		else if(borderingMask & (board_rank[row - 1] | board_rank[row + 1]))
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
		//connected pawns
		else if(borderingMask & (board_rank[row - 1] | board_rank[row + 1]))
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
				eval[MIDDLEGAME] += hce_params.knightOutpostBonus[MIDDLEGAME];
				eval[ENDGAME] += hce_params.knightOutpostBonus[ENDGAME];
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
				eval[MIDDLEGAME] -= hce_params.knightOutpostBonus[MIDDLEGAME];
				eval[ENDGAME] -= hce_params.knightOutpostBonus[ENDGAME];
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
	int connectedRooksByRow = 0;
	int connectedRooksByColumn = 0;

	uint64_t mask = board->pieces[WHITE_ROOK];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		int row = getRow(sq);

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
		
        //rook rams
		uint64_t connections = rookMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		uint64_t connectedRooks = connections & board->pieces[BLACK_ROOK];
		connectedRooksByRow -= __builtin_popcountll(connectedRooks & board_rank[row]);
		connectedRooksByColumn -= __builtin_popcountll(connectedRooks & board_file[column]);

		mask &= mask - 1;
	}

	eval[MIDDLEGAME] += connectedRooksByRow * hce_params.connectedRookBonus[MIDDLEGAME][CONNECTED_ROW];
	eval[ENDGAME] += connectedRooksByRow * hce_params.connectedRookBonus[ENDGAME][CONNECTED_ROW];
	
	eval[MIDDLEGAME] += connectedRooksByColumn * hce_params.connectedRookBonus[MIDDLEGAME][CONNECTED_COLUMN];
	eval[ENDGAME] += connectedRooksByColumn * hce_params.connectedRookBonus[ENDGAME][CONNECTED_COLUMN];
	
	*mgScore += eval[MIDDLEGAME];
	*egScore += eval[ENDGAME];
}

void evaluateQueens(bitboard* board, int* mgScore, int* egScore)
{
	int eval[PHASE_COUNT] = {0};
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
        eval[MIDDLEGAME] += pieceBonusTable[MIDDLEGAME][WHITE_QUEEN][sq];
        eval[ENDGAME] += pieceBonusTable[ENDGAME][WHITE_QUEEN][sq];

		//mobility
		uint64_t moves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);
		
		eval[MIDDLEGAME] += hce_params.queenMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] += hce_params.queenMobilityBonus[ENDGAME][moveCount];
		
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
        eval[MIDDLEGAME] -= pieceBonusTable[MIDDLEGAME][BLACK_QUEEN][sq];
        eval[ENDGAME] -= pieceBonusTable[ENDGAME][BLACK_QUEEN][sq];

		//mobility
		uint64_t moves = queenMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		
		eval[MIDDLEGAME] -= hce_params.queenMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] -= hce_params.queenMobilityBonus[ENDGAME][moveCount];
		
        //queen & slider rams
		uint64_t rookConnections = rookMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		rookConnections &= (board->pieces[BLACK_QUEEN] | board->pieces[BLACK_ROOK]);
		connectedSlidersByRow -= __builtin_popcountll(rookConnections & board_rank[row]);
		connectedSlidersByColumn -= __builtin_popcountll(rookConnections & board_file[column]);

		uint64_t bishopConnections = bishopMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		bishopConnections &= (board->pieces[BLACK] | board->pieces[BLACK_BISHOP]);
		connectedSlidersByDiagonal -= __builtin_popcountll(bishopConnections);

		mask &= mask - 1;
	}
	
	eval[MIDDLEGAME] += connectedSlidersByRow * hce_params.connectedQueenBonus[MIDDLEGAME][CONNECTED_ROW];
	eval[ENDGAME] += connectedSlidersByRow * hce_params.connectedQueenBonus[ENDGAME][CONNECTED_ROW];
	
	eval[MIDDLEGAME] += connectedSlidersByColumn * hce_params.connectedQueenBonus[MIDDLEGAME][CONNECTED_COLUMN];
	eval[ENDGAME] += connectedSlidersByColumn * hce_params.connectedQueenBonus[ENDGAME][CONNECTED_COLUMN];
	
	eval[MIDDLEGAME] += connectedSlidersByDiagonal * hce_params.connectedQueenBonus[MIDDLEGAME][CONNECTED_DIAGONAL];
	eval[ENDGAME] += connectedSlidersByDiagonal * hce_params.connectedQueenBonus[ENDGAME][CONNECTED_DIAGONAL];

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
	eval = clamp(eval, -(MIN_MATE_SCORE - 1), MIN_MATE_SCORE - 1);
	return (ISWHITE(board->turn)) ? eval : -eval;
}