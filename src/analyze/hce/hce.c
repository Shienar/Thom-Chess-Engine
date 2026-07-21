#include "analyze/hce/hce.h"

//For simplicity, this represents white's view of the board when
//viewed in a text editor.
//
//White Piece square tables receive a rank-wise FLIP_SQUARE. (xor 56)
//Black piece square tables receive a column-wise MIRROR_SQUARE (xor 7)
//Black column-based parameters receive the column-wise MIRROR_SQUARE.
//Black row-based parameters also received MIRROR_SQUARE, since the math works out there as well given row=[0, 7].
evalParameters hce_params = {
	.genericPieceValues = {
		[MIDDLEGAME] = {   81,  276,  301,  386,  888,    0},
		[ENDGAME] = {  140,  344,  359,  633, 1144,    0}
	},
	.rawPieceTables = {
		[MIDDLEGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				   13,   39,   55,   76,   62,   31,  -11,   -6,
				   15,    8,   40,   24,   34,   45,   -2,   11,
				   -3,  -11,    2,    0,    6,   -6,  -18,   -6,
				  -16,  -24,  -10,   -5,   -5,  -13,  -29,  -17,
				  -14,  -21,  -15,  -17,  -14,  -18,  -30,  -16,
				  -16,  -14,  -12,  -20,  -20,  -13,  -20,  -21,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				 -122, -118,  -59,  -26,    9,  -37,  -68, -110,
				   -7,    1,   47,   30,   28,   59,   -3,    5,
				   15,   40,   54,   67,   77,   63,   43,   15,
				   15,   14,   43,   38,   34,   41,   13,   18,
				   -5,    6,   17,   17,   22,   13,   18,   -9,
				  -23,   -1,    3,   14,   18,    2,   -4,  -24,
				  -18,  -21,   -9,    0,    0,  -13,  -23,  -14,
				  -49,  -19,  -16,  -11,  -11,   -9,  -19,  -48
			},
			[BISHOP / 2] = {
				  -38,  -41,  -50,  -73,  -67,  -44,  -30,  -34,
				  -15,    5,   11,   10,    1,    0,    7,   -9,
				   18,   23,   29,   26,   27,   36,   36,   20,
				    2,   -2,   20,   28,   29,   16,    3,  -10,
				   -5,   -1,   -8,   23,   17,   -1,  -10,    0,
				    5,    3,    9,   -2,    6,    2,    6,   -6,
				   -3,   15,   15,    6,   -6,    9,    8,    2,
				    8,   18,   -5,  -12,   -9,  -14,    7,  -14
			},
			[ROOK / 2] = {
				   23,   15,   18,   26,   25,   34,   11,   12,
				   11,   19,   40,   39,   36,   38,    8,   10,
				   -7,   22,   25,   22,   31,   18,   28,    0,
				  -18,   -5,    4,    2,   -1,   -4,   -8,  -15,
				  -25,  -19,  -17,   -6,  -13,  -22,  -24,  -33,
				  -24,  -10,  -12,   -8,  -13,  -12,   -5,  -19,
				  -31,  -22,   -8,   -6,   -7,  -10,  -16,  -31,
				  -19,  -10,   -7,    1,    1,   -7,  -12,  -16
			},
			[QUEEN / 2] = {
				  -16,   13,   -1,   24,   21,    0,   -7,  -24,
				   11,  -19,    4,  -29,  -23,  -11,  -25,    0,
				   32,   27,    5,    4,    6,   12,   50,   38,
				   10,   -7,   -1,  -10,  -10,   -3,   -1,   12,
				   -7,   -1,  -13,  -12,   -4,  -10,   -5,   -9,
				   -4,   -1,   -4,  -10,   -7,   -5,   -2,   -1,
				    5,    2,    6,    4,    1,    8,    5,    2,
				   -1,    0,   -1,    2,   -1,   -7,   -1,  -11
			},
			[KING / 2] = {
				   82,  117,   62,   -4,  -11,   47,    4,   41,
				   13,   35,   14,   58,   45,  -15,    0,  -72,
				  -24,   34,   67,   23,    1,   17,   30,  -72,
				  -73,  -39,  -34,  -48,  -72,  -43,  -47,  -91,
				  -87,  -43,  -51,  -66,  -78,  -48,  -64,  -95,
				  -21,    8,  -29,  -39,  -31,  -29,   -2,  -25,
				   62,   54,   17,   -4,   -5,   13,   46,   57,
				   66,   75,   37,   39,   25,   40,   70,   64
			}
		},
		[ENDGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				   68,   85,   64,   24,   26,   57,   95,   81,
				    9,   44,   -6,  -35,  -39,  -22,   38,   11,
				  -12,   11,  -24,  -37,  -40,  -26,    8,  -14,
				  -26,    6,  -26,  -32,  -35,  -25,    4,  -23,
				  -29,    4,  -21,  -19,  -22,  -21,    3,  -28,
				  -28,    8,  -12,   -5,   -5,  -14,    6,  -25,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				  -66,   -6,   -3,   -4,   -6,  -11,  -17,  -80,
				  -14,    0,   -5,   -2,   -8,  -18,    9,  -20,
				   -3,   -2,   16,   11,   11,    7,    0,  -11,
				    8,   18,   27,   32,   32,   26,   20,    2,
				    8,   12,   26,   31,   31,   24,    7,    5,
				   -5,    1,    5,   23,   16,    6,   -1,   -5,
				   -4,   -5,   -1,    1,   -2,    1,   -3,   -7,
				  -15,  -19,  -12,   -4,   -4,  -14,  -23,  -16
			},
			[BISHOP / 2] = {
				    7,    7,    1,   15,    7,    5,    9,    2,
				   -4,    0,   -3,   -9,   -3,   -5,   -2,   -9,
				    4,    3,    3,   -2,  -10,    0,   -6,    1,
				    1,    7,    5,    7,   13,    4,    3,    2,
				   -6,    7,   10,    9,    7,    7,    3,   -7,
				   -4,    1,    5,   10,    6,    5,    4,   -5,
				   -8,   -5,  -21,   -5,   -3,   -8,   -6,    0,
				  -15,  -26,   -5,   -4,   -2,   -2,   19,  -12
			},
			[ROOK / 2] = {
				    6,   13,   16,    5,    8,   10,   21,    6,
				    7,   18,   15,    7,    9,   11,   19,    7,
				    4,    9,    3,   -2,   -3,    2,    7,    2,
				    6,    9,    2,    0,    0,    9,   10,    4,
				   -2,    7,    3,   -4,   -1,    3,    5,    1,
				  -12,   -7,  -10,  -12,   -8,   -9,  -16,  -12,
				   -9,   -7,   -9,  -12,  -11,  -10,   -8,  -15,
				   -9,  -10,   -7,  -17,  -16,  -11,   -7,   -8
			},
			[QUEEN / 2] = {
				   -4,   -8,   26,   13,   15,   26,   -1,   13,
				   -5,   21,   38,   70,   64,   41,   24,    7,
				  -20,    2,   36,   36,   44,   36,  -31,  -17,
				   -6,   16,   21,   40,   39,   28,   23,    3,
				   -1,    9,   22,   30,   19,    7,    8,    8,
				  -19,  -11,    3,   -1,   -1,    3,   -6,  -19,
				  -52,  -47,  -42,  -23,  -21,  -42,  -46,  -36,
				  -44,  -54,  -53,  -44,  -29,  -38,  -41,  -30
			},
			[KING / 2] = {
				 -117,  -54,  -22,  -10,   -5,  -36,  -33, -102,
				  -17,   27,   40,   25,   23,   34,   25,   -6,
				    0,   35,   46,   53,   59,   48,   33,    4,
				   -2,   32,   54,   66,   66,   52,   34,    3,
				   -9,   14,   40,   56,   61,   40,   21,   -7,
				  -23,   -4,   24,   35,   34,   21,   -3,  -26,
				  -53,  -26,   -2,    9,    7,   -4,  -24,  -57,
				  -92,  -61,  -40,  -44,  -39,  -38,  -67, -100
			}
		}
	},
	.knightMobilityBonus = {
		[MIDDLEGAME] = {  -37,  -15,   -6,    2,    9,   10,   11,   12,   13},
		[ENDGAME] = {  -38,  -12,   -3,   -2,    1,   12,   13,   14,   15}
	},
	.bishopMobilityBonus = {
		[MIDDLEGAME] = {  -30,  -21,  -14,  -11,   -6,    0,    5,    8,    9,   10,   11,   12,   13,   14},
		[ENDGAME] = {  -60,  -42,  -32,  -21,   -7,    5,    8,   14,   20,   21,   22,   23,   24,   25}
	},
	.rookMobilityBonus = {
		[MIDDLEGAME] = {  -22,  -11,   -8,   -5,   -4,   -3,   -2,    0,    1,    3,    6,   10,   11,   13,   14},
		[ENDGAME] = {  -40,  -23,  -20,  -14,  -13,   -5,   -2,    2,    9,   12,   14,   16,   19,   22,   23}
	},
	.queenMobilityBonus = {
		[MIDDLEGAME] = {  -55,  -17,  -16,  -15,  -14,  -10,   -5,   -4,   -3,   -2,   -1,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,   16},
		[ENDGAME] = { -101, -100,  -99,  -70,  -50,  -46,  -42,  -33,  -22,  -15,   -6,   -2,    5,   10,   15,   19,   25,   30,   37,   39,   45,   46,   47,   48,   49,   55,   56,   57}
	},
	.virtualMobilityBonus = {
		[MIDDLEGAME] = {   56,   55,   54,   53,   52,   51,   45,   40,   38,   31,   30,   25,   20,   14,    7,   -1,  -14,  -22,  -31,  -41,  -49,  -50,  -57,  -58,  -59,  -60,  -61,  -62},
		[ENDGAME] = {   79,   29,   24,   12,   11,   10,    8,    6,    5,    4,    3,    2,    1,    0,   -1,   -2,   -3,   -4,   -5,   -6,   -9,  -12,  -16,  -20,  -22,  -30,  -31,  -32}
	},
	.bishopPairBonus = {   19,   63},
	.openFileRookBonus = {
		[MIDDLEGAME] = {   20,   17,   15,   16,   15,    9,   36,   57},
		[ENDGAME] = {   16,    5,   12,   11,    8,    4,   -6,   -1}
	},
	.passedPawnBonus = {
		[MIDDLEGAME] = {    0,    1,    3,    7,   19,   27,   28,    0},
		[ENDGAME] = {    0,  -19,  -17,   -1,   28,   87,   88,    0}
	},
	.doubledPawnBonus = {
		[MIDDLEGAME] = {  -13,    2,   -3,    1,   -3,   -2,    5,  -10},
		[ENDGAME] = {  -26,  -25,  -16,   -9,   -3,  -17,  -26,  -28}
	},
	.isolatedPawnBonus = {
		[MIDDLEGAME] = {  -13,   -5,  -14,  -15,  -18,  -18,   -5,  -13},
		[ENDGAME] = {   17,  -14,   -4,  -11,   -9,   -2,  -11,   15}
	},
	.connectedPawnBonus = {
		[MIDDLEGAME] = {    8,    8,    6,   11,    6,    7,   11,    7},
		[ENDGAME] = {    2,    5,    5,    9,   12,    9,    7,    4}
	},
	.backwardPawnBonus = {
		[MIDDLEGAME] = {    3,    4,    5,    8,    0,    5,    7,  -10},
		[ENDGAME] = {   -1,    2,   -6,    0,    6,    2,   -5,   -3}
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

	.bishopPairBonus[ENDGAME] = 1,
    .openFileRookBonus[ENDGAME] = { [0 ... 7] = 1 },

    .passedPawnBonus[ENDGAME] = { [0 ... 7] = 1 },
    .doubledPawnBonus[ENDGAME] = { [0 ... 7] = 1 },
    .isolatedPawnBonus[ENDGAME] = { [0 ... 7] = 1 },
    .connectedPawnBonus[ENDGAME] = { [0 ... 7] = 1 },
    .backwardPawnBonus[ENDGAME] = { [0 ... 7] = 1 },
	
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
			eval[MIDDLEGAME] += hce_params.connectedPawnBonus[MIDDLEGAME][column];
			eval[ENDGAME] += hce_params.connectedPawnBonus[ENDGAME][column];
		}

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_PAWN];
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
			eval[MIDDLEGAME] -= hce_params.connectedPawnBonus[MIDDLEGAME][mirroredColumn];
			eval[ENDGAME] -= hce_params.connectedPawnBonus[ENDGAME][mirroredColumn];
		}

		mask &= mask - 1;
	}
	
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

		//piece/square value
        eval[MIDDLEGAME] += pieceBonusTable[MIDDLEGAME][WHITE_KNIGHT][sq];
        eval[ENDGAME] += pieceBonusTable[ENDGAME][WHITE_KNIGHT][sq];

		//mobility
		uint64_t moves = knightMoves(board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);

		eval[MIDDLEGAME] += hce_params.knightMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] += hce_params.knightMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_KNIGHT];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/square value
        eval[MIDDLEGAME] -= pieceBonusTable[MIDDLEGAME][BLACK_KNIGHT][sq];
        eval[ENDGAME] -= pieceBonusTable[ENDGAME][BLACK_KNIGHT][sq];

		//mobility
		uint64_t moves = knightMoves(board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);

		eval[MIDDLEGAME] -= hce_params.knightMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] -= hce_params.knightMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	*mgScore += eval[MIDDLEGAME];
	*egScore += eval[ENDGAME];
}

void evaluateBishops(bitboard* board, int* mgScore, int* egScore)
{
	int eval[PHASE_COUNT] = {0};

	uint64_t mask = board->pieces[WHITE_BISHOP];

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
        uint64_t pawns_col = board->pieces[WHITE_PAWN] & board_file[column];

		//piece/sq value
        eval[MIDDLEGAME] += pieceBonusTable[MIDDLEGAME][WHITE_ROOK][sq];
        eval[ENDGAME] += pieceBonusTable[ENDGAME][WHITE_ROOK][sq];
		
		//open rook file
        if(!pawns_col)
        {
            eval[MIDDLEGAME] += hce_params.openFileRookBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.openFileRookBonus[ENDGAME][column];
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
        uint64_t pawns_col = board->pieces[BLACK_PAWN] & board_file[column];
		
		//piece/sq value
        eval[MIDDLEGAME] -= pieceBonusTable[MIDDLEGAME][BLACK_ROOK][sq];
        eval[ENDGAME] -= pieceBonusTable[ENDGAME][BLACK_ROOK][sq];
		
		//open rook file
        if(!pawns_col)
        {
            eval[MIDDLEGAME] -= hce_params.openFileRookBonus[MIDDLEGAME][column];
            eval[ENDGAME] -= hce_params.openFileRookBonus[ENDGAME][column];
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