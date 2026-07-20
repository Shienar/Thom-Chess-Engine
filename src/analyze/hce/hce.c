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
		[MIDDLEGAME] = {   79,  334,  349,  434,  980,    0},
		[ENDGAME] = {   92,  314,  323,  558, 1059,    0}
	},
	.rawPieceTables = {
		[MIDDLEGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				   74,  108,   74,  101,   80,   96,   24,  -10,
				   -7,    3,   23,   21,   48,   59,   28,  -11,
				  -15,    9,   -1,   13,   22,   14,   19,  -13,
				  -24,   -4,   -3,   -1,   13,    8,   10,  -15,
				  -24,    6,   -6,   -8,    6,   13,   26,  -15,
				  -32,   -6,  -17,  -16,  -13,   10,   32,  -25,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				 -166, -115,  -51,  -44,   32,  -79,  -56, -122,
				  -50,  -31,   46,   41,   24,   56,   -9,  -12,
				  -26,   43,   43,   54,   72,  102,   57,   21,
				   -8,   11,   15,   41,   30,   51,    9,   19,
				   -6,    7,    7,   17,   17,   24,   19,  -10,
				   -8,    2,    8,   19,   25,   10,   20,   -3,
				  -11,  -30,    3,    2,   12,   14,   -5,  -17,
				  -79,  -19,  -30,  -13,  -11,  -16,  -10,  -33
			},
			[BISHOP / 2] = {
				  -39,  -29,  -68,  -65,  -50,  -49,  -10,  -34,
				  -15,    1,  -16,  -17,   18,   26,    5,  -32,
				  -15,   29,   27,   27,   19,   35,   30,    0,
				  -10,   -6,    7,   21,   20,   16,   -3,  -11,
				   -4,   -3,   -8,   24,   11,   -1,   -9,    7,
				   -5,    6,    7,    5,    4,   14,   11,    3,
				    1,   17,   17,   -3,    4,   18,   20,   -3,
				  -11,    5,  -10,   -5,   -3,    1,   -8,   -5
			},
			[ROOK / 2] = {
				   22,   26,   20,   35,   42,   20,   29,   21,
				   11,   10,   35,   50,   41,   42,   14,   23,
				  -22,    8,    8,   19,   10,   27,   53,    6,
				  -32,  -15,   -8,    4,    5,   17,   -3,  -18,
				  -39,  -30,  -21,   -5,  -12,  -15,    3,  -28,
				  -37,  -19,  -16,   -8,    2,    4,    5,  -20,
				  -21,  -11,  -15,    3,    5,   12,   -3,  -42,
				  -18,   -5,   -4,    8,    6,   -6,  -17,  -11
			},
			[QUEEN / 2] = {
				  -37,   -1,   15,   17,   45,   34,   41,   14,
				  -13,  -39,  -19,  -15,  -28,   27,    5,   29,
				   -7,  -13,   -8,   -2,    5,   36,   46,   49,
				  -19,  -25,  -22,  -28,  -13,   -5,   -6,   -1,
				  -13,  -23,  -18,  -11,  -17,   -5,   -4,   -4,
				  -11,   -5,  -13,    3,    4,   -4,   13,    8,
				  -15,    1,   13,    3,   15,   22,    9,   12,
				    4,   -1,   11,    8,    2,   -4,    0,  -24
			},
			[KING / 2] = {
				  -18,   32,   26,  -48,  -55,  -14,    8,   40,
				  -14,  -10,  -42,   12,  -10,  -13,  -27,  -45,
				  -45,   14,  -15,  -34,  -30,    8,   17,  -46,
				  -54,  -54,  -55,  -79,  -82,  -62,  -53,  -85,
				  -80,  -44,  -69,  -87,  -92,  -68,  -69, -101,
				  -29,  -22,  -47,  -63,  -58,  -46,  -20,  -39,
				   12,   15,   -4,  -51,  -43,  -15,   11,    6,
				   10,   40,   12,  -29,   16,   -8,   19,   29
			}
		},
		[ENDGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				  162,  162,  154,  115,  126,  129,  165,  174,
				   75,   90,   65,   38,   41,   39,   75,   69,
				   25,   25,    4,    2,   -3,   -1,   21,   13,
				    5,    9,   -5,  -18,   -3,   -4,    7,    0,
				   -3,   19,    2,   -3,   -3,    7,    1,   -9,
				    8,    9,    8,    9,    8,    2,    9,   -7,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				  -62,  -20,    0,  -15,  -11,  -22,  -34,  -94,
				  -23,   -6,   -5,   10,   -1,  -27,  -10,  -42,
				  -15,   -1,   15,   15,   -4,    5,   -2,  -27,
				   -5,   13,   21,   23,   29,   23,   13,   -8,
				    3,   12,   22,   40,   23,   31,   16,   -6,
				    5,   16,   14,   31,   25,   11,    6,    7,
				   -4,   -3,   14,   12,   16,    8,   -1,  -13,
				  -15,  -26,    0,    7,    4,   -2,  -17,  -30
			},
			[BISHOP / 2] = {
				   -1,    3,    3,   13,   13,    8,    4,   -4,
				    1,    5,    9,   -1,   11,   -1,    8,  -12,
				    6,    9,   10,    4,    0,    5,    5,    3,
				   11,   11,   12,   10,   17,   19,    9,   12,
				    9,   13,   13,   28,    9,   17,    4,    6,
				   -5,   10,   15,   20,   27,   17,    7,   -6,
				    4,   -4,    3,    5,   13,   11,   -2,  -11,
				  -10,    0,  -10,   15,   11,    9,   10,   -7
			},
			[ROOK / 2] = {
				   14,   19,   17,   17,   10,   11,   13,    6,
				   16,   17,   16,   15,    0,    1,    8,    2,
				    3,    4,    3,    5,  -11,  -12,   -9,  -11,
				    2,    4,    5,    1,   -1,    5,   -3,   -6,
				    1,    6,    6,    7,   -9,    2,    2,  -10,
				   -3,    3,   -3,    4,   -1,    1,  -16,  -18,
				    7,    8,    0,   10,   -1,    3,  -10,   -3,
				   -3,   10,    4,   -2,   -9,   -9,    2,   -3
			},
			[QUEEN / 2] = {
				    0,   31,   46,   36,   38,   36,    8,   22,
				  -10,   28,   50,   76,   79,   48,   42,    5,
				  -11,   13,   38,   58,   57,   45,    5,    2,
				    9,   26,   30,   51,   69,   47,   52,   27,
				   -5,   29,   29,   59,   37,   45,   36,   25,
				   -7,   -9,   22,   28,   31,   22,   15,    7,
				  -19,  -11,  -17,   -4,    6,  -13,  -33,  -39,
				  -14,  -12,   -6,  -27,    0,   -6,  -19,  -26
			},
			[KING / 2] = {
				  -91,  -39,  -18,   -5,   -7,    4,   -3,  -65,
				   -8,   28,   28,   26,   27,   50,   30,   15,
				   12,   26,   35,   39,   44,   49,   52,   18,
				    3,   27,   37,   47,   48,   44,   38,   12,
				  -14,    6,   29,   42,   43,   37,   21,   -1,
				   -9,   -3,   25,   36,   37,   30,   17,    2,
				  -35,   -3,   13,   13,   14,   14,  -13,  -30,
				  -65,  -43,  -30,   -4,  -22,   -7,  -43,  -53
			}
		}
	},
	.knightMobilityBonus = {
		[MIDDLEGAME] = {  -60,   -9,   -1,   14,   13,   15,   18,   20,   22},
		[ENDGAME] = {  -71,  -23,  -14,    0,    1,    5,    9,   12,   15}
	},
	.bishopMobilityBonus = {
		[MIDDLEGAME] = { -115,  -76,  -62,  -48,  -26,   -7,   -2,    2,    4,   10,   15,   25,   29,   40},
		[ENDGAME] = { -134, -100,  -85,  -70,  -44,  -24,  -20,  -15,  -10,   -6,   -3,    7,   14,   14}
	},
	.rookMobilityBonus = {
		[MIDDLEGAME] = {  -37,  -26,  -17,  -11,  -13,   -8,   -3,   -1,    2,    6,   11,   19,   24,   31,   35},
		[ENDGAME] = {  -49,  -33,  -24,  -14,  -15,  -11,   -6,   -3,    1,    5,    8,   18,   23,   29,   32}
	},
	.queenMobilityBonus = {
		[MIDDLEGAME] = { -129, -115, -101,  -71,  -42,  -27,  -18,  -14,  -10,   -6,   -4,    0,    5,    8,   13,   40,   45,   50,   55,   60,   65,   70,   72,   74,   76,   78,   80,   82},
		[ENDGAME] = { -152, -130, -113,  -96,  -48,  -33,  -25,   -6,   15,   20,   24,   30,   35,   38,   42,   60,   65,   70,   75,   80,   85,   90,   90,   90,   90,   90,   90,   90}
	},
	.virtualMobilityBonus = {
		[MIDDLEGAME] = {   96,   82,   73,   70,   69,   55,   35,   18,    8,   -2,   -7,  -15,  -21,  -30,  -37,  -45,  -53,  -60,  -66,  -68,  -73,  -80,  -82,  -75,  -79,  -71,  -73,  -65},
		[ENDGAME] = {   60,   45,   43,   40,   39,   24,   19,   10,    8,    7,    3,   -4,   -6,   -9,  -13,  -15,  -19,  -24,  -28,  -33,  -38,  -43,  -43,  -46,  -49,  -49,  -52,  -52}
	},
	.bishopPairBonus = {   21,   53},
	.openFileRookBonus = {
		[MIDDLEGAME] = {    7,    5,   -1,   10,    3,    7,   14,   34},
		[ENDGAME] = {   -3,   -6,   -4,    9,    1,   -3,   -6,   -5}
	},
	.passedPawnBonus = {
		[MIDDLEGAME] = {    0,    8,   12,   15,   30,   64,   91,    0},
		[ENDGAME] = {    0,   11,   16,   22,   60,   97,  128,    0}
	},
	.doubledPawnBonus = {
		[MIDDLEGAME] = {    1,    1,    1,   -4,   -2,    3,   -2,   -4},
		[ENDGAME] = {   -9,   -6,   -5,   -5,    0,   -3,   -5,  -13}
	},
	.isolatedPawnBonus = {
		[MIDDLEGAME] = {   -4,   -9,   -5,  -10,   -9,    0,   -5,   -2},
		[ENDGAME] = {    3,   -1,    3,   -6,   -1,    5,    4,   -1}
	},
	.connectedPawnBonus = {
		[MIDDLEGAME] = {    4,    4,    5,    7,    6,    5,    4,    2},
		[ENDGAME] = {    4,    5,    6,    7,    7,    7,    5,    4}
	},
	.backwardPawnBonus = {
		[MIDDLEGAME] = {   -1,   -2,   -3,   -4,   -3,   -2,   -2,   -1},
		[ENDGAME] = {   -2,   -2,   -4,   -4,   -4,   -4,   -3,   -2}
	},
	.tempo = {   10,   10}
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

/*************************/
/** Piece Square Tables **/
/*************************/
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

void evaluatePieceSquareTables(bitboard* board, int* middlegameScore, int* endgameScore)
{
    int eval[PHASE_COUNT][2] = {0};

    uint64_t mask = board->pieces_all;
    while(mask)
    {
        int sq = __builtin_ctzll(mask);
        int piece = findPieceOnSquare(board, sq);

        eval[MIDDLEGAME][COLOR(piece)] += pieceBonusTable[MIDDLEGAME][piece][sq];
        eval[ENDGAME][COLOR(piece)] += pieceBonusTable[ENDGAME][piece][sq];

        mask &= (mask - 1);
    }

    *middlegameScore += eval[MIDDLEGAME][WHITE] - eval[MIDDLEGAME][BLACK];
    *endgameScore += eval[ENDGAME][WHITE] - eval[ENDGAME][BLACK];
}

/**********************************/
/**  Open rook file, bishop pair **/
/**********************************/

void evaluateSimplePieceDetails(bitboard* board, int* middlegameScore, int* endgameScore)
{
    int eval[PHASE_COUNT] = {0};

	//Bishop pair
	if(__builtin_popcountll(board->pieces[WHITE_BISHOP]) >= 2)
	{
		eval[MIDDLEGAME] += hce_params.bishopPairBonus[MIDDLEGAME];
		eval[ENDGAME] += hce_params.bishopPairBonus[ENDGAME];
	}
	if(__builtin_popcountll(board->pieces[BLACK_BISHOP]) >= 2)
	{
		eval[MIDDLEGAME] -= hce_params.bishopPairBonus[MIDDLEGAME];
		eval[ENDGAME] -= hce_params.bishopPairBonus[ENDGAME];
	}

	//Open rook file.
    uint64_t whiteRooks = board->pieces[WHITE_ROOK];
    uint64_t blackRooks = board->pieces[BLACK_ROOK];

    while(whiteRooks)
    {
        int column = getColumn(__builtin_ctzll(whiteRooks));
        uint64_t pawns_col = board->pieces[WHITE_PAWN] & board_file[column];

        if(!pawns_col)
        {
            eval[MIDDLEGAME] += hce_params.openFileRookBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.openFileRookBonus[ENDGAME][column];
        }

        whiteRooks &= (whiteRooks - 1);
    }

    while(blackRooks)
    {
        int column = getColumn(__builtin_ctzll(blackRooks));
        uint64_t pawns_col = board->pieces[BLACK_PAWN] & board_file[column];

		int mirroredColumn = MIRROR_SQUARE(column);
    
        if(!pawns_col)
        {
            eval[MIDDLEGAME] -= hce_params.openFileRookBonus[MIDDLEGAME][mirroredColumn];
            eval[ENDGAME] -= hce_params.openFileRookBonus[ENDGAME][mirroredColumn];
        }

        blackRooks &= (blackRooks - 1);
    }

    *middlegameScore += eval[MIDDLEGAME];
    *endgameScore += eval[ENDGAME];
}

/***********************************************************************************/
/** Passed Pawns, Doubled Pawns, Isolated Pawns, Backward Pawns, Pawn Connections **/
/***********************************************************************************/

void evaluatePawnDetails(bitboard* board, int* middlegameScore, int* endgameScore)
{
    int eval[PHASE_COUNT] = {0};

    uint64_t whitePawns = board->pieces[WHITE_PAWN];
    uint64_t blackPawns = board->pieces[BLACK_PAWN];

    uint64_t mask = whitePawns;
    while(mask)
    {
		int sq = __builtin_ctzll(mask);
        int column = getColumn(sq);
		int row = getRow(sq);
        
        //Passed Pawns
        if((blackPawns & board_file[column]) == 0)
        {
            eval[MIDDLEGAME] += hce_params.passedPawnBonus[MIDDLEGAME][row];
            eval[ENDGAME] += hce_params.passedPawnBonus[ENDGAME][row];
        }

        //Doubled pawns
        if(__builtin_popcountll(whitePawns & board_file[column]) > 1)
        {
            eval[MIDDLEGAME] += hce_params.doubledPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.doubledPawnBonus[ENDGAME][column];
        }

		uint64_t borderingMask = whitePawns & bordering_files[column];
        //Isolated Pawns
        if(!borderingMask)
        {
            eval[MIDDLEGAME] += hce_params.isolatedPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.isolatedPawnBonus[ENDGAME][column];
        }
		//Backward pawns
		else if((borderingMask & (sq - 1)) == 0)
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
    
    mask = blackPawns;
    while(mask)
    {
		int sq = __builtin_ctzll(mask);
        int column = getColumn(sq);
		int row = getRow(sq);

		int mirroredColumn = MIRROR_SQUARE(column);
		int mirroredRow = MIRROR_SQUARE(row);
        
        //Passed Pawns
        if((whitePawns & board_file[column]) == 0)
        {
            eval[MIDDLEGAME] -= hce_params.passedPawnBonus[MIDDLEGAME][mirroredRow];
            eval[ENDGAME] -= hce_params.passedPawnBonus[ENDGAME][mirroredRow];
        }
        
        //Doubled pawns
        if(__builtin_popcountll(blackPawns & board_file[column]) > 1)
        {
            eval[MIDDLEGAME] -= hce_params.doubledPawnBonus[MIDDLEGAME][mirroredColumn];
            eval[ENDGAME] -= hce_params.doubledPawnBonus[ENDGAME][mirroredColumn];
        }

		uint64_t borderingMask = blackPawns & bordering_files[column];
        //Isolated Pawns
        if(!borderingMask)
        {
            eval[MIDDLEGAME] -= hce_params.isolatedPawnBonus[MIDDLEGAME][mirroredColumn];
            eval[ENDGAME] -= hce_params.isolatedPawnBonus[ENDGAME][mirroredColumn];
        }
		//Backward pawns
		else if((borderingMask & ~(sq - 1)) == 0)
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

    *middlegameScore += eval[MIDDLEGAME];
    *endgameScore += eval[ENDGAME];
}

/************************/
/** (Virtual) Mobility **/
/************************/
void evaluateMobility(bitboard* board, int* middlegameScore, int* endgameScore)
{
	int eval[2] = {0};

	uint64_t whitePieces = board->pieces_side[WHITE];
	uint64_t blackPieces = board->pieces_side[BLACK];

	//Knight Mobility
	uint64_t pieces = board->pieces[WHITE_KNIGHT];
	while(pieces)
	{
		uint64_t moves = knightMoves(whitePieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);

		eval[MIDDLEGAME] += hce_params.knightMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] += hce_params.knightMobilityBonus[ENDGAME][moveCount];

		pieces &= pieces - 1;
	}
	pieces = board->pieces[BLACK_KNIGHT];
	while(pieces)
	{
		uint64_t moves = knightMoves(blackPieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);

		eval[MIDDLEGAME] -= hce_params.knightMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] -= hce_params.knightMobilityBonus[ENDGAME][moveCount];

		pieces &= pieces - 1;
	}

	//Bishop Mobility
	pieces = board->pieces[WHITE_BISHOP];
	while(pieces)
	{
		uint64_t moves = bishopMoves(whitePieces, blackPieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);

		eval[MIDDLEGAME] += hce_params.bishopMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] += hce_params.bishopMobilityBonus[ENDGAME][moveCount];
		
		pieces &= pieces - 1;
	}
	pieces = board->pieces[BLACK_BISHOP];
	while(pieces)
	{
		uint64_t moves = bishopMoves(blackPieces, whitePieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);

		eval[MIDDLEGAME] -= hce_params.bishopMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] -= hce_params.bishopMobilityBonus[ENDGAME][moveCount];

		pieces &= pieces - 1;
	}

	//Rook Mobility
	pieces = board->pieces[WHITE_ROOK];
	while(pieces)
	{
		uint64_t moves = rookMoves(whitePieces, blackPieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);
		
		eval[MIDDLEGAME] += hce_params.rookMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] += hce_params.rookMobilityBonus[ENDGAME][moveCount];

		pieces &= pieces - 1;
	}
	pieces = board->pieces[BLACK_ROOK];
	while(pieces)
	{
		uint64_t moves = rookMoves(blackPieces, whitePieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);
		
		eval[MIDDLEGAME] -= hce_params.rookMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] -= hce_params.rookMobilityBonus[ENDGAME][moveCount];

		pieces &= pieces - 1;
	}

	//Queen Mobility
	pieces = board->pieces[WHITE_QUEEN];
	while(pieces)
	{
		uint64_t moves = rookMoves(whitePieces, blackPieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);
		
		eval[MIDDLEGAME] += hce_params.queenMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] += hce_params.queenMobilityBonus[ENDGAME][moveCount];
		
		pieces &= pieces - 1;
	}
	pieces = board->pieces[BLACK_QUEEN];
	while(pieces)
	{
		uint64_t moves = rookMoves(blackPieces, whitePieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);
		
		eval[MIDDLEGAME] -= hce_params.queenMobilityBonus[MIDDLEGAME][moveCount];
		eval[ENDGAME] -= hce_params.queenMobilityBonus[ENDGAME][moveCount];

		pieces &= pieces - 1;
	}

	//Virtual Mobility
	pieces = board->pieces[WHITE_KING];
	while(pieces)
	{
		uint64_t virtualMoves = queenMoves(whitePieces, blackPieces, __builtin_ctzll(pieces));
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
		eval[MIDDLEGAME] += hce_params.virtualMobilityBonus[MIDDLEGAME][virtualMoveCount];
		eval[ENDGAME] += hce_params.virtualMobilityBonus[ENDGAME][virtualMoveCount];

		pieces &= pieces - 1;
	}
	pieces = board->pieces[BLACK_KING];
	while(pieces)
	{
		uint64_t virtualMoves = queenMoves(blackPieces, whitePieces, __builtin_ctzll(pieces));
		int virtualMoveCount = __builtin_popcountll(virtualMoves);

		eval[MIDDLEGAME] -= hce_params.virtualMobilityBonus[MIDDLEGAME][virtualMoveCount];
		eval[ENDGAME] -= hce_params.virtualMobilityBonus[ENDGAME][virtualMoveCount];

		pieces &= pieces - 1;
	}

	*middlegameScore += eval[MIDDLEGAME];
	*endgameScore += eval[ENDGAME];
}

/******************/
/** Tapered Eval **/
/******************/
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
    int midgame_eval = 0;
    int endgame_eval = 0;

    evaluatePieceSquareTables(board, &midgame_eval, &endgame_eval);
    evaluateSimplePieceDetails(board, &midgame_eval, &endgame_eval);
    evaluatePawnDetails(board, &midgame_eval, &endgame_eval);
	evaluateMobility(board, &midgame_eval, &endgame_eval);

	midgame_eval += hce_params.tempo[MIDDLEGAME];
	endgame_eval += hce_params.tempo[ENDGAME];

    int eval = evaluatePhasedScore(board, midgame_eval, endgame_eval);

	if(ISBLACK(board->turn))
		return -eval;
	else
		return eval;
}