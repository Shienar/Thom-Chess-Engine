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
		[MIDDLEGAME] = {   63,  334,  347,  433,  972,    0},
		[ENDGAME] = {   92,  313,  321,  553, 1048,    0}
	},
	.rawPieceTables = {
		[MIDDLEGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				   79,  111,   73,  100,   79,  100,   24,  -11,
				   -9,    0,   20,   19,   50,   59,   28,  -12,
				  -16,    8,   -3,   13,   22,   14,   19,  -14,
				  -25,   -5,   -4,   -2,   15,    8,   10,  -15,
				  -25,    6,   -6,   -8,    7,   14,   27,  -15,
				  -33,   -6,  -18,  -17,  -13,   11,   33,  -25,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				 -167, -113,  -49,  -45,   34,  -81,  -54, -119,
				  -53,  -34,   45,   41,   23,   56,   -9,  -13,
				  -29,   43,   41,   52,   71,  104,   58,   22,
				  -10,   11,   13,   40,   28,   51,    8,   19,
				   -6,    7,    6,   16,   16,   24,   18,  -10,
				   -7,    2,    8,   19,   25,   10,   21,   -1,
				   -9,  -30,    3,    3,   13,   16,   -4,  -17,
				  -81,  -18,  -30,  -13,  -11,  -16,   -8,  -32
			},
			[BISHOP / 2] = {
				  -40,  -28,  -70,  -64,  -49,  -50,   -9,  -34,
				  -16,    0,  -19,  -20,   18,   28,    4,  -35,
				  -18,   28,   26,   26,   18,   35,   30,   -3,
				  -11,   -7,    5,   22,   19,   16,   -4,  -12,
				   -5,   -3,   -8,   23,   10,   -1,   -9,    7,
				   -7,    5,    6,    5,    3,   15,   11,    3,
				    1,   16,   16,   -4,    4,   18,   20,   -4,
				  -14,    3,  -11,   -6,   -3,    2,  -10,   -5
			},
			[ROOK / 2] = {
				   21,   26,   19,   35,   43,   18,   29,   21,
				   10,    8,   33,   50,   41,   42,   14,   24,
				  -24,    6,    6,   18,    8,   28,   54,    7,
				  -34,  -16,  -10,    4,    5,   19,   -3,  -18,
				  -40,  -32,  -21,   -5,  -12,  -14,    5,  -27,
				  -39,  -21,  -17,   -9,    2,    5,    5,  -21,
				  -21,  -11,  -16,    3,    5,   13,   -2,  -43,
				  -18,   -5,   -4,    9,    6,   -6,  -17,  -11
			},
			[QUEEN / 2] = {
				  -39,   -3,   15,   15,   46,   36,   44,   17,
				  -16,  -42,  -22,  -15,  -29,   29,    7,   30,
				  -10,  -17,  -10,   -4,    4,   37,   44,   49,
				  -22,  -27,  -24,  -30,  -14,   -5,   -7,   -3,
				  -14,  -26,  -20,  -12,  -19,   -5,   -5,   -5,
				  -12,   -6,  -14,    3,    4,   -5,   14,    8,
				  -17,    0,   13,    3,   15,   22,    9,   13,
				    4,   -1,   11,    9,    2,   -4,    0,  -25
			},
			[KING / 2] = {
				  -22,   28,   25,  -48,  -56,  -17,   10,   40,
				  -14,  -11,  -44,   10,  -12,  -11,  -27,  -41,
				  -45,   14,  -19,  -36,  -29,   10,   18,  -41,
				  -50,  -52,  -54,  -77,  -78,  -60,  -52,  -82,
				  -76,  -41,  -67,  -85,  -90,  -68,  -67,  -99,
				  -26,  -21,  -45,  -62,  -58,  -45,  -19,  -37,
				   12,   15,   -2,  -52,  -44,  -15,   11,    6,
				    9,   41,   13,  -31,   18,  -10,   18,   30
			}
		},
		[ENDGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				  164,  164,  157,  119,  130,  131,  168,  176,
				   78,   93,   69,   43,   47,   43,   78,   72,
				   25,   25,    5,    4,   -2,    0,   22,   13,
				    5,    8,   -5,  -18,   -2,   -4,    7,    0,
				   -3,   19,    2,   -3,   -3,    8,    0,  -10,
				    9,    9,    7,    9,    7,    2,    8,   -8,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				  -61,  -21,    0,  -16,  -11,  -23,  -35,  -95,
				  -24,   -7,   -5,   10,   -1,  -28,  -12,  -44,
				  -16,   -1,   14,   15,   -5,    4,   -2,  -28,
				   -6,   12,   20,   22,   28,   22,   12,   -9,
				    2,   11,   21,   40,   21,   31,   16,   -8,
				    5,   16,   14,   31,   25,   10,    5,    7,
				   -5,   -4,   13,   11,   17,    7,   -2,  -14,
				  -16,  -27,    0,    7,    3,   -3,  -16,  -32
			},
			[BISHOP / 2] = {
				   -4,    0,    1,   11,   12,    6,    2,   -6,
				    0,    3,    8,   -2,   10,   -2,    7,  -14,
				    5,    8,    9,    2,   -1,    4,    5,    2,
				   10,   10,   11,    9,   16,   19,    8,   12,
				    9,   12,   12,   27,    8,   16,    2,    5,
				   -7,    9,   14,   19,   26,   16,    5,   -7,
				    3,   -6,    3,    4,   12,   10,   -3,  -14,
				  -12,    0,  -13,   14,   10,    9,    7,   -8
			},
			[ROOK / 2] = {
				   14,   18,   16,   17,   10,   11,   12,    5,
				   16,   16,   15,   15,   -1,    0,    7,    2,
				    2,    3,    2,    5,  -12,  -13,  -10,  -12,
				    1,    3,    5,    0,   -1,    4,   -4,   -7,
				    0,    5,    5,    7,  -10,    2,    1,  -11,
				   -3,    3,   -3,    4,   -1,    2,  -16,  -19,
				    7,    8,   -1,   11,   -1,    3,  -11,   -3,
				   -3,   11,    4,   -2,   -9,  -10,    2,   -4
			},
			[QUEEN / 2] = {
				   -1,   32,   45,   36,   38,   35,    8,   21,
				  -12,   26,   48,   73,   78,   46,   42,    4,
				  -11,   12,   35,   57,   56,   43,    7,    3,
				    9,   24,   28,   49,   69,   47,   53,   27,
				   -7,   28,   27,   58,   35,   46,   35,   24,
				   -8,  -11,   21,   27,   30,   20,   15,    7,
				  -18,  -11,  -17,   -5,    5,  -13,  -34,  -41,
				  -14,  -11,   -5,  -27,    1,   -6,  -19,  -28
			},
			[KING / 2] = {
				  -90,  -39,  -19,   -6,   -8,    7,   -1,  -62,
				   -8,   27,   26,   25,   26,   50,   29,   16,
				   12,   24,   32,   36,   41,   48,   53,   18,
				    2,   26,   34,   44,   44,   42,   37,   11,
				  -16,    5,   27,   39,   40,   35,   20,   -2,
				   -9,   -4,   25,   35,   36,   30,   17,    2,
				  -36,   -2,   13,   12,   13,   14,  -13,  -30,
				  -65,  -43,  -30,   -1,  -22,   -6,  -43,  -52
			}
		}
	},
	.knightMobilityBonus = {
		[MIDDLEGAME] = {  -65,  -12,   -4,   13,   13,   15,   19,   22,   25},
		[ENDGAME] = {  -79,  -27,  -18,   -2,    0,    4,    8,   12,   15}
	},
	.bishopMobilityBonus = {
		[MIDDLEGAME] = { -121,  -79,  -65,  -49,  -25,   -5,    0,    4,    7,   12,   17,   27,   33,   42},
		[ENDGAME] = { -140, -104,  -89,  -73,  -46,  -25,  -20,  -16,  -11,   -6,   -2,    8,   15,   15}
	},
	.rookMobilityBonus = {
		[MIDDLEGAME] = {  -40,  -27,  -18,  -12,  -12,   -8,   -2,    0,    3,    7,   12,   21,   26,   33,   37},
		[ENDGAME] = {  -52,  -35,  -25,  -16,  -16,  -12,   -7,   -4,    0,    5,    8,   19,   24,   30,   33}
	},
	.queenMobilityBonus = {
		[MIDDLEGAME] = { -136, -121, -105,  -72,  -40,  -24,  -15,  -10,   -6,   -2,    1,    6,   11,   14,   19,   40,   45,   50,   55,   60,   65,   70,   70,   70,   70,   70,   70,   70},
		[ENDGAME] = { -158, -136, -119, -101,  -50,  -33,  -25,   -4,   18,   23,   27,   33,   38,   42,   46,   60,   65,   70,   75,   80,   85,   90,   90,   90,   90,   90,   90,   90}
	},
	.virtualMobilityBonus = {
		[MIDDLEGAME] = {   97,   83,   75,   71,   69,   54,   33,   16,    6,   -4,   -9,  -16,  -22,  -29,  -36,  -43,  -50,  -56,  -62,  -65,  -70,  -76,  -78,  -73,  -76,  -70,  -72,  -67},
		[ENDGAME] = {   65,   51,   48,   44,   42,   26,   21,   11,    8,    6,    2,   -6,   -9,  -13,  -17,  -20,  -24,  -29,  -33,  -38,  -43,  -48,  -48,  -50,  -52,  -52,  -54,  -54}
	},
	.bishopPairBonus = {   21,   53},
	.openFileRookBonus = {
		[MIDDLEGAME] = {    5,    3,   -2,    9,    2,    7,   13,   34},
		[ENDGAME] = {   -5,   -7,   -5,    9,    0,   -4,   -7,   -6}
	},
	.passedPawnBonus = {
		[MIDDLEGAME] = {    0,    9,   13,   16,   32,   67,   97,    0},
		[ENDGAME] = {    0,   13,   18,   24,   62,   97,  133,    0}
	},
	.doubledPawnBonus = {
		[MIDDLEGAME] = {    2,    2,    1,   -3,   -2,    4,   -1,   -2},
		[ENDGAME] = {   -8,   -5,   -5,   -5,    0,   -3,   -4,  -12}
	},
	.isolatedPawnBonus = {
		[MIDDLEGAME] = {   -4,   -8,   -4,   -9,   -8,    1,   -4,   -3},
		[ENDGAME] = {    2,   -1,    3,   -6,    0,    5,    5,   -1}
	},
	.tempo = {    10,   10}
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

/******************************************************************/
/** Passed Pawns, Doubled Pawns, Isolated Pawns, Pawn kingshield **/
/******************************************************************/

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

        //Isolated Pawns
        if((whitePawns & bordering_files[column]) == 0)
        {
            eval[MIDDLEGAME] += hce_params.isolatedPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.isolatedPawnBonus[ENDGAME][column];
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

        
        //Isolated Pawns
        if((blackPawns & bordering_files[column]) == 0)
        {
            eval[MIDDLEGAME] -= hce_params.isolatedPawnBonus[MIDDLEGAME][mirroredColumn];
            eval[ENDGAME] -= hce_params.isolatedPawnBonus[ENDGAME][mirroredColumn];
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