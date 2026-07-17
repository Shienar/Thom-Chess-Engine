#include "analyze/hce/hce.h"

evalParameters hce_params = {
	.genericPieceValues = {
		{   74,  336,  349,  455,  997,    0},
		{   94,  304,  312,  533,  993,    0}
	},
	//For simplicity, these arrays represent white's view of the board when viewed on a text editor.
	//a1 is bottomleft, h8 is topright.
	.rawPieceTables = {
		//Middlegame
		{
			//Pawn
			{
				    0,    0,    0,    0,    0,    0,    0,    0,
				   95,  128,   78,  104,   76,  117,   31,  -13,
				   -5,    6,   31,   35,   68,   77,   39,  -10,
				  -14,   13,    1,   24,   26,   18,   22,  -15,
				  -28,   -4,   -4,    7,   19,    6,    9,  -21,
				  -29,    3,   -7,  -17,    4,    8,   34,  -17,
				  -35,   -3,  -21,  -23,  -19,   20,   43,  -24,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			//Knight
			{
				 -163, -101,  -37,  -46,   51,  -85,  -34, -106,
				  -61,  -34,   59,   47,   29,   66,    1,  -10,
				  -37,   54,   46,   61,   82,  125,   72,   36,
				  -12,   17,   17,   48,   34,   65,   12,   24,
				  -15,    7,    6,   19,   17,   26,   21,  -15,
				  -19,   -5,   12,   17,   24,   14,   26,   -8,
				  -27,  -50,   -5,   -1,    8,   18,  -11,  -26,
				 -103,  -21,  -50,  -28,  -20,  -28,  -20,  -36
			},
			//Bishop
			{
				  -34,  -16,  -73,  -53,  -35,  -44,    2,  -22,
				  -12,   10,  -17,  -18,   32,   51,   15,  -34,
				  -18,   39,   39,   37,   29,   48,   39,    2,
				   -7,   -2,   13,   36,   28,   27,    2,   -5,
				   -8,    2,    1,   32,   21,    8,   -4,    8,
				   -7,   10,    7,   14,   11,   22,   18,    4,
				    2,   12,   21,   -8,    6,   22,   25,    2,
				  -33,   -2,  -19,  -19,  -12,   -8,  -24,  -17
			},
			//Rook
			{
				   25,   33,   22,   40,   51,   13,   32,   31,
				   23,   17,   41,   58,   54,   55,   26,   41,
				  -16,   10,   13,   27,   14,   42,   68,   19,
				  -30,  -14,   -4,   14,   17,   35,   -2,  -11,
				  -41,  -34,  -20,   -2,   -8,  -11,   10,  -24,
				  -48,  -27,  -20,  -15,    2,    4,    1,  -28,
				  -39,  -17,  -25,   -2,    0,   12,   -5,  -59,
				  -20,  -11,   -2,   13,   12,   -3,  -27,  -19
			},
			//Queen
			{
				  -37,    0,   28,   16,   60,   50,   54,   36,
				  -17,  -39,  -16,    1,  -20,   53,   25,   46,
				  -12,  -17,    0,    8,   19,   56,   51,   59,
				  -25,  -27,  -19,  -20,   -2,   11,   -2,    0,
				  -17,  -29,  -17,   -6,  -12,   -1,    0,   -5,
				  -19,   -4,  -17,    3,    0,   -3,   16,    8,
				  -30,   -9,   14,    2,   15,   18,    1,   10,
				   -2,  -16,   -2,    8,  -14,  -26,  -21,  -39
			},
			//King
			{
				  -52,   13,   16,  -39,  -60,  -29,   11,   28,
				    3,  -13,  -44,   -4,  -20,   -9,  -34,  -25,
				  -31,   14,  -26,  -37,  -30,    8,   22,  -26,
				  -32,  -45,  -45,  -64,  -63,  -51,  -39,  -62,
				  -63,  -31,  -60,  -75,  -81,  -64,  -58,  -82,
				  -18,  -26,  -41,  -62,  -58,  -43,  -18,  -33,
				    8,   12,   -3,  -64,  -53,  -17,    8,    3,
				    2,   43,   17,  -51,   13,  -26,   22,   27
			}
		},
		//Endgame
		{
			//Pawn
			{
				    0,    0,    0,    0,    0,    0,    0,    0,
				  172,  170,  165,  130,  141,  130,  169,  182,
				   90,  101,   84,   62,   65,   57,   86,   83,
				   32,   27,    9,   12,    3,    1,   22,   16,
				    9,    6,   -5,  -15,    1,   -6,    4,    0,
				   -1,   15,    1,   -6,   -3,    3,   -3,  -10,
				   14,   10,    8,    6,    7,   -1,    6,   -8,
				    0,    0,    0,    0,    0,    0,    0,    0
			},
			//Knight
			{
				  -57,  -31,   -3,  -23,  -18,  -27,  -47,  -99,
				  -29,  -12,   -9,    7,   -4,  -31,  -20,  -52,
				  -23,   -5,   10,   14,   -9,    1,   -6,  -37,
				  -14,    4,   16,   17,   24,   18,    5,  -16,
				  -11,    4,   14,   37,   13,   27,   13,  -22,
				   -9,    7,   10,   25,   21,    5,   -4,   -7,
				  -24,  -20,    4,    4,   11,   -2,  -13,  -34,
				  -30,  -41,  -13,   -5,   -9,  -15,  -36,  -51
			},
			//Bishop
			{
				  -13,  -12,   -7,    2,    5,   -1,   -7,  -14,
				   -3,    0,    6,   -8,    7,   -3,    5,  -20,
				   -1,    6,    8,    1,   -3,    2,    6,    0,
				    7,    5,   10,    6,   14,   20,    4,   10,
				    6,    7,    8,   29,    5,   15,   -5,    1,
				  -16,    4,   11,   18,   28,   15,    2,  -18,
				   -5,  -15,    2,    0,    9,    4,   -8,  -25,
				  -28,   -4,  -26,    3,   -1,   -2,   -6,  -22
			},
			//Rook
			{
				   12,   12,   13,   15,    7,    8,    7,   -1,
				   20,   14,   14,   12,   -5,   -1,    6,    1,
				    2,    3,    2,    5,   -8,  -13,   -8,  -13,
				    0,   -1,    7,   -1,    3,    7,   -6,   -7,
				   -3,    1,    4,    7,  -13,   -1,   -3,  -18,
				   -9,    1,   -6,    1,   -4,   -2,  -18,  -25,
				   -4,    1,   -6,   11,   -4,   -1,  -15,   -9,
				   -7,    6,    5,    1,   -5,  -13,   -1,  -11
			},
			//Queen
			{
				   -4,   31,   36,   30,   35,   27,    9,   21,
				  -18,   18,   41,   60,   68,   38,   38,   -1,
				  -18,    7,   24,   54,   51,   37,   13,    4,
				    3,   17,   22,   44,   68,   45,   51,   28,
				  -23,   22,   17,   54,   29,   42,   32,   16,
				  -18,  -21,   13,   19,   22,   15,    7,    2,
				  -22,  -22,  -21,  -12,   -4,  -18,  -46,  -45,
				  -24,  -18,  -11,  -34,   -3,  -19,  -26,  -40
			},
			//King
			{
				  -81,  -36,  -20,  -12,  -12,   15,    5,  -42,
				  -10,   24,   16,   21,   20,   47,   26,   18,
				   12,   19,   21,   23,   28,   45,   54,   17,
				    0,   23,   24,   29,   31,   34,   33,    9,
				  -20,   -2,   20,   27,   29,   29,   15,   -8,
				  -11,   -9,   20,   30,   32,   28,   17,    4,
				  -37,   -2,   10,    7,    9,   13,  -10,  -28,
				  -63,  -39,  -26,   -4,  -24,   -8,  -36,  -46
			}
		}
	},
	.openFileRookBonus = {
		{    5,    2,    1,    9,    5,   10,   11,   30},
		{   -2,   -5,   -3,   11,    3,    0,   -5,   -2}
	},
	.passedPawnBonus = {
		{    0,   -2,   -4,   -6,    5,   -7,   -4,   -2},
		{    0,    2,   -3,   -4,    2,   -5,    0,   -3}
	},
	.doubledPawnBonus = {
		{    6,    7,    4,    5,    5,    5,    4,    0},
		{   -1,    2,    1,    1,    4,    2,    2,   -6}
	},
	.isolatedPawnBonus = {
		{   -1,   -1,    0,    0,   -2,    4,    1,   -2},
		{    2,    5,    7,    0,    5,    8,    9,    0}
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
    evaluatePieceSquareTables(board, &midgame_eval, &endgame_eval);

    evaluateSimplePieceDetails(board, &midgame_eval, &endgame_eval);
    evaluateSimplePieceDetails(board, &midgame_eval, &endgame_eval);

    evaluatePawnDetails(board, &midgame_eval, &endgame_eval);
    evaluatePawnDetails(board, &midgame_eval, &endgame_eval);

    return evaluatePhasedScore(board, midgame_eval, endgame_eval);
}