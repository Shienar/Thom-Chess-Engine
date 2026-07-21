#include "tuner.h"
#include "analyze/engine.h"
#include <math.h>
#include <float.h>


evalParameters_fp currentParameters;
tuningEntry* tuner_entries = NULL;
int tuner_entry_count = 0;

//Print out all of the parameters in C array format.
//Just look at the declaration at the top of hce.c if this doesn't make sense.
void print_parameters(FILE* output, evalParameters_fp* currentParameters)
{
    evalParameters params = {0};
    for(int i = 0; i < PARAMETER_COUNT; i++)
        params.parameters[i] = round(currentParameters->parameters[i]);
    
    const char* phase_names[2] = {"MIDDLEGAME", "ENDGAME"};
    const char* piece_names[6] = {"PAWN", "KNIGHT", "BISHOP", "ROOK", "QUEEN", "KING"};

    rewind(output);
    fprintf(output, "evalParameters hce_params = {\n");
    
    fprintf(output, "\t.genericPieceValues = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int piece = 0; piece < PIECE_TYPE_COUNT; piece++)
            fprintf(output, "%5d%s", params.genericPieceValues[phase][piece], (piece == (PIECE_TYPE_COUNT) - 1) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");

    fprintf(output, "\t.rawPieceTables = {\n");


    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {\n", phase_names[phase]);
        
        for(int pc = 0; pc < 6; pc++)
        {
            fprintf(output, "\t\t\t[%s / 2] = {\n", piece_names[pc]);
            for(int row = 0; row < ROW_COUNT; row++) 
            {
                fprintf(output, "\t\t\t\t");
                for(int col = 0; col < COLUMN_COUNT; col++) 
                {
                    int idx = row * COLUMN_COUNT + col;
                    fprintf(output, "%5d%s", params.rawPieceTables[phase][pc][idx], (idx == 63) ? "" : ",");
                }
                fprintf(output, "\n");
            }
            fprintf(output, "\t\t\t}%s\n", (pc == 5) ? "" : ",");
        }
        fprintf(output, "\t\t}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");
    
    fprintf(output, "\t.knightMobilityBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int i = 0; i < 9; i++)
            fprintf(output, "%5d%s", params.knightMobilityBonus[phase][i], (i == 8) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");
    
    fprintf(output, "\t.bishopMobilityBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int i = 0; i < 14; i++)
            fprintf(output, "%5d%s", params.bishopMobilityBonus[phase][i], (i == 13) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");
    
    fprintf(output, "\t.rookMobilityBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int i = 0; i < 15; i++)
            fprintf(output, "%5d%s", params.rookMobilityBonus[phase][i], (i == 14) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");
    
    fprintf(output, "\t.queenMobilityBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int i = 0; i < 28; i++)
            fprintf(output, "%5d%s", params.queenMobilityBonus[phase][i], (i == 27) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");

    fprintf(output, "\t.virtualMobilityBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int i = 0; i < 28; i++)
            fprintf(output, "%5d%s", params.virtualMobilityBonus[phase][i], (i == 27) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");

    fprintf(output, "\t.bishopPairBonus = {%5d,%5d},\n", params.bishopPairBonus[MIDDLEGAME], params.bishopPairBonus[ENDGAME]);

    fprintf(output, "\t.openFileRookBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int column = 0; column < COLUMN_COUNT; column++)
            fprintf(output, "%5d%s", params.openFileRookBonus[phase][column], (column == 7) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");

    fprintf(output, "\t.passedPawnBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int column = 0; column < COLUMN_COUNT; column++)
            fprintf(output, "%5d%s", params.passedPawnBonus[phase][column], (column == 7) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");

    fprintf(output, "\t.doubledPawnBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int column = 0; column < COLUMN_COUNT; column++)
            fprintf(output, "%5d%s", params.doubledPawnBonus[phase][column], (column == 7) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");

    fprintf(output, "\t.isolatedPawnBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int column = 0; column < COLUMN_COUNT; column++)
            fprintf(output, "%5d%s", params.isolatedPawnBonus[phase][column], (column == 7) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");
    
    fprintf(output, "\t.connectedPawnBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int column = 0; column < COLUMN_COUNT; column++)
            fprintf(output, "%5d%s", params.connectedPawnBonus[phase][column], (column == 7) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");
    
    fprintf(output, "\t.backwardPawnBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int column = 0; column < COLUMN_COUNT; column++)
            fprintf(output, "%5d%s", params.backwardPawnBonus[phase][column], (column == 7) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");

    fprintf(output, "\t.tempo = {%5d,%5d}\n", params.tempo[MIDDLEGAME], params.tempo[ENDGAME]);

    fprintf(output, "};\n");
    fflush(output);
}

void initPawnCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
	uint64_t mask = board->pieces[WHITE_PAWN];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
        int column = getColumn(sq);
		int row = getRow(sq);

		//piece/square value
        coefficients->genericPieceValues[MIDDLEGAME][PAWN / 2]++;
        coefficients->rawPieceTables[MIDDLEGAME][PAWN / 2][FLIP_SQUARE(sq)]++;

        coefficients->genericPieceValues[ENDGAME][PAWN / 2]++;
        coefficients->rawPieceTables[ENDGAME][PAWN / 2][FLIP_SQUARE(sq)]++;

        *mgScore += params.genericPieceValues[MIDDLEGAME][PAWN / 2];
        *mgScore += params.rawPieceTables[MIDDLEGAME][PAWN / 2][FLIP_SQUARE(sq)];
        
        *egScore += params.genericPieceValues[ENDGAME][PAWN / 2];
        *egScore += params.rawPieceTables[ENDGAME][PAWN / 2][FLIP_SQUARE(sq)];

		//Passed Pawns
        if((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
        {
            coefficients->passedPawnBonus[MIDDLEGAME][row]++;
            coefficients->passedPawnBonus[ENDGAME][row]++;

            *mgScore += hce_params.passedPawnBonus[MIDDLEGAME][row];
            *egScore += hce_params.passedPawnBonus[ENDGAME][row];
        }

        //Doubled pawns
        if(__builtin_popcountll(board->pieces[WHITE_PAWN] & board_file[column]) > 1)
        {
            coefficients->doubledPawnBonus[MIDDLEGAME][column]++;
            coefficients->doubledPawnBonus[ENDGAME][column]++;

            *mgScore += hce_params.doubledPawnBonus[MIDDLEGAME][column];
            *egScore += hce_params.doubledPawnBonus[ENDGAME][column];
        }

		uint64_t borderingMask = board->pieces[WHITE_PAWN] & bordering_files[column];
        //Isolated Pawns
        if(!borderingMask)
        {
            coefficients->isolatedPawnBonus[MIDDLEGAME][column]++;
            coefficients->isolatedPawnBonus[ENDGAME][column]++;

            *mgScore += hce_params.isolatedPawnBonus[MIDDLEGAME][column];
            *egScore += hce_params.isolatedPawnBonus[ENDGAME][column];
        }
		//Backward pawns
		else if(row > 1 && (borderingMask & (sq - 1)) == 0 && borderingMask & board_rank[row + 1])
		{
            coefficients->backwardPawnBonus[MIDDLEGAME][column]++;
            coefficients->backwardPawnBonus[ENDGAME][column]++;

			*mgScore += hce_params.backwardPawnBonus[MIDDLEGAME][column];
			*egScore += hce_params.backwardPawnBonus[ENDGAME][column];
		}
		//connected pawns
		else if(borderingMask & board_rank[row - 1])
		{
            coefficients->connectedPawnBonus[MIDDLEGAME][column]++;
            coefficients->connectedPawnBonus[ENDGAME][column]++;

			*mgScore += hce_params.connectedPawnBonus[MIDDLEGAME][column];
			*egScore += hce_params.connectedPawnBonus[ENDGAME][column];
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
        coefficients->genericPieceValues[MIDDLEGAME][PAWN / 2]--;
        coefficients->rawPieceTables[MIDDLEGAME][PAWN / 2][MIRROR_SQUARE(sq)]--;

        coefficients->genericPieceValues[ENDGAME][PAWN / 2]--;
        coefficients->rawPieceTables[ENDGAME][PAWN / 2][MIRROR_SQUARE(sq)]--;

        *mgScore -= params.genericPieceValues[MIDDLEGAME][PAWN / 2];
        *mgScore -= params.rawPieceTables[MIDDLEGAME][PAWN / 2][MIRROR_SQUARE(sq)];
        
        *egScore -= params.genericPieceValues[ENDGAME][PAWN / 2];
        *egScore -= params.rawPieceTables[ENDGAME][PAWN / 2][MIRROR_SQUARE(sq)];

		//Passed Pawns
        if((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
        {
            coefficients->passedPawnBonus[MIDDLEGAME][mirroredRow]--;
            coefficients->passedPawnBonus[ENDGAME][mirroredRow]--;

            *mgScore -= hce_params.passedPawnBonus[MIDDLEGAME][mirroredRow];
            *egScore -= hce_params.passedPawnBonus[ENDGAME][mirroredRow];
        }
        
        //Doubled pawns
        if(__builtin_popcountll(board->pieces[BLACK_PAWN] & board_file[column]) > 1)
        {
            coefficients->doubledPawnBonus[MIDDLEGAME][mirroredColumn]--;
            coefficients->doubledPawnBonus[ENDGAME][mirroredColumn]--;

            *mgScore -= hce_params.doubledPawnBonus[MIDDLEGAME][mirroredColumn];
            *egScore -= hce_params.doubledPawnBonus[ENDGAME][mirroredColumn];
        }

		uint64_t borderingMask = board->pieces[BLACK_PAWN] & bordering_files[column];
        //Isolated Pawns
        if(!borderingMask)
        {
            coefficients->isolatedPawnBonus[MIDDLEGAME][mirroredColumn]--;
            coefficients->isolatedPawnBonus[ENDGAME][mirroredColumn]--;

            *mgScore -= hce_params.isolatedPawnBonus[MIDDLEGAME][mirroredColumn];
            *egScore -= hce_params.isolatedPawnBonus[ENDGAME][mirroredColumn];
        }
		//Backward pawns
		else if(row < 6 && (borderingMask & ~(sq - 1)) == 0 && borderingMask & board_rank[row - 1])
		{
            coefficients->backwardPawnBonus[MIDDLEGAME][mirroredColumn]--;
            coefficients->backwardPawnBonus[ENDGAME][mirroredColumn]--;

			*mgScore -= hce_params.backwardPawnBonus[MIDDLEGAME][mirroredColumn];
			*egScore -= hce_params.backwardPawnBonus[ENDGAME][mirroredColumn];
		}
		//connected pawns
		else if(borderingMask & board_rank[row + 1])
		{
            coefficients->connectedPawnBonus[MIDDLEGAME][mirroredColumn]--;
            coefficients->connectedPawnBonus[ENDGAME][mirroredColumn]--;

			*mgScore -= hce_params.connectedPawnBonus[MIDDLEGAME][mirroredColumn];
			*egScore -= hce_params.connectedPawnBonus[ENDGAME][mirroredColumn];
		}

		mask &= mask - 1;
	}
	
	*mgScore += *mgScore;
	*egScore += *egScore;
}

void initKnightCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
	uint64_t mask = board->pieces[WHITE_KNIGHT];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
        coefficients->genericPieceValues[MIDDLEGAME][KNIGHT / 2]++;
        coefficients->rawPieceTables[MIDDLEGAME][KNIGHT / 2][FLIP_SQUARE(sq)]++;

        coefficients->genericPieceValues[ENDGAME][KNIGHT / 2]++;
        coefficients->rawPieceTables[ENDGAME][KNIGHT / 2][FLIP_SQUARE(sq)]++;

        *mgScore += params.genericPieceValues[MIDDLEGAME][KNIGHT / 2];
        *mgScore += params.rawPieceTables[MIDDLEGAME][KNIGHT / 2][FLIP_SQUARE(sq)];
        
        *egScore += params.genericPieceValues[ENDGAME][KNIGHT / 2];
        *egScore += params.rawPieceTables[ENDGAME][KNIGHT / 2][FLIP_SQUARE(sq)];

		//mobility
		uint64_t moves = knightMoves(board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);

        coefficients->knightMobilityBonus[MIDDLEGAME][moveCount]++;
        coefficients->knightMobilityBonus[ENDGAME][moveCount]++;

		*mgScore += hce_params.knightMobilityBonus[MIDDLEGAME][moveCount];
		*egScore += hce_params.knightMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_KNIGHT];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/square value
        coefficients->genericPieceValues[MIDDLEGAME][KNIGHT / 2]--;
        coefficients->rawPieceTables[MIDDLEGAME][KNIGHT / 2][MIRROR_SQUARE(sq)]--;

        coefficients->genericPieceValues[ENDGAME][KNIGHT / 2]--;
        coefficients->rawPieceTables[ENDGAME][KNIGHT / 2][MIRROR_SQUARE(sq)]--;

        *mgScore -= params.genericPieceValues[MIDDLEGAME][KNIGHT / 2];
        *mgScore -= params.rawPieceTables[MIDDLEGAME][KNIGHT / 2][MIRROR_SQUARE(sq)];
        
        *egScore -= params.genericPieceValues[ENDGAME][KNIGHT / 2];
        *egScore -= params.rawPieceTables[ENDGAME][KNIGHT / 2][MIRROR_SQUARE(sq)];

		//mobility
		uint64_t moves = knightMoves(board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);

        coefficients->knightMobilityBonus[MIDDLEGAME][moveCount]--;
        coefficients->knightMobilityBonus[ENDGAME][moveCount]--;

		*mgScore -= hce_params.knightMobilityBonus[MIDDLEGAME][moveCount];
		*egScore -= hce_params.knightMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	*mgScore += *mgScore;
	*egScore += *egScore;
}

void initBishopCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
	uint64_t mask = board->pieces[WHITE_BISHOP];

	//bishop pair
	if(__builtin_popcountll(mask) >= 2)
	{
        coefficients->bishopPairBonus[MIDDLEGAME]++;
        coefficients->bishopPairBonus[ENDGAME]++;

		*mgScore += hce_params.bishopPairBonus[MIDDLEGAME];
		*egScore += hce_params.bishopPairBonus[ENDGAME];
	}

	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
        coefficients->genericPieceValues[MIDDLEGAME][BISHOP / 2]++;
        coefficients->rawPieceTables[MIDDLEGAME][BISHOP / 2][FLIP_SQUARE(sq)]++;

        coefficients->genericPieceValues[ENDGAME][BISHOP / 2]++;
        coefficients->rawPieceTables[ENDGAME][BISHOP / 2][FLIP_SQUARE(sq)]++;

        *mgScore += params.genericPieceValues[MIDDLEGAME][BISHOP / 2];
        *mgScore += params.rawPieceTables[MIDDLEGAME][BISHOP / 2][FLIP_SQUARE(sq)];
        
        *egScore += params.genericPieceValues[ENDGAME][BISHOP / 2];
        *egScore += params.rawPieceTables[ENDGAME][BISHOP / 2][FLIP_SQUARE(sq)];

		//mobility
		uint64_t moves = bishopMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);

        coefficients->bishopMobilityBonus[MIDDLEGAME][moveCount]++;
        coefficients->bishopMobilityBonus[ENDGAME][moveCount]++;

		*mgScore += hce_params.bishopMobilityBonus[MIDDLEGAME][moveCount];
		*egScore += hce_params.bishopMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_BISHOP];

	//bishop pair
	if(__builtin_popcountll(mask) >= 2)
	{
        coefficients->bishopPairBonus[MIDDLEGAME]--;
        coefficients->bishopPairBonus[ENDGAME]--;

		*mgScore -= hce_params.bishopPairBonus[MIDDLEGAME];
		*egScore -= hce_params.bishopPairBonus[ENDGAME];
	}

	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/sq value
        coefficients->genericPieceValues[MIDDLEGAME][BISHOP / 2]--;
        coefficients->rawPieceTables[MIDDLEGAME][BISHOP / 2][MIRROR_SQUARE(sq)]--;

        coefficients->genericPieceValues[ENDGAME][BISHOP / 2]--;
        coefficients->rawPieceTables[ENDGAME][BISHOP / 2][MIRROR_SQUARE(sq)]--;

        *mgScore -= params.genericPieceValues[MIDDLEGAME][BISHOP / 2];
        *mgScore -= params.rawPieceTables[MIDDLEGAME][BISHOP / 2][MIRROR_SQUARE(sq)];
        
        *egScore -= params.genericPieceValues[ENDGAME][BISHOP / 2];
        *egScore -= params.rawPieceTables[ENDGAME][BISHOP / 2][MIRROR_SQUARE(sq)];
		
		//mobility
		uint64_t moves = bishopMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);

        coefficients->bishopMobilityBonus[MIDDLEGAME][moveCount]--;
        coefficients->bishopMobilityBonus[ENDGAME][moveCount]--;

		*mgScore -= hce_params.bishopMobilityBonus[MIDDLEGAME][moveCount];
		*egScore -= hce_params.bishopMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	*mgScore += *mgScore;
	*egScore += *egScore;
}

void initRookCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
	uint64_t mask = board->pieces[WHITE_ROOK];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
        uint64_t pawns_col = board->pieces[WHITE_PAWN] & board_file[column];

		//piece/sq value
        coefficients->genericPieceValues[MIDDLEGAME][ROOK / 2]++;
        coefficients->rawPieceTables[MIDDLEGAME][ROOK / 2][FLIP_SQUARE(sq)]++;

        coefficients->genericPieceValues[ENDGAME][ROOK / 2]++;
        coefficients->rawPieceTables[ENDGAME][ROOK / 2][FLIP_SQUARE(sq)]++;

        *mgScore += params.genericPieceValues[MIDDLEGAME][ROOK / 2];
        *mgScore += params.rawPieceTables[MIDDLEGAME][ROOK / 2][FLIP_SQUARE(sq)];
        
        *egScore += params.genericPieceValues[ENDGAME][ROOK / 2];
        *egScore += params.rawPieceTables[ENDGAME][ROOK / 2][FLIP_SQUARE(sq)];
		
		//open rook file
        if(!pawns_col)
        {
            coefficients->openFileRookBonus[MIDDLEGAME][column]++;
            coefficients->openFileRookBonus[ENDGAME][column]++;

            *mgScore += hce_params.openFileRookBonus[MIDDLEGAME][column];
            *egScore += hce_params.openFileRookBonus[ENDGAME][column];
        }

		//mobility
		uint64_t moves = rookMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);
		
        coefficients->rookMobilityBonus[MIDDLEGAME][moveCount]++;
        coefficients->rookMobilityBonus[ENDGAME][moveCount]++;

		*mgScore += hce_params.rookMobilityBonus[MIDDLEGAME][moveCount];
		*egScore += hce_params.rookMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_ROOK];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
        uint64_t pawns_col = board->pieces[BLACK_PAWN] & board_file[column];
		
		//piece/sq value
        coefficients->genericPieceValues[MIDDLEGAME][ROOK / 2]--;
        coefficients->rawPieceTables[MIDDLEGAME][ROOK / 2][MIRROR_SQUARE(sq)]--;

        coefficients->genericPieceValues[ENDGAME][ROOK / 2]--;
        coefficients->rawPieceTables[ENDGAME][ROOK / 2][MIRROR_SQUARE(sq)]--;

        *mgScore -= params.genericPieceValues[MIDDLEGAME][ROOK / 2];
        *mgScore -= params.rawPieceTables[MIDDLEGAME][ROOK / 2][MIRROR_SQUARE(sq)];
        
        *egScore -= params.genericPieceValues[ENDGAME][ROOK / 2];
        *egScore -= params.rawPieceTables[ENDGAME][ROOK / 2][MIRROR_SQUARE(sq)];
		
		//open rook file
        if(!pawns_col)
        {
            coefficients->openFileRookBonus[MIDDLEGAME][column]--;
            coefficients->openFileRookBonus[ENDGAME][column]--;

            *mgScore -= hce_params.openFileRookBonus[MIDDLEGAME][column];
            *egScore -= hce_params.openFileRookBonus[ENDGAME][column];
        }
		
		//mobility
		uint64_t moves = rookMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		
        coefficients->rookMobilityBonus[MIDDLEGAME][moveCount]--;
        coefficients->rookMobilityBonus[ENDGAME][moveCount]--;

		*mgScore -= hce_params.rookMobilityBonus[MIDDLEGAME][moveCount];
		*egScore -= hce_params.rookMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	*mgScore += *mgScore;
	*egScore += *egScore;
}

void initQueenCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
	uint64_t mask = board->pieces[WHITE_QUEEN];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
        coefficients->genericPieceValues[MIDDLEGAME][QUEEN / 2]++;
        coefficients->rawPieceTables[MIDDLEGAME][QUEEN / 2][FLIP_SQUARE(sq)]++;

        coefficients->genericPieceValues[ENDGAME][QUEEN / 2]++;
        coefficients->rawPieceTables[ENDGAME][QUEEN / 2][FLIP_SQUARE(sq)]++;

        *mgScore += params.genericPieceValues[MIDDLEGAME][QUEEN / 2];
        *mgScore += params.rawPieceTables[MIDDLEGAME][QUEEN / 2][FLIP_SQUARE(sq)];
        
        *egScore += params.genericPieceValues[ENDGAME][QUEEN / 2];
        *egScore += params.rawPieceTables[ENDGAME][QUEEN / 2][FLIP_SQUARE(sq)];

		//mobility
		uint64_t moves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);

        coefficients->queenMobilityBonus[MIDDLEGAME][moveCount]++;
        coefficients->queenMobilityBonus[ENDGAME][moveCount]++;
		
		*mgScore += hce_params.queenMobilityBonus[MIDDLEGAME][moveCount];
		*egScore += hce_params.queenMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_QUEEN];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/square value
        coefficients->genericPieceValues[MIDDLEGAME][QUEEN / 2]--;
        coefficients->rawPieceTables[MIDDLEGAME][QUEEN / 2][MIRROR_SQUARE(sq)]--;

        coefficients->genericPieceValues[ENDGAME][QUEEN / 2]--;
        coefficients->rawPieceTables[ENDGAME][QUEEN / 2][MIRROR_SQUARE(sq)]--;

        *mgScore -= params.genericPieceValues[MIDDLEGAME][QUEEN / 2];
        *mgScore -= params.rawPieceTables[MIDDLEGAME][QUEEN / 2][MIRROR_SQUARE(sq)];
        
        *egScore -= params.genericPieceValues[ENDGAME][QUEEN / 2];
        *egScore -= params.rawPieceTables[ENDGAME][QUEEN / 2][MIRROR_SQUARE(sq)];

		//mobility
		uint64_t moves = queenMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		
        coefficients->queenMobilityBonus[MIDDLEGAME][moveCount]--;
        coefficients->queenMobilityBonus[ENDGAME][moveCount]--;

		*mgScore -= hce_params.queenMobilityBonus[MIDDLEGAME][moveCount];
		*egScore -= hce_params.queenMobilityBonus[ENDGAME][moveCount];

		mask &= mask - 1;
	}
	
	*mgScore += *mgScore;
	*egScore += *egScore;
}

void initKingCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{

	uint64_t mask = board->pieces[WHITE_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
        coefficients->genericPieceValues[MIDDLEGAME][KING / 2]++;
        coefficients->rawPieceTables[MIDDLEGAME][KING / 2][FLIP_SQUARE(sq)]++;

        coefficients->genericPieceValues[ENDGAME][KING / 2]++;
        coefficients->rawPieceTables[ENDGAME][KING / 2][FLIP_SQUARE(sq)]++;

        *mgScore += params.genericPieceValues[MIDDLEGAME][KING / 2];
        *mgScore += params.rawPieceTables[MIDDLEGAME][KING / 2][FLIP_SQUARE(sq)];
        
        *egScore += params.genericPieceValues[ENDGAME][KING / 2];
        *egScore += params.rawPieceTables[ENDGAME][KING / 2][FLIP_SQUARE(sq)];

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
        coefficients->virtualMobilityBonus[MIDDLEGAME][virtualMoveCount]++;
        coefficients->virtualMobilityBonus[ENDGAME][virtualMoveCount]++;

		*mgScore += hce_params.virtualMobilityBonus[MIDDLEGAME][virtualMoveCount];
		*egScore += hce_params.virtualMobilityBonus[ENDGAME][virtualMoveCount];

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/square value
        coefficients->genericPieceValues[MIDDLEGAME][KING / 2]--;
        coefficients->rawPieceTables[MIDDLEGAME][KING / 2][MIRROR_SQUARE(sq)]--;

        coefficients->genericPieceValues[ENDGAME][KING / 2]--;
        coefficients->rawPieceTables[ENDGAME][KING / 2][MIRROR_SQUARE(sq)]--;

        *mgScore -= params.genericPieceValues[MIDDLEGAME][KING / 2];
        *mgScore -= params.rawPieceTables[MIDDLEGAME][KING / 2][MIRROR_SQUARE(sq)];
        
        *egScore -= params.genericPieceValues[ENDGAME][KING / 2];
        *egScore -= params.rawPieceTables[ENDGAME][KING / 2][MIRROR_SQUARE(sq)];

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
        coefficients->virtualMobilityBonus[MIDDLEGAME][virtualMoveCount]--;
        coefficients->virtualMobilityBonus[ENDGAME][virtualMoveCount]--;

		*mgScore -= hce_params.virtualMobilityBonus[MIDDLEGAME][virtualMoveCount];
		*egScore -= hce_params.virtualMobilityBonus[ENDGAME][virtualMoveCount];

		mask &= mask - 1;
	}

	*mgScore += *mgScore;
	*egScore += *egScore;
}

void initCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
    *mgScore = 0;
    *egScore = 0;

    if(ISWHITE(board->turn))
    {
        coefficients->tempo[MIDDLEGAME]++;
        coefficients->tempo[ENDGAME]++;

        *mgScore += params.tempo[MIDDLEGAME];
        *egScore += params.tempo[ENDGAME];
    }
    else
    {
        coefficients->tempo[MIDDLEGAME]--;
        coefficients->tempo[ENDGAME]--;

        *mgScore -= params.tempo[MIDDLEGAME];
        *egScore -= params.tempo[ENDGAME];
    }

    initPawnCoefficients(board, coefficients, params, mgScore, egScore);
    initKnightCoefficients(board, coefficients, params, mgScore, egScore);
    initBishopCoefficients(board, coefficients, params, mgScore, egScore);
    initRookCoefficients(board, coefficients, params, mgScore, egScore);
    initQueenCoefficients(board, coefficients, params, mgScore, egScore);
    initKingCoefficients(board, coefficients, params, mgScore, egScore);
}

void initTuples(tuningEntry* entry, evalParameters* coefficients)
{
    int length = 0;
    for(int i = 0; i < PARAMETER_COUNT; i++)
        if(coefficients->parameters[i] != 0)
            length++;

    entry->activeTupleCount = length;
    entry->activeTuples = calloc(length, sizeof(tuningTuple));

    int insertIndex = 0;
    for(int i = 0; i < PARAMETER_COUNT; i++)
        if(coefficients->parameters[i] != 0)
            entry->activeTuples[insertIndex++] = (tuningTuple) { i, coefficients->parameters[i] };
}

void initSingleEntry(tuningEntry* entry, bitboard* board)
{
    int phase = 24;
    for(int pc = 2; pc < PIECE_COUNT - 2; pc++)
    {
        phase -= gamephasePieceValues[pc] * __builtin_popcountll(board->pieces[pc]);
    }
	phase = clamp(phase, 0, 24);

    entry->phaseFactors[MIDDLEGAME] = 1.0 - phase / 24.0;
    entry->phaseFactors[ENDGAME] = phase / 24.0;

	entry->phase[ENDGAME] = (phase * 256 + 12) / 24;
	entry->phase[MIDDLEGAME] = 256 - entry->phase[ENDGAME];

    //White relative
    evalParameters coefficients = {0};
    initCoefficients(board, &coefficients, currentParameters, &entry->mg_score, &entry->eg_score);

	entry->heuristic_eval =  (entry->phase[MIDDLEGAME] * entry->mg_score + entry->phase[ENDGAME] * entry->eg_score) / 256;

    initTuples(entry, &coefficients);
}

void initDataEntries(const char* dataPath)
{
    bitboard board;
    char buffer[256];

    FILE* data = fopen(dataPath, "r");
    if(!data)
    {
        printf("Cannot open data file.");
        exit(1);
    }

    int entryCount = 0;
    while(fgets(buffer, 256, data) != NULL)
    {
        if(entryCount % 100000 == 0)
            printf("\rCounting Entries: %d", entryCount);
        entryCount++;
    }

    tuner_entries = calloc(entryCount, sizeof(tuningEntry));
    tuner_entry_count = entryCount;

    rewind(data);
    int insertIndex = 0;
    while(fgets(buffer, 256, data) != NULL)
    {
        double result = 0.0;
        char* eval = strchr(buffer, '[');
        sscanf(eval, "[%lf]\n", &result);
        *(eval - 1) = '\0';

        load_fen_string_to_board(&board, buffer);

        //White relative
        tuner_entries[insertIndex].result = result;

        initSingleEntry(&tuner_entries[insertIndex++], &board);

        if(insertIndex % 100000 == 0)
            printf("\rInitialized Entry %d/%d", insertIndex, tuner_entry_count);
    }
    printf("\33[2K\r");

    fclose(data);
}

double getError(tuningEntry* entries, double K)
{
    double sumSquaredError = 0.0;

    #pragma omp parallel shared(sumSquaredError)
    {
        #pragma omp for reduction(+:sumSquaredError)
        for(int i = 0; i < tuner_entry_count; i++)
        {
            double err = entries[i].result - sigmoidK(entries[i].heuristic_eval, K);
            sumSquaredError += err * err;
        }
    }

    return sumSquaredError / tuner_entry_count;
}

double computeOptimalK(tuningEntry* entries)
{
    double start = -10.0;
    double end = 10.0;
    double step = 1.0;
    
    double current = start;
    printf("\33[2K\rCurrent K=%f", start);

    double currentError;
    double minError = getError(entries, start);

    for(int i = 0; i < 2; i++)
    {
        current = start - step;
        while(current < end)
        {
            current+= step;
            currentError = getError(entries, current);
            if(currentError <= minError)
            {
                minError = currentError;
                start = current;
                printf("\33[2K\rCurrent K=%f", start);
            }
        }

        end = start + step;
        start = start - step;
        step /= 10.0;
    }

    if(start == 0.0)
        start = 0.1;

    printf("\33[2K\r");
    return start;
}

void updateSingleGradient(tuningEntry* entry, evalParameters_fp* gradient, double K)
{
    int eval = entry->heuristic_eval;
    double s = sigmoidK(eval, K);
    double err = (entry->result - s) * s * (1 - s);
    double mgBase = err * entry->phaseFactors[MIDDLEGAME];
    double egBase = err * entry->phaseFactors[ENDGAME];

    for(int i = 0; i < entry->activeTupleCount; i++)
    {
        int index = entry->activeTuples[i].index;
        int coeff = entry->activeTuples[i].coefficient;

        if(is_param_eg.parameters[index])
            gradient->parameters[index] += egBase * coeff;
        else
            gradient->parameters[index] += mgBase * coeff;
    }
}

void computeGradient(tuningEntry* entries, evalParameters_fp* gradient,  double K)
{
    memset(gradient, 0, sizeof(evalParameters_fp));

    #pragma omp parallel shared(gradient)
    {
        evalParameters_fp local = {0};

        #pragma omp for
        for(int i = 0; i < tuner_entry_count; i++)
            updateSingleGradient(&entries[i], &local, K);

        for(int i = 0; i < PARAMETER_COUNT; i++)
            gradient->parameters[i] += local.parameters[i];
    }
}

void zeroCenter(double* table, int size)
{
    double sum = 0.0;
    for (int i = 0; i < size; i++)
        sum += table[i];

    double average = sum / size;
    for (int i = 0; i < size; i++)
        table[i] -= average;
}

void enforceMonotonicIncreasing(double* table,  int size)
{
    for (int i = 1; i < size; i++)
        table[i] = _max(table[i - 1] + 1, table[i]);
}

void enforceMonotonicDecreasing(double* table,  int size)
{
    for (int i = 1; i < size; i++)
        table[i] = _min(table[i - 1] - 1, table[i]);
}

void refreshEvaluations(evalParameters_fp* currentParameters, evalParameters_fp* deltaParameters)
{
    for(int i = 0; i < PARAMETER_COUNT; i++)
        currentParameters->parameters[i] += deltaParameters->parameters[i];

    //When initial weights are all zero (excepting 100/300/300/500/900 piece values), they tend to bleed together.
    //
    //I don't want a piece's mobility weights or square tables being in the range of [150, 250] with a raw piece value that is 
    //200 lower than what it should be.
    //
    //This is an attempt to enforce some kind of intuitive understanding of what
    //the weights are supposed to be doing.
    for(int phase = 0; phase < PHASE_COUNT; phase++)\
    {
        zeroCenter(&currentParameters->rawPieceTables[phase][PAWN / 2][8], 48);

        for(int pc = 1; pc < 6; pc++)
            zeroCenter(currentParameters->rawPieceTables[phase][pc], 64);

        zeroCenter(currentParameters->knightMobilityBonus[phase], 9);
        enforceMonotonicIncreasing(currentParameters->knightMobilityBonus[phase], 9);

        zeroCenter(currentParameters->bishopMobilityBonus[phase], 14);
        enforceMonotonicIncreasing(currentParameters->bishopMobilityBonus[phase], 14);

        zeroCenter(currentParameters->rookMobilityBonus[phase], 15);
        enforceMonotonicIncreasing(currentParameters->rookMobilityBonus[phase], 15);
        
        zeroCenter(currentParameters->queenMobilityBonus[phase], 28);
        enforceMonotonicIncreasing(currentParameters->queenMobilityBonus[phase], 28);

        zeroCenter(currentParameters->virtualMobilityBonus[phase], 28);
        enforceMonotonicDecreasing(currentParameters->virtualMobilityBonus[phase], 28);

        enforceMonotonicIncreasing(&currentParameters->passedPawnBonus[phase][1], 6);
    }


    #pragma omp parallel for
    for(int entryNum = 0; entryNum < tuner_entry_count; entryNum++)
    {
        tuningEntry* entry = &tuner_entries[entryNum];
        entry->eg_score = 0;
        entry->mg_score = 0;
        for(int i = 0; i < entry->activeTupleCount; i++)
        {
            int index = entry->activeTuples[i].index;
            int coeff = entry->activeTuples[i].coefficient;
            
            if(is_param_eg.parameters[index])
                entry->eg_score += coeff * currentParameters->parameters[index];
            else
                entry->mg_score += coeff * currentParameters->parameters[index];
        }

        entry->heuristic_eval = (entry->phase[MIDDLEGAME] * entry->mg_score + entry->phase[ENDGAME] * entry->eg_score) / 256;
    }
}

void Tune(const char* dataPath, const char* outputPath, double forcedK, uint64_t epochs, double max_lr, double min_lr)
{
    printf("Tuning for %lld epochs at LR=[%g ... %g]...\n", epochs, max_lr, min_lr);

    for(int i = 0; i < PARAMETER_COUNT; i++)
        currentParameters.parameters[i] = (double) hce_params.parameters[i];

    printf("Initializing data entries...\n");
    initDataEntries(dataPath);

    FILE* output = fopen(outputPath, "w");
    if(!output)
    {
        printf("Cannot open output file.\n");
        exit(1);
    }

    double K;
    if(forcedK != 0)
    {
        //Enforce specific scale.
        K = forcedK;
        printf("Enforcing ");
    }
    else
    {
        //Retain same scale of preexisting terms
        printf("Calculating K...\n");
        K = computeOptimalK(tuner_entries);
    }
    printf("K=%f\n", K);
    
    evalParameters_fp firstMoments = {0};
    evalParameters_fp secondMoments = {0};

    double lr = max_lr;
    int end_lr_epoch = _min(epochs, 2500);

    printf("Initial Error = %e\n", getError(tuner_entries, K));
    print_parameters(output, &currentParameters);

    for(int epoch = 1; epoch < epochs; epoch++)
    {
        if(epoch < end_lr_epoch)
        {
            lr = min_lr + 0.5 * (max_lr -  min_lr) * (1.0 + cos(PI * (epoch) / end_lr_epoch));
            lr = clamp(lr, min_lr, max_lr);
        }

        evalParameters_fp gradient = {0};
        computeGradient(tuner_entries, &gradient, K);

        double biasCorrection1 = 1.0 - pow(ADAM_BETA1, epoch + 1);
        double biasCorrection2 = 1.0 - pow(ADAM_BETA2, epoch + 1);

        for(int i = 0; i < PARAMETER_COUNT; i++)
        {
            //Average: /= tuner_entry_count
            //MSE derivative: *= 2
            //Chain rule: *= -K / 400.0
            gradient.parameters[i] *= (-K / (200.0 * tuner_entry_count));

            firstMoments.parameters[i] = ADAM_BETA1 * firstMoments.parameters[i] + (1.0 - ADAM_BETA1) * gradient.parameters[i];
            secondMoments.parameters[i] = ADAM_BETA2 * secondMoments.parameters[i] + (1.0 - ADAM_BETA2) * gradient.parameters[i] * gradient.parameters[i];;

            double correctedFirstMoment = firstMoments.parameters[i] / biasCorrection1;
            double correctedSecondMoment = secondMoments.parameters[i] / biasCorrection2;

            gradient.parameters[i] = -(lr * correctedFirstMoment) / (sqrt(correctedSecondMoment) + 1e-8);
        }

        refreshEvaluations(&currentParameters, &gradient);
        double error = getError(tuner_entries, K);

        printf("\rEpoch %d (LR=%g): Error = %e", epoch, lr, error);

        if(epoch % 25 == 0)
        {
            printf("\n");
            print_parameters(output, &currentParameters);
        }
    }

    printf("\nTuning complete!\n");

    fclose(output);
}