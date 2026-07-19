#include "analyze/hce/hce.h"

evalParameters hce_params = {
	.genericPieceValues = {
		[MIDDLEGAME] = {   64,  335,  345,  439,  975,    0},
		[ENDGAME] = {   91,  309,  315,  542, 1023,    0}
	},
	//For simplicity, these arrays represent white's view of the board when viewed on a text editor.
	//a1 is bottomleft, h8 is topright.
	.rawPieceTables = {
		[MIDDLEGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				   85,  117,   69,   96,   74,  105,   22,  -18,
				  -12,   -4,   17,   18,   56,   65,   30,  -14,
				  -19,    9,   -5,   14,   23,   15,   22,  -16,
				  -27,   -4,   -6,   -3,   16,    9,   12,  -16,
				  -29,    6,   -7,  -13,    6,   14,   33,  -17,
				  -37,   -7,  -21,  -20,  -17,   15,   39,  -24,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				 -170, -107,  -45,  -47,   41,  -85,  -47, -113,
				  -58,  -36,   48,   46,   25,   61,   -7,  -12,
				  -35,   46,   41,   53,   76,  114,   63,   27,
				  -12,   14,   13,   43,   29,   56,   10,   20,
				   -8,    7,    5,   18,   14,   26,   19,  -11,
				   -6,    2,    9,   18,   25,   12,   26,    1,
				   -9,  -34,    4,    2,   13,   20,   -4,  -17,
				  -88,  -17,  -34,  -16,  -15,  -18,   -8,  -31
			},
			[BISHOP / 2] = {
				  -39,  -25,  -73,  -61,  -44,  -50,   -4,  -32,
				  -15,    3,  -21,  -22,   24,   37,    6,  -38,
				  -21,   30,   29,   30,   21,   40,   32,   -5,
				  -12,   -6,    7,   27,   22,   20,   -2,  -11,
				   -8,   -1,   -4,   26,   14,    2,   -8,    8,
				   -9,    6,    5,    8,    5,   19,   14,    4,
				    1,   13,   16,   -9,    6,   19,   21,   -5,
				  -22,   -2,  -15,  -11,   -5,    0,  -16,   -8
			},
			[ROOK / 2] = {
				   20,   28,   17,   35,   46,   14,   30,   21,
				   11,    8,   32,   53,   44,   44,   17,   27,
				  -25,    3,    4,   19,    7,   33,   60,   10,
				  -35,  -17,  -11,    6,    8,   26,   -2,  -18,
				  -42,  -34,  -23,   -5,  -12,  -12,    9,  -25,
				  -42,  -23,  -20,  -12,    3,    6,    4,  -23,
				  -21,  -11,  -20,    1,    4,   14,   -2,  -48,
				  -18,   -5,   -5,    9,    7,   -6,  -20,  -12
			},
			[QUEEN / 2] = {
				  -43,   -6,   20,   16,   53,   45,   53,   26,
				  -18,  -42,  -22,   -7,  -25,   40,   17,   38,
				  -13,  -21,   -8,    0,   10,   47,   46,   54,
				  -25,  -28,  -23,  -28,   -8,    0,   -4,   -4,
				  -13,  -28,  -19,   -8,  -17,   -2,   -3,   -3,
				  -13,   -6,  -16,    4,    3,   -4,   17,   11,
				  -22,   -1,   13,    1,   15,   23,   10,   14,
				    6,   -5,    9,    8,    0,   -8,   -2,  -27
			},
			[KING / 2] = {
				  -42,   14,   22,  -49,  -58,  -25,   15,   40,
				  -13,  -14,  -47,    6,  -18,   -6,  -27,  -28,
				  -45,   15,  -29,  -40,  -28,   15,   20,  -31,
				  -40,  -49,  -51,  -74,  -71,  -57,  -47,  -75,
				  -67,  -35,  -64,  -82,  -87,  -66,  -63,  -93,
				  -20,  -21,  -41,  -61,  -58,  -44,  -17,  -34,
				   13,   16,    1,  -56,  -47,  -15,   10,    5,
				    8,   44,   15,  -37,   21,  -12,   17,   33
			}
		},
		[ENDGAME] = {
			[PAWN / 2] = {
				    0,    0,    0,    0,    0,    0,    0,    0,
				  169,  168,  164,  125,  137,  132,  171,  181,
				   88,  101,   81,   56,   61,   53,   84,   80,
				   28,   24,    8,    7,    1,    0,   22,   14,
				    6,    6,   -5,  -18,    0,   -4,    5,    0,
				   -3,   17,    2,   -4,   -3,    8,   -3,  -11,
				   11,    7,    8,    8,    7,    0,    6,  -10,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			[KNIGHT / 2] = {
				  -61,  -26,   -1,  -20,  -14,  -27,  -40, -100,
				  -27,  -10,   -6,    9,   -2,  -31,  -17,  -50,
				  -19,   -2,   12,   15,   -8,    2,   -5,  -33,
				   -9,    9,   19,   19,   26,   20,    9,  -13,
				   -3,    8,   18,   40,   17,   30,   16,  -14,
				    3,   14,   13,   30,   25,    8,    1,    4,
				  -11,   -9,   12,   11,   17,    4,   -5,  -20,
				  -21,  -33,   -2,    4,    0,   -6,  -20,  -40
			},
			[BISHOP / 2] = {
				  -10,   -7,   -4,    6,    8,    1,   -5,  -12,
				   -3,    0,    6,   -5,    7,   -5,    4,  -19,
				    1,    5,    7,   -1,   -3,    2,    4,    0,
				    7,    7,    9,    7,   14,   19,    6,    9,
				    7,    9,    9,   27,    6,   15,   -2,    3,
				  -12,    6,   12,   18,   26,   16,    2,  -14,
				   -1,  -11,    3,    2,   11,    7,   -7,  -22,
				  -19,    1,  -19,   10,    6,    6,   -1,  -15
			},
			[ROOK / 2] = {
				   14,   16,   15,   18,    9,    9,    9,    3,
				   17,   14,   14,   14,   -3,   -3,    3,   -1,
				    1,    2,    1,    5,  -12,  -16,  -13,  -16,
				   -1,    0,    6,   -1,   -1,    3,   -7,  -10,
				   -1,    3,    4,    8,  -12,    1,   -2,  -16,
				   -4,    3,   -4,    4,   -1,    1,  -18,  -23,
				    6,    8,   -2,   13,    0,    2,  -13,   -5,
				   -5,   11,    4,    0,   -7,  -11,    1,   -7
			},
			[QUEEN / 2] = {
				   -2,   35,   42,   33,   37,   30,    6,   20,
				  -16,   20,   44,   65,   74,   40,   38,    0,
				  -13,    9,   28,   54,   51,   37,   10,    3,
				    6,   19,   24,   44,   68,   44,   51,   27,
				  -15,   24,   21,   56,   31,   45,   31,   18,
				  -13,  -16,   17,   25,   28,   16,   11,    5,
				  -16,  -12,  -16,   -7,    3,  -11,  -40,  -46,
				  -13,   -5,    0,  -25,    6,   -4,  -18,  -33
			},
			[KING / 2] = {
				  -84,  -37,  -21,   -8,  -11,   12,    2,  -54,
				   -8,   25,   20,   22,   23,   50,   28,   18,
				   13,   20,   26,   30,   34,   46,   54,   18,
				    1,   24,   28,   36,   37,   38,   35,   11,
				  -19,    1,   23,   33,   34,   32,   17,   -3,
				  -10,   -6,   23,   33,   34,   29,   18,    5,
				  -36,   -1,   11,    9,   11,   14,  -12,  -28,
				  -63,  -42,  -31,    3,  -22,   -3,  -41,  -46
			}
		}
	},
	.knightMobilityBonus = {
		[MIDDLEGAME] = {  -73,  -17,   -8,   11,   14,   18,   22,   27,   31},
		[ENDGAME] = {  -89,  -34,  -25,   -6,   -2,    2,    7,   11,   15}
	},
	.bishopMobilityBonus = {
		[MIDDLEGAME] = { -128,  -83,  -68,  -50,  -23,    1,    5,   10,   14,   19,   24,   34,   40,   47},
		[ENDGAME] = { -148, -110,  -95,  -77,  -49,  -25,  -20,  -16,  -11,   -5,   -1,    9,   16,   18}
	},
	.rookMobilityBonus = {
		[MIDDLEGAME] = {  -47,  -29,  -20,  -13,  -10,   -6,    0,    3,    7,   11,   17,   26,   31,   37,   41},
		[ENDGAME] = {  -57,  -38,  -28,  -18,  -18,  -14,   -9,   -5,    0,    5,    9,   20,   25,   31,   35}
	},
	.queenMobilityBonus = {
		[MIDDLEGAME] = { -146, -128, -110,  -74,  -37,  -19,  -10,   -5,    0,    4,    8,   13,   18,   22,   27,   40,   45,   50,   55,   60,   65,   70,   70,   70,   70,   70,   70,   70},
		[ENDGAME] = { -165, -144, -126, -108,  -52,  -33,  -24,   -1,   22,   27,   31,   37,   42,   47,   51,   60,   65,   70,   75,   80,   85,   90,   90,   90,   90,   90,   90,   90}
	},
	.virtualMobilityBonus = {
		[MIDDLEGAME] = {   95,   84,   77,   73,   69,   52,   31,   13,    3,   -7,  -12,  -18,  -23,  -29,  -35,  -41,  -47,  -53,  -58,  -62,  -67,  -72,  -73,  -71,  -72,  -69,  -70,  -68},
		[ENDGAME] = {   70,   58,   54,   50,   46,   28,   23,   13,    9,    5,    1,   -8,  -12,  -17,  -21,  -25,  -30,  -35,  -39,  -44,  -49,  -54,  -54,  -55,  -56,  -56,  -57,  -57}
	},
	.bishopPairBonus = {   23,   53},
	.openFileRookBonus = {
		[MIDDLEGAME] = {    2,    0,   -2,    8,    1,    6,   11,   36},
		[ENDGAME] = {   -6,   -7,   -6,    9,    0,   -5,   -8,   -8}
	},
	.passedPawnBonus = {
		[MIDDLEGAME] = {    7,   3,   2,   0,    8,   -3,    1,    4},
		[ENDGAME] = {    0,    3,     4,    4,    8,    2,    7,    4}
	},
	.doubledPawnBonus = {
		[MIDDLEGAME] = {    4,    4,    3,   -1,    0,    4,    1,    0},
		[ENDGAME] = {   -4,   -2,   -3,   -4,    1,   -1,   -1,   -9}
	},
	.isolatedPawnBonus = {
		[MIDDLEGAME] = {   -4,   -6,   -3,   -7,   -7,    2,   -3,   -4},
		[ENDGAME] = {    0,    1,    4,   -5,    2,    6,    7,   -2}
	},
	.tempo = {   10,   10}
};


evalParameters is_param_eg = {
    .genericPieceValues[ENDGAME] = { [0 ... PIECE_TYPE_COUNT - 1] = 1 },
    .rawPieceTables[ENDGAME] = { [0 ... 5] = { [0 ... 63] = 1 } },
    .virtualMobilityBonus[ENDGAME] = { [0 ... 27] = 1 },
    .knightMobilityBonus[ENDGAME] = { [0 ... 8] = 1 },
    .bishopMobilityBonus[ENDGAME] = { [0 ... 13] = 1 },
    .rookMobilityBonus[ENDGAME] = { [0 ... 14] = 1 },
    .queenMobilityBonus[ENDGAME] = { [0 ... 27] = 1 },
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
uint64_t bordering_files[8];

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
                pieceBonusTable[gamephase][piece + 1][square] = hce_params.rawPieceTables[gamephase][piece / 2][square] + hce_params.genericPieceValues[gamephase][piece / 2];
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
    uint64_t pawnMask = board->pieces[WHITE_PAWN] | board->pieces[BLACK_PAWN];

    uint64_t whiteRooks = board->pieces[WHITE_ROOK];
    uint64_t blackRooks = board->pieces[BLACK_ROOK];

    while(whiteRooks)
    {
        int column = getColumn(__builtin_ctzll(whiteRooks));
        uint64_t pawns_col = pawnMask & board_file[column];

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
        uint64_t pawns_col = pawnMask & board_file[column];
    
        if(!pawns_col)
        {
            eval[MIDDLEGAME] -= hce_params.openFileRookBonus[MIDDLEGAME][column];
            eval[ENDGAME] -= hce_params.openFileRookBonus[ENDGAME][column];
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
        int column = getColumn(__builtin_ctzll(mask));
        
        //Passed Pawns
        if((blackPawns & board_file[column]) == 0)
        {
            eval[MIDDLEGAME] += hce_params.passedPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.passedPawnBonus[ENDGAME][column];
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
        int column = getColumn(__builtin_ctzll(mask));
        
        //Passed Pawns
        if((whitePawns & board_file[column]) == 0)
        {
            eval[MIDDLEGAME] -= hce_params.passedPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] -= hce_params.passedPawnBonus[ENDGAME][column];
        }
        
        //Doubled pawns
        if(__builtin_popcountll(blackPawns & board_file[column]) > 1)
        {
            eval[MIDDLEGAME] -= hce_params.doubledPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] -= hce_params.doubledPawnBonus[ENDGAME][column];
        }

        
        //Isolated Pawns
        if((whitePawns & bordering_files[column]) == 0)
        {
            eval[MIDDLEGAME] -= hce_params.isolatedPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] -= hce_params.isolatedPawnBonus[ENDGAME][column];
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