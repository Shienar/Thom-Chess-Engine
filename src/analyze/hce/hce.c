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
		[MIDDLEGAME] = {   76,  260,  299,  367,  804,    0},
		[ENDGAME] = {  141,  348,  378,  649, 1216,    0}
	},
	.rawPieceTables = {
		[MIDDLEGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				    4,   34,   52,   71,   56,   25,  -13,  -13,
				    8,    9,   38,   26,   34,   43,    0,    5,
				   -8,   -7,    6,    5,   10,   -3,  -14,  -11,
				  -19,  -15,   -4,    3,    1,   -7,  -19,  -22,
				  -20,  -22,  -11,  -12,  -12,  -14,  -28,  -23,
				  -21,  -12,  -10,  -20,  -20,  -11,  -17,  -26,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				 -110, -114,  -48,  -21,   18,  -29,  -60,  -97,
				  -29,  -21,   23,    7,    5,   35,  -24,  -17,
				    1,   20,   33,   45,   54,   43,   24,    0,
				    7,   19,   42,   38,   36,   40,   18,   10,
				    5,   18,   28,   28,   33,   24,   30,    1,
				  -13,   10,   14,   23,   27,   13,    6,  -14,
				  -13,  -15,   -1,    9,    9,   -5,  -16,  -11,
				  -44,  -16,  -10,   -3,   -3,   -4,  -15,  -43
			},
			[BISHOP / 2] = {
				  -39,  -42,  -53,  -75,  -68,  -48,  -33,  -35,
				  -10,    6,   10,   11,    1,    0,   10,   -4,
				   26,   26,   35,   26,   27,   38,   38,   25,
				    4,    1,   22,   30,   30,   18,    6,   -9,
				   -3,   -1,   -5,   25,   19,    1,   -9,    3,
				    4,    4,    9,   -3,    3,    1,    7,   -7,
				   -8,   11,   10,    3,   -8,    5,    5,   -5,
				    6,   15,   -8,  -11,   -8,  -17,    3,  -14
			},
			[ROOK / 2] = {
				   27,   10,    8,   14,   13,   22,    6,   17,
				   13,   14,   25,   26,   23,   22,    3,   13,
				    3,   22,   18,   10,   20,   10,   29,    9,
				   -5,    1,   -2,   -5,   -9,  -10,   -2,   -3,
				  -12,  -11,  -20,  -10,  -16,  -25,  -15,  -21,
				  -15,   -2,  -14,  -11,  -16,  -13,    3,  -11,
				  -22,  -15,  -11,  -11,  -10,  -14,   -9,  -22,
				  -11,    0,   -3,    4,    4,   -3,   -3,   -9
			},
			[QUEEN / 2] = {
				   -8,   17,    0,   21,   23,   -2,   -3,  -15,
				   20,  -16,    2,  -30,  -23,  -15,  -22,    9,
				   36,   29,    5,    2,    3,   12,   52,   43,
				   13,   -6,   -1,  -12,  -12,   -4,   -1,   13,
				   -6,    1,  -14,  -13,   -7,  -10,   -3,   -9,
				   -3,   -2,   -6,  -12,   -9,   -6,   -3,   -1,
				    2,    1,    2,    0,   -2,    5,    3,    1,
				   -8,    0,   -5,    2,   -2,  -10,    1,  -17
			},
			[KING / 2] = {
				  133,  144,   70,  -15,  -19,   54,    6,   67,
				   21,   29,    4,   51,   41,  -20,   -2,  -78,
				  -19,   31,   58,   13,  -13,    9,   28,  -71,
				  -72,  -43,  -41,  -59,  -84,  -51,  -49,  -92,
				  -86,  -44,  -55,  -73,  -85,  -53,  -64,  -95,
				  -20,    7,  -30,  -40,  -33,  -30,   -2,  -22,
				   66,   56,   18,   -2,   -4,   15,   50,   61,
				   69,   78,   42,   39,   25,   46,   72,   64
			}
		},
		[ENDGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				   66,   83,   61,   22,   26,   57,   94,   78,
				    6,   39,  -11,  -40,  -42,  -25,   34,    8,
				  -12,    9,  -26,  -38,  -39,  -26,    7,  -14,
				  -23,    8,  -24,  -28,  -28,  -21,    6,  -21,
				  -28,    4,  -20,  -15,  -17,  -18,    4,  -26,
				  -27,    8,  -13,   -5,   -4,  -14,    6,  -24,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				  -68,   -3,    0,    1,   -2,   -7,  -15,  -82,
				  -21,   -6,   -9,   -5,  -12,  -21,    3,  -27,
				  -11,   -6,   12,    7,    6,    3,   -3,  -18,
				    0,   12,   20,   24,   24,   19,   13,   -5,
				   10,   17,   31,   35,   35,   29,   12,    7,
				   -3,    5,    9,   28,   21,   10,    4,   -3,
				   -2,   -3,    4,    5,    2,    6,   -1,   -6,
				  -13,  -15,   -8,    2,    1,   -9,  -18,  -13
			},
			[BISHOP / 2] = {
				    5,    6,    0,   14,    6,    4,    8,   -2,
				   -6,    0,   -1,   -8,   -2,   -4,   -4,  -10,
				    2,    4,    3,    0,   -7,    1,   -5,   -1,
				    1,    8,    6,    8,   14,    6,    4,    2,
				   -7,    8,   12,   11,    9,   10,    5,   -9,
				   -6,    1,    7,   12,    9,    6,    4,   -7,
				  -14,  -10,  -19,   -4,   -2,   -8,   -9,   -4,
				  -17,  -28,   -2,   -4,   -2,    2,   18,  -14
			},
			[ROOK / 2] = {
				    8,   10,   19,   10,   13,   13,   20,   10,
				   10,   17,   19,   12,   13,   14,   18,   10,
				    6,    6,    5,    2,    2,    4,    4,    4,
				    7,    3,    3,    3,    3,   10,    4,    5,
				    0,    1,    4,   -2,    1,    3,   -1,    2,
				   -9,  -12,  -10,  -10,   -6,   -9,  -20,   -9,
				   -6,  -12,   -8,   -9,   -9,   -9,  -13,  -12,
				  -12,  -18,  -12,  -21,  -21,  -18,  -14,  -11
			},
			[QUEEN / 2] = {
				  -10,  -12,   25,   14,   11,   27,   -5,    7,
				   -9,   20,   39,   71,   63,   43,   22,    4,
				  -19,    2,   36,   38,   47,   36,  -30,  -15,
				   -4,   16,   20,   40,   40,   29,   25,    7,
				    1,   10,   22,   30,   20,    8,    9,   13,
				  -16,   -6,    5,    1,    2,    3,   -1,  -15,
				  -49,  -44,  -37,  -17,  -16,  -37,  -41,  -34,
				  -43,  -60,  -56,  -60,  -46,  -44,  -51,  -29
			},
			[KING / 2] = {
				 -128,  -61,  -23,   -8,   -4,  -37,  -33, -108,
				  -18,   29,   42,   26,   24,   34,   25,   -5,
				    0,   36,   49,   56,   62,   50,   33,    4,
				   -2,   33,   56,   69,   69,   53,   34,    3,
				   -9,   15,   42,   59,   63,   40,   22,   -7,
				  -23,   -4,   25,   36,   34,   21,   -4,  -27,
				  -54,  -26,   -2,    9,    7,   -5,  -26,  -59,
				  -93,  -62,  -41,  -41,  -38,  -39,  -69, -101
			}
		}
	},
	.knightMobilityBonus = {
		[MIDDLEGAME] = {  -32,  -14,   -5,    2,    8,    9,   10,   11,   12},
		[ENDGAME] = {  -41,   -9,   -1,    0,    2,   12,   13,   14,   15}
	},
	.bishopMobilityBonus = {
		[MIDDLEGAME] = {  -30,  -19,  -13,  -11,   -6,    0,    5,    8,    9,   10,   11,   12,   13,   14},
		[ENDGAME] = {  -56,  -37,  -28,  -17,   -6,    6,    8,   12,   18,   19,   20,   21,   22,   23}
	},
	.rookMobilityBonus = {
		[MIDDLEGAME] = {  -22,  -11,   -9,   -6,   -5,   -3,   -1,    2,    3,    4,    6,    8,    9,   13,   14},
		[ENDGAME] = {  -42,  -26,  -21,  -17,  -14,   -5,   -3,    1,    9,   13,   15,   18,   22,   24,   25}
	},
	.queenMobilityBonus = {
		[MIDDLEGAME] = {  -50,  -18,  -17,  -16,  -13,   -9,   -4,   -3,   -2,   -1,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,   15,   16,   17},
		[ENDGAME] = {  -97,  -96,  -95,  -73,  -63,  -55,  -48,  -37,  -25,  -17,   -7,   -3,    5,   10,   16,   20,   27,   32,   39,   41,   46,   48,   49,   50,   51,   59,   60,   63}
	},
	.virtualMobilityBonus = {
		[MIDDLEGAME] = {   53,   52,   51,   50,   48,   47,   41,   35,   33,   27,   26,   22,   17,   11,    4,   -4,  -16,  -25,  -33,  -43,  -46,  -47,  -48,  -49,  -50,  -51,  -52,  -53},
		[ENDGAME] = {   85,   37,   25,   12,   11,   10,    8,    6,    5,    4,    3,    2,    1,    0,   -1,   -2,   -3,   -4,   -5,   -6,  -10,  -13,  -18,  -22,  -25,  -34,  -35,  -36}
	},
	.pawnAttacks = {
		[MIDDLEGAME] = {   39,   53,   53,   63},
		[ENDGAME] = {   23,   45,   20,  -42}
	},
	.minorPawnCover = {    8,    3},
	.passedPawnBonus = {
		[MIDDLEGAME] = {    0,    1,    2,    6,   17,   24,   25,    0},
		[ENDGAME] = {    0,  -18,  -16,   -1,   31,   94,   95,    0}
	},
	.connectedPawnBonus = {
		[MIDDLEGAME] = {    0,   10,    1,    3,    4,   18,  116,    0},
		[ENDGAME] = {    0,    8,    1,    1,    7,   15,    3,    0}
	},
	.doubledPawnBonus = {
		[MIDDLEGAME] = {   -7,    5,   -2,    0,   -3,   -1,    6,   -4},
		[ENDGAME] = {  -26,  -26,  -16,   -8,   -2,  -17,  -26,  -29}
	},
	.isolatedPawnBonus = {
		[MIDDLEGAME] = {  -11,   -7,  -15,  -16,  -18,  -18,   -7,  -10},
		[ENDGAME] = {   17,  -13,   -3,  -13,  -13,   -3,  -11,   15}
	},
	.knightOutpostBonus = {   33,   10},
	.bishopPairBonus = {   18,   64},
	.badBishopBonus = {   -2,   -5},
	.openRookFileBonus = {
		[MIDDLEGAME] = {    8,   29},
		[ENDGAME] = {   11,    4}
	},
	.connectedRookBonus = {
		[MIDDLEGAME] = {   -4,   11},
		[ENDGAME] = {   10,    2}
	},
	.connectedQueenBonus = {
		[MIDDLEGAME] = {   -6,    4,    8},
		[ENDGAME] = {   42,   32,    6}
	},
	.tempo = {   21,   22}
};


evalParameters is_param_eg = {
    .genericPieceValues[ENDGAME] = { [0 ... PIECE_TYPE_COUNT - 1] = 1 },
    .rawPieceTables[ENDGAME] = { [0 ... 5] = { [0 ... 63] = 1 } },

    .knightMobilityBonus[ENDGAME] = { [0 ... 8] = 1 },
    .bishopMobilityBonus[ENDGAME] = { [0 ... 13] = 1 },
    .rookMobilityBonus[ENDGAME] = { [0 ... 14] = 1 },
    .queenMobilityBonus[ENDGAME] = { [0 ... 27] = 1 },
    .virtualMobilityBonus[ENDGAME] = { [0 ... 27] = 1 },

	.pawnAttacks[ENDGAME] = { [0 ... 3] = 1 },
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
}

void evaluatePawns(bitboard* board, int* mgScore, int* egScore)
{
	int eval[PHASE_COUNT] = {0};

	uint64_t mask = board->pieces[WHITE_PAWN];
	int protectedCount = __builtin_popcountll((mask >> 8) & (board->pieces[WHITE_BISHOP] | board->pieces[WHITE_KNIGHT]));

	int pawnThreats = (~(board->pieces[BLACK_PAWN] | board->pieces[BLACK_KING])) & (WHITE_PAWN_LEFTATTACKS(board) | WHITE_PAWN_RIGHTATTACKS(board));
	while(pawnThreats)
	{
		int pc = findPieceOnSquare(board, __builtin_ctzll(pawnThreats));

		if(ISKNIGHT(pc))
		{
			eval[MIDDLEGAME] += hce_params.pawnAttacks[MIDDLEGAME][ATTACKING_KNIGHT];
			eval[ENDGAME] += hce_params.pawnAttacks[ENDGAME][ATTACKING_KNIGHT];
		}
		else if(ISBISHOP(pc))
		{
			eval[MIDDLEGAME] += hce_params.pawnAttacks[MIDDLEGAME][ATTACKING_BISHOP];
			eval[ENDGAME] += hce_params.pawnAttacks[ENDGAME][ATTACKING_BISHOP];
		}
		else if(ISROOK(pc))
		{
			eval[MIDDLEGAME] += hce_params.pawnAttacks[MIDDLEGAME][ATTACKING_ROOK];
			eval[ENDGAME] += hce_params.pawnAttacks[ENDGAME][ATTACKING_ROOK];
		}
		else if(ISQUEEN(pc))
		{
			eval[MIDDLEGAME] += hce_params.pawnAttacks[MIDDLEGAME][ATTACKING_QUEEN];
			eval[ENDGAME] += hce_params.pawnAttacks[ENDGAME][ATTACKING_QUEEN];
		}

		pawnThreats &= pawnThreats - 1;
	}

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

	pawnThreats = (~(board->pieces[WHITE_PAWN] | board->pieces[WHITE_KING])) & (BLACK_PAWN_LEFTATTACKS(board) | BLACK_PAWN_RIGHTATTACKS(board));
	while(pawnThreats)
	{
		int pc = findPieceOnSquare(board, __builtin_ctzll(pawnThreats));

		if(ISKNIGHT(pc))
		{
			eval[MIDDLEGAME] -= hce_params.pawnAttacks[MIDDLEGAME][ATTACKING_KNIGHT];
			eval[ENDGAME] -= hce_params.pawnAttacks[ENDGAME][ATTACKING_KNIGHT];
		}
		else if(ISBISHOP(pc))
		{
			eval[MIDDLEGAME] -= hce_params.pawnAttacks[MIDDLEGAME][ATTACKING_BISHOP];
			eval[ENDGAME] -= hce_params.pawnAttacks[ENDGAME][ATTACKING_BISHOP];
		}
		else if(ISROOK(pc))
		{
			eval[MIDDLEGAME] -= hce_params.pawnAttacks[MIDDLEGAME][ATTACKING_ROOK];
			eval[ENDGAME] -= hce_params.pawnAttacks[ENDGAME][ATTACKING_ROOK];
		}
		else if(ISQUEEN(pc))
		{
			eval[MIDDLEGAME] -= hce_params.pawnAttacks[MIDDLEGAME][ATTACKING_QUEEN];
			eval[ENDGAME] -= hce_params.pawnAttacks[ENDGAME][ATTACKING_QUEEN];
		}

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
	 	badPawns -= __builtin_popcountll(board->pieces[BLACK_PAWN] & LIGHT_SQUARES);
	if(mask & DARK_SQUARES)
	 	badPawns -= __builtin_popcountll(board->pieces[BLACK_PAWN] & DARK_SQUARES);

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
				eval[MIDDLEGAME] += hce_params.openRookFileBonus[MIDDLEGAME][SEMI_OPEN_FILE];
				eval[ENDGAME] += hce_params.openRookFileBonus[ENDGAME][SEMI_OPEN_FILE];
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
				eval[MIDDLEGAME] -= hce_params.openRookFileBonus[MIDDLEGAME][SEMI_OPEN_FILE];
				eval[ENDGAME] -= hce_params.openRookFileBonus[ENDGAME][SEMI_OPEN_FILE];
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
		bishopConnections &= (board->pieces[BLACK_QUEEN] | board->pieces[BLACK_BISHOP]);
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