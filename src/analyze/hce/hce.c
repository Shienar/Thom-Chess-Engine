#include "analyze/hce/hce.h"

evalParameters hce_params = {
	.genericPieceValues = {
		{   68,  337,  347,  447,  988,    0},
		{   92,  307,  313,  538, 1009,    0}
	},
	//For simplicity, these arrays represent white's view of the board when viewed on a text editor.
	//a1 is bottomleft, h8 is topright.
	.rawPieceTables = {
		//Middlegame
		{
			//Pawn
			{
				    0,    0,    0,    0,    0,    0,    0,    0,
				   89,  121,   71,   98,   74,  109,   24,  -16,
				  -10,   -3,   18,   20,   58,   66,   30,  -14,
				  -19,   10,   -4,   15,   22,   15,   22,  -16,
				  -27,   -4,   -6,   -2,   16,    8,   12,  -17,
				  -28,    6,   -6,  -14,    6,   12,   34,  -17,
				  -37,   -6,  -22,  -21,  -18,   17,   40,  -25,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			//Knight
			{
				 -169, -105,  -43,  -47,   45,  -86,  -42, -112,
				  -60,  -36,   52,   46,   26,   62,   -4,  -12,
				  -36,   49,   42,   55,   78,  118,   66,   30,
				  -12,   15,   15,   44,   30,   59,   11,   21,
				   -9,    8,    6,   19,   16,   27,   20,  -10,
				   -5,    2,   11,   18,   25,   13,   28,    1,
				  -10,  -37,    3,    3,   13,   21,   -5,  -18,
				  -92,  -16,  -38,  -19,  -15,  -21,   -8,  -32
			},
			//Bishop
			{
				  -38,  -22,  -74,  -58,  -41,  -48,   -1,  -29,
				  -14,    5,  -20,  -21,   26,   42,    8,  -39,
				  -21,   32,   32,   32,   24,   41,   34,   -5,
				  -10,   -5,    9,   31,   23,   22,   -1,   -9,
				   -7,    0,   -3,   28,   17,    4,   -6,    8,
				   -8,    8,    6,    9,    7,   21,   15,    4,
				    2,   14,   17,   -8,    7,   20,   23,   -4,
				  -24,   -2,  -15,  -12,   -5,    0,  -19,  -10
			},
			//Rook
			{
				   21,   29,   18,   36,   47,   12,   30,   25,
				   15,   11,   35,   54,   48,   46,   20,   32,
				  -24,    4,    6,   21,    8,   34,   61,   12,
				  -35,  -17,  -10,    8,   11,   29,   -3,  -17,
				  -42,  -34,  -23,   -5,  -11,  -11,   10,  -25,
				  -44,  -25,  -20,  -13,    3,    5,    3,  -25,
				  -24,  -13,  -21,    0,    5,   15,   -3,  -51,
				  -18,   -7,   -4,   10,    7,   -5,  -22,  -13
			},
			//Queen
			{
				  -43,   -5,   22,   14,   55,   46,   52,   28,
				  -19,  -43,  -22,   -6,  -26,   43,   17,   39,
				  -14,  -21,   -7,    0,   12,   47,   46,   53,
				  -26,  -29,  -24,  -26,   -8,    2,   -5,   -5,
				  -14,  -31,  -20,   -9,  -17,   -3,   -4,   -5,
				  -16,   -6,  -17,    3,    2,   -5,   16,   10,
				  -25,   -3,   14,    1,   16,   22,    8,   13,
				    5,   -7,    6,    9,   -4,  -13,   -7,  -31
			},
			//King
			{
				  -49,   11,   17,  -47,  -61,  -27,   13,   35,
				   -7,  -13,  -46,    1,  -19,   -7,  -29,  -26,
				  -40,   15,  -28,  -39,  -29,   12,   22,  -29,
				  -38,  -47,  -49,  -70,  -68,  -55,  -43,  -70,
				  -66,  -33,  -63,  -80,  -86,  -65,  -60,  -89,
				  -19,  -22,  -40,  -62,  -58,  -43,  -17,  -33,
				   11,   15,    1,  -59,  -49,  -15,   10,    3,
				    5,   43,   15,  -38,   20,  -15,   18,   32
			}
		},
		//Endgame
		{
			//Pawn
			{
				    0,    0,    0,    0,    0,    0,    0,    0,
				  170,  168,  164,  127,  138,  130,  170,  182,
				   89,  101,   81,   56,   62,   54,   84,   81,
				   30,   25,    8,    7,    1,    0,   22,   15,
				    8,    6,   -5,  -18,    0,   -4,    5,    0,
				   -2,   16,    2,   -4,   -3,    6,   -3,  -10,
				   12,    8,    7,    8,    7,   -1,    5,   -9,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			//Knight
			{
				  -59,  -28,   -2,  -21,  -16,  -27,  -43, -100,
				  -28,  -11,   -8,    8,   -3,  -31,  -19,  -51,
				  -21,   -4,   10,   13,   -9,    1,   -6,  -35,
				  -11,    6,   17,   17,   24,   18,    7,  -15,
				   -7,    6,   16,   39,   15,   29,   14,  -17,
				    1,   12,   12,   28,   23,    6,   -1,    2,
				  -14,  -13,    9,    8,   15,    2,   -7,  -24,
				  -24,  -35,   -6,    1,   -3,   -9,  -23,  -44
			},
			//Bishop
			{
				  -12,   -9,   -6,    5,    7,    0,   -6,  -13,
				   -3,    0,    6,   -6,    6,   -4,    4,  -20,
				    0,    6,    7,    0,   -3,    1,    5,   -1,
				    7,    6,    9,    6,   13,   18,    5,   10,
				    7,    8,    8,   27,    5,   14,   -3,    2,
				  -14,    5,   12,   17,   26,   15,    2,  -16,
				   -3,  -13,    2,    1,   10,    5,   -8,  -24,
				  -22,   -1,  -22,    8,    4,    4,   -3,  -17
			},
			//Rook
			{
				   13,   14,   15,   17,    8,    8,    8,    1,
				   18,   14,   14,   13,   -4,   -3,    4,   -1,
				    1,    2,    1,    4,  -11,  -16,  -11,  -15,
				   -2,    0,    6,   -1,    1,    4,   -6,   -9,
				   -2,    3,    4,    7,  -12,    1,   -2,  -17,
				   -5,    3,   -4,    3,   -2,    0,  -17,  -23,
				    4,    6,   -2,   13,    0,    2,  -13,   -5,
				   -5,   10,    4,    0,   -7,  -12,    1,   -7
			},
			//Queen
			{
				   -3,   34,   40,   32,   37,   28,    7,   20,
				  -17,   19,   43,   63,   71,   38,   38,   -1,
				  -15,    9,   26,   53,   51,   35,   11,    3,
				    5,   18,   23,   44,   67,   44,   51,   27,
				  -18,   23,   19,   55,   30,   43,   31,   17,
				  -15,  -19,   15,   23,   26,   15,    9,    4,
				  -17,  -15,  -17,   -8,    2,  -12,  -41,  -45,
				  -16,   -8,   -2,  -27,    5,   -7,  -19,  -35
			},
			//King
			{
				  -82,  -36,  -20,   -9,  -11,   14,    4,  -49,
				   -9,   25,   19,   22,   22,   49,   27,   18,
				   13,   20,   24,   27,   31,   46,   54,   18,
				    1,   24,   26,   32,   34,   36,   35,   10,
				  -19,    0,   22,   30,   32,   31,   17,   -5,
				  -10,   -7,   23,   32,   34,   30,   18,    5,
				  -36,   -1,   11,    9,   11,   14,  -11,  -28,
				  -63,  -41,  -30,    2,  -21,   -4,  -40,  -45
			}
		}
	},
	.virtualMobilityBonus = {    -1,    1},
	.mobilityBonus = {
		{   -2,    0,    3,    0,   -3,    1},
		{   -1,    1,    4,   -1,   -2,    2}
	},
	.openFileRookBonus = {
		{    2,    0,   -2,    7,    1,    5,   10,   32},
		{   -5,   -6,   -5,    9,    1,   -4,   -7,   -6}
	},
	.passedPawnBonus = {
		{    0,   -3,   -5,   -8,    2,   -9,   -7,   -3},
		{    0,    0,   -5,   -6,    0,   -7,   -3,   -5}
	},
	.doubledPawnBonus = {
		{    5,    4,    3,    1,    1,    4,    2,    1},
		{   -3,   -1,   -1,   -2,    2,    0,    0,   -7}
	},
	.isolatedPawnBonus = {
		{   -2,   -5,   -2,   -5,   -6,    3,   -2,   -3},
		{    1,    3,    6,   -3,    3,    7,    7,   -1}
	},
};

evalParameters is_param_eg = {
    .genericPieceValues = { [ENDGAME] = { [0 ... (PIECE_COUNT / 2 - 1)] = 1 } },
    .rawPieceTables = { [ENDGAME] = { [0 ... 5] = { [0 ... 63] = 1 } } },
    .openFileRookBonus  = { [ENDGAME] = { [0 ... 7] = 1 } },
    .passedPawnBonus    = { [ENDGAME] = { [0 ... 7] = 1 } },
    .doubledPawnBonus   = { [ENDGAME] = { [0 ... 7] = 1 } },
    .isolatedPawnBonus  = { [ENDGAME] = { [0 ... 7] = 1 } }
};

/*************************/
/** Piece Square Tables **/
/*************************/
int initHCE = 0;

int pieceBonusTable[PHASE_COUNT][PIECE_COUNT][64];

//Other tables
uint64_t bordering_files[8];

void init_HCE_tables(int hasNewValues)
{
    if(initHCE && !hasNewValues) return;
    initHCE = 1;

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

    *middlegameScore += eval[MIDDLEGAME][board->turn] - eval[MIDDLEGAME][FLIP_COLOR(board->turn)];
    *endgameScore += eval[ENDGAME][board->turn] - eval[ENDGAME][FLIP_COLOR(board->turn)];
}

/**********************/
/**  Open Rook File **/
/**********************/

void evaluateSimplePieceDetails(bitboard* board, int* middlegameScore, int* endgameScore)
{
    int eval[PHASE_COUNT] = {0};
    
        
    uint64_t pawnMask = board->pieces[WHITE_PAWN] | board->pieces[BLACK_PAWN];

    uint64_t allyRooks = board->pieces[ROOK | board->turn];
    uint64_t enemyRooks = board->pieces[ROOK | FLIP_COLOR(board->turn)];

    while(allyRooks)
    {
        int column = getColumn(__builtin_ctzll(allyRooks));
        uint64_t pawns_col = pawnMask & board_file[column];

        if(!pawns_col)
        {
            eval[MIDDLEGAME] += hce_params.openFileRookBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.openFileRookBonus[ENDGAME][column];
        }

        allyRooks &= (allyRooks - 1);
    }

    while(enemyRooks)
    {
        int column = getColumn(__builtin_ctzll(enemyRooks));
        uint64_t pawns_col = pawnMask & board_file[column];
    
        if(!pawns_col)
        {
            eval[MIDDLEGAME] -= hce_params.openFileRookBonus[MIDDLEGAME][column];
            eval[ENDGAME] -= hce_params.openFileRookBonus[ENDGAME][column];
        }

        enemyRooks &= (enemyRooks - 1);
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

    uint64_t allyPawns = board->pieces[PAWN | board->turn];
    uint64_t enemyPawns = board->pieces[PAWN | FLIP_COLOR(board->turn)];

    uint64_t mask = allyPawns;
    while(mask)
    {
        int column = getColumn(__builtin_ctzll(mask));
        
        //Passed Pawns
        if((enemyPawns & board_file[column]) == 0)
        {
            eval[MIDDLEGAME] += hce_params.passedPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.passedPawnBonus[ENDGAME][column];
        }

        //Doubled pawns
        if(__builtin_popcountll(allyPawns & board_file[column]) > 1)
        {
            eval[MIDDLEGAME] += hce_params.doubledPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.doubledPawnBonus[ENDGAME][column];
        }

        //Isolated Pawns
        if((allyPawns & bordering_files[column]) == 0)
        {
            eval[MIDDLEGAME] += hce_params.isolatedPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] += hce_params.isolatedPawnBonus[ENDGAME][column];
        }

        mask &= mask - 1;
    }
    
    mask = enemyPawns;
    while(mask)
    {
        int column = getColumn(__builtin_ctzll(mask));

        
        //Passed Pawns
        if((allyPawns & board_file[column]) == 0)
        {
            eval[MIDDLEGAME] -= hce_params.passedPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] -= hce_params.passedPawnBonus[ENDGAME][column];
        }
        
        //Doubled pawns
        if(__builtin_popcountll(enemyPawns & board_file[column]) > 1)
        {
            eval[MIDDLEGAME] -= hce_params.doubledPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] -= hce_params.doubledPawnBonus[ENDGAME][column];
        }

        
        //Isolated Pawns
        if((allyPawns & bordering_files[column]) == 0)
        {
            eval[MIDDLEGAME] -= hce_params.isolatedPawnBonus[MIDDLEGAME][column];
            eval[ENDGAME] -= hce_params.isolatedPawnBonus[ENDGAME][column];
        }

        mask &= mask - 1;
    }

    *middlegameScore += eval[MIDDLEGAME];
    *endgameScore += eval[ENDGAME];
}

/**************/
/** Mobility **/
/**************/
void evaluateMobility(bitboard* board, int* middlegameScore, int* endgameScore)
{
	int eval[2] = {0};

	int allyColor = board->turn;
	int enemyColor = FLIP_COLOR(board->turn);

	uint64_t allyPieces = board->pieces_side[allyColor];
	uint64_t enemyPieces = board->pieces_side[enemyColor];

	//Pawns
	uint64_t allyMask =  WHITE_PAWN_PUSH_MASK(board) | 
						 WHITE_PAWN_DOUBLEPUSH_MASK(board) | 
						 WHITE_PAWN_LEFTATTACKS(board) | 
						 WHITE_PAWN_RIGHTATTACKS(board) | 
						 EN_PASSANT_ATTACKERS_WHITE(singleBitMask(board->enPassantSquare), board);

	uint64_t enemyMask = BLACK_PAWN_PUSH_MASK(board) | 
						 BLACK_PAWN_DOUBLEPUSH_MASK(board) | 
						 BLACK_PAWN_LEFTATTACKS(board) | 
						 BLACK_PAWN_RIGHTATTACKS(board) | 
						 EN_PASSANT_ATTACKERS_BLACK(singleBitMask(board->enPassantSquare), board);

	if(ISBLACK(allyColor))
	{
		uint64_t temp = enemyMask;
		enemyMask = allyMask;
		allyMask = temp;
	}

	int mobilityScore = __builtin_popcountll(allyMask) - __builtin_popcountll(enemyMask);
	eval[MIDDLEGAME] += hce_params.mobilityBonus[MIDDLEGAME][PAWN / 2] * mobilityScore;
	eval[ENDGAME] += hce_params.mobilityBonus[ENDGAME][PAWN / 2] * mobilityScore;

	//Knights
	uint64_t pieces = board->pieces[KNIGHT | allyColor];
	mobilityScore = 0;
	while(pieces)
	{
		mobilityScore += __builtin_popcountll(knightMoves(allyPieces, __builtin_ctzll(pieces)));
		pieces &= pieces - 1;
	}
	pieces = board->pieces[KNIGHT | enemyColor];
	while(pieces)
	{
		mobilityScore -= __builtin_popcountll(knightMoves(enemyPieces, __builtin_ctzll(pieces)));
		pieces &= pieces - 1;
	}

	eval[MIDDLEGAME] += hce_params.mobilityBonus[MIDDLEGAME][KNIGHT / 2] * mobilityScore;
	eval[ENDGAME] += hce_params.mobilityBonus[ENDGAME][KNIGHT / 2] * mobilityScore;

	//Bishops
	pieces = board->pieces[BISHOP | allyColor];
	mobilityScore = 0;
	while(pieces)
	{
		mobilityScore += __builtin_popcountll(bishopMoves(allyPieces, enemyPieces, __builtin_ctzll(pieces)));
		pieces &= pieces - 1;
	}
	pieces = board->pieces[BISHOP | enemyColor];
	while(pieces)
	{
		mobilityScore -= __builtin_popcountll(bishopMoves(enemyPieces, allyPieces, __builtin_ctzll(pieces)));
		pieces &= pieces - 1;
	}

	eval[MIDDLEGAME] += hce_params.mobilityBonus[MIDDLEGAME][BISHOP / 2] * mobilityScore;
	eval[ENDGAME] += hce_params.mobilityBonus[ENDGAME][BISHOP / 2] * mobilityScore;

	//Rook
	pieces = board->pieces[ROOK | allyColor];
	mobilityScore = 0;
	while(pieces)
	{
		mobilityScore += __builtin_popcountll(rookMoves(allyPieces, enemyPieces, __builtin_ctzll(pieces)));
		pieces &= pieces - 1;
	}
	pieces = board->pieces[ROOK | enemyColor];
	while(pieces)
	{
		mobilityScore -= __builtin_popcountll(rookMoves(enemyPieces, allyPieces, __builtin_ctzll(pieces)));
		pieces &= pieces - 1;
	}

	eval[MIDDLEGAME] += hce_params.mobilityBonus[MIDDLEGAME][ROOK / 2] * mobilityScore;
	eval[ENDGAME] += hce_params.mobilityBonus[ENDGAME][ROOK / 2] * mobilityScore;

	//Queen
	pieces = board->pieces[QUEEN | allyColor];
	mobilityScore = 0;
	while(pieces)
	{
		mobilityScore += __builtin_popcountll(rookMoves(allyPieces, enemyPieces, __builtin_ctzll(pieces)));
		pieces &= pieces - 1;
	}
	pieces = board->pieces[QUEEN | enemyColor];
	while(pieces)
	{
		mobilityScore -= __builtin_popcountll(rookMoves(enemyPieces, allyPieces, __builtin_ctzll(pieces)));
		pieces &= pieces - 1;
	}

	eval[MIDDLEGAME] += hce_params.mobilityBonus[MIDDLEGAME][QUEEN / 2] * mobilityScore;
	eval[ENDGAME] += hce_params.mobilityBonus[ENDGAME][QUEEN / 2] * mobilityScore;

	//Kings
	pieces = board->pieces[KING | allyColor];
	mobilityScore = 0;
	while(pieces)
	{
		mobilityScore += __builtin_popcountll(kingAttacks[__builtin_ctzll(pieces)] & (~allyPieces));
		pieces &= pieces - 1;
	}
	pieces = board->pieces[KING | enemyColor];
	while(pieces)
	{
		mobilityScore -= __builtin_popcountll(kingAttacks[__builtin_ctzll(pieces)] & (~enemyPieces));
		pieces &= pieces - 1;
	}

	eval[MIDDLEGAME] += hce_params.mobilityBonus[MIDDLEGAME][KING / 2] * mobilityScore;
	eval[ENDGAME] += hce_params.mobilityBonus[ENDGAME][KING / 2] * mobilityScore;

	*middlegameScore += eval[MIDDLEGAME];
	*endgameScore += eval[ENDGAME];
}

/*****************/
/** King Safety **/
/*****************/
void evaluateKingSafety(bitboard* board, int* middlegameScore, int* endgameScore)
{
	int eval[2] = {0};

	int allyColor = board->turn;
	int enemyColor = FLIP_COLOR(board->turn);

	uint64_t allyPieces = board->pieces_side[allyColor];
	uint64_t enemyPieces = board->pieces_side[enemyColor];

	uint64_t pieces = board->pieces[KING | allyColor];
	int virtualMobilityScore = 0;
	while(pieces)
	{
		virtualMobilityScore += __builtin_popcountll(queenMoves(allyPieces, enemyPieces, __builtin_ctzll(pieces)));
		pieces &= pieces - 1;
	}
	pieces = board->pieces[KING | enemyColor];
	while(pieces)
	{
		virtualMobilityScore -= __builtin_popcountll(queenMoves(enemyPieces, allyPieces, __builtin_ctzll(pieces)));
		pieces &= pieces - 1;
	}

	eval[MIDDLEGAME] += hce_params.virtualMobilityBonus[MIDDLEGAME] * virtualMobilityScore;
	eval[ENDGAME] += hce_params.virtualMobilityBonus[ENDGAME] * virtualMobilityScore;
	
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

    return evaluatePhasedScore(board, midgame_eval, endgame_eval);
}