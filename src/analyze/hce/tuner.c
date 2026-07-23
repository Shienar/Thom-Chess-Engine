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
    
    fprintf(output, "\t.pawnAttacks = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int i = 0; i < PAWN_ATTACK_TYPES; i++)
            fprintf(output, "%5d%s", params.pawnAttacks[phase][i], (i == PAWN_ATTACK_TYPES - 1) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");

    fprintf(output, "\t.minorPawnCover = {%5d,%5d},\n", params.minorPawnCover[MIDDLEGAME], params.minorPawnCover[ENDGAME]);

    fprintf(output, "\t.passedPawnBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int column = 0; column < COLUMN_COUNT; column++)
            fprintf(output, "%5d%s", params.passedPawnBonus[phase][column], (column == 7) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");

    fprintf(output, "\t.connectedPawnBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int row = 0; row < ROW_COUNT; row++)
            fprintf(output, "%5d%s", params.connectedPawnBonus[phase][row], (row == 7) ? "" : ",");
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
    
    fprintf(output, "\t.knightOutpostBonus = {%5d,%5d},\n", params.knightOutpostBonus[MIDDLEGAME], params.knightOutpostBonus[ENDGAME]);

    fprintf(output, "\t.bishopPairBonus = {%5d,%5d},\n", params.bishopPairBonus[MIDDLEGAME], params.bishopPairBonus[ENDGAME]);
    fprintf(output, "\t.badBishopBonus = {%5d,%5d},\n", params.badBishopBonus[MIDDLEGAME], params.badBishopBonus[ENDGAME]);

    fprintf(output, "\t.openRookFileBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int i = 0; i < 2; i++)
            fprintf(output, "%5d%s", params.openRookFileBonus[phase][i], (i == 1) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");
    
    fprintf(output, "\t.connectedRookBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int i = 0; i < MAX_ROOK_CONNECTIONS; i++)
            fprintf(output, "%5d%s", params.connectedRookBonus[phase][i], (i == 1) ? "" : ",");
        fprintf(output, "}%s\n", (phase == PHASE_COUNT - 1) ? "" : ",");
    }
    fprintf(output, "\t},\n");
    
    fprintf(output, "\t.connectedQueenBonus = {\n");
    for(int phase = 0; phase < PHASE_COUNT; phase++) 
    {
        fprintf(output, "\t\t[%s] = {", phase_names[phase]);
        for(int i = 0; i < MAX_QUEEN_CONNECTIONS; i++)
            fprintf(output, "%5d%s", params.connectedQueenBonus[phase][i], (i == 2) ? "" : ",");
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
    
	int protectedCount = __builtin_popcountll((mask >> 8) & (board->pieces[WHITE_BISHOP] | board->pieces[WHITE_KNIGHT]));

    int pawnThreats = (~(board->pieces[BLACK_PAWN] | board->pieces[BLACK_KING])) & (WHITE_PAWN_LEFTATTACKS(board) | WHITE_PAWN_RIGHTATTACKS(board));
	while(pawnThreats)
	{
		int pc = findPieceOnSquare(board, __builtin_ctzll(pawnThreats));

		if(ISKNIGHT(pc))
		{
            UPDATE_COEFFICIENTS(pawnAttacks, 1, +=, [ATTACKING_KNIGHT]);
		}
		else if(ISBISHOP(pc))
		{
            UPDATE_COEFFICIENTS(pawnAttacks, 1, +=, [ATTACKING_BISHOP]);
		}
		else if(ISROOK(pc))
		{
            UPDATE_COEFFICIENTS(pawnAttacks, 1, +=, [ATTACKING_ROOK]);
		}
		else if(ISQUEEN(pc))
		{
            UPDATE_COEFFICIENTS(pawnAttacks, 1, +=, [ATTACKING_QUEEN]);
		}

		pawnThreats &= pawnThreats - 1;
	}

	while(mask)
	{
        int sq = __builtin_ctzll(mask);
        int column = getColumn(sq);
		int row = getRow(sq);

		//piece/square value
        UPDATE_COEFFICIENTS(genericPieceValues, 1, +=, [PAWN / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, +=, [PAWN / 2][FLIP_SQUARE(sq)]);

		//Passed Pawns
        if((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
        {
            UPDATE_COEFFICIENTS(passedPawnBonus, 1, +=, [row]);
        }

        //Doubled pawns
        if(__builtin_popcountll(board->pieces[WHITE_PAWN] & board_file[column]) > 1)
        {
            UPDATE_COEFFICIENTS(doubledPawnBonus, 1, +=, [column]);
        }

		uint64_t borderingMask = board->pieces[WHITE_PAWN] & bordering_files[column];
        //Isolated Pawns
        if(!borderingMask)
        {
            UPDATE_COEFFICIENTS(isolatedPawnBonus, 1, +=, [column]);
        }
		//connected pawns
		else if(borderingMask & (board_rank[row - 1] | board_rank[row + 1]))
		{
            UPDATE_COEFFICIENTS(connectedPawnBonus, 1, +=, [row]);
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
            UPDATE_COEFFICIENTS(pawnAttacks, 1, -=, [ATTACKING_KNIGHT]);
		}
		else if(ISBISHOP(pc))
		{
            UPDATE_COEFFICIENTS(pawnAttacks, 1, -=, [ATTACKING_BISHOP]);
		}
		else if(ISROOK(pc))
		{
            UPDATE_COEFFICIENTS(pawnAttacks, 1, -=, [ATTACKING_ROOK]);
		}
		else if(ISQUEEN(pc))
		{
            UPDATE_COEFFICIENTS(pawnAttacks, 1, -=, [ATTACKING_QUEEN]);
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
        UPDATE_COEFFICIENTS(genericPieceValues, 1, -=, [PAWN / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, -=, [PAWN / 2][MIRROR_SQUARE(sq)]);

		//Passed Pawns
        if((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
        {
            UPDATE_COEFFICIENTS(passedPawnBonus, 1, -=, [mirroredRow]);
        }
        
        //Doubled pawns
        if(__builtin_popcountll(board->pieces[BLACK_PAWN] & board_file[column]) > 1)
        {
            UPDATE_COEFFICIENTS(doubledPawnBonus, 1, -=, [mirroredColumn]);
        }

		uint64_t borderingMask = board->pieces[BLACK_PAWN] & bordering_files[column];
        //Isolated Pawns
        if(!borderingMask)
        {
            UPDATE_COEFFICIENTS(isolatedPawnBonus, 1, -=, [mirroredColumn]);
        }
		//connected pawns
		else if(borderingMask & (board_rank[row - 1] | board_rank[row + 1]))
		{
            UPDATE_COEFFICIENTS(connectedPawnBonus, 1, -=, [mirroredRow]);
		}

		mask &= mask - 1;
	}

    UPDATE_COEFFICIENTS(minorPawnCover, protectedCount, +=,);
}

void initKnightCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
	uint64_t mask = board->pieces[WHITE_KNIGHT];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
        int column = getColumn(sq);
        int row = getRow(sq);

		//piece/square value
        UPDATE_COEFFICIENTS(genericPieceValues, 1, +=, [KNIGHT / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, +=, [KNIGHT / 2][FLIP_SQUARE(sq)]);

		//mobility
		uint64_t moves = knightMoves(board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
        
        UPDATE_COEFFICIENTS(knightMobilityBonus, 1, +=, [moveCount]);

        //outpost
		if(row >= 4 && row <= 6 && 
		   board->pieces[WHITE_PAWN] && board_rank[row - 1] &&
		   (bordering_files[column] & board->pieces[BLACK_PAWN] & (~(singleBitMask(sq + 7) - 1))) == 0)
			{	
                UPDATE_COEFFICIENTS(knightOutpostBonus, 1, +=,);
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
        UPDATE_COEFFICIENTS(genericPieceValues, 1, -=, [KNIGHT / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, -=, [KNIGHT / 2][MIRROR_SQUARE(sq)]);

		//mobility
		uint64_t moves = knightMoves(board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);

        UPDATE_COEFFICIENTS(knightMobilityBonus, 1, -=, [moveCount]);

        //outpost
		if(row >= 1 && row <= 3 &&
		   board->pieces[BLACK_PAWN] && board_rank[row + 1] &&
		   (bordering_files[column] & board->pieces[WHITE_PAWN] & ((singleBitMask(sq - 6) - 1))) == 0)
			{	
                UPDATE_COEFFICIENTS(knightOutpostBonus, 1, -=,);
			}

		mask &= mask - 1;
	}
}

void initBishopCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
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
        UPDATE_COEFFICIENTS(bishopPairBonus, 1, +=,);
	}

	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
        UPDATE_COEFFICIENTS(genericPieceValues, 1, +=, [BISHOP / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, +=, [BISHOP / 2][FLIP_SQUARE(sq)]);

		//mobility
		uint64_t moves = bishopMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);

        UPDATE_COEFFICIENTS(bishopMobilityBonus, 1, +=, [moveCount]);

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
        UPDATE_COEFFICIENTS(bishopPairBonus, 1, -=,);
	}

	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/sq value
        UPDATE_COEFFICIENTS(genericPieceValues, 1, -=, [BISHOP / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, -=, [BISHOP / 2][MIRROR_SQUARE(sq)]);
		
		//mobility
		uint64_t moves = bishopMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);

        UPDATE_COEFFICIENTS(bishopMobilityBonus, 1, -=, [moveCount]);

		mask &= mask - 1;
	}
    
    UPDATE_COEFFICIENTS(badBishopBonus, badPawns, +=,);
}

void initRookCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
	int connectedRooksByRow = 0;
	int connectedRooksByColumn = 0;

	uint64_t mask = board->pieces[WHITE_ROOK];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		int column = getColumn(sq);
		int row = getRow(sq);

		//piece/sq value
        UPDATE_COEFFICIENTS(genericPieceValues, 1, +=, [ROOK / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, +=, [ROOK / 2][FLIP_SQUARE(sq)]);
		
		//open rook file
        if((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
        {
            if((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
            {
                UPDATE_COEFFICIENTS(openRookFileBonus, 1, +=, [OPEN_FILE]);
            }
            else
            {
                UPDATE_COEFFICIENTS(openRookFileBonus, 1, +=, [SEMI_OPEN_FILE]);
            }
        }

		//mobility
		uint64_t moves = rookMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);
		
        UPDATE_COEFFICIENTS(rookMobilityBonus, 1, +=, [moveCount]);
        
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
        UPDATE_COEFFICIENTS(genericPieceValues, 1, -=, [ROOK / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, -=, [ROOK / 2][MIRROR_SQUARE(sq)]);
		
		//open rook file
        if((board->pieces[BLACK_PAWN] & board_file[column]) == 0)
        {
            if((board->pieces[WHITE_PAWN] & board_file[column]) == 0)
            {
                UPDATE_COEFFICIENTS(openRookFileBonus, 1, -=, [OPEN_FILE]);
            }
            else
            {
                UPDATE_COEFFICIENTS(openRookFileBonus, 1, -=, [SEMI_OPEN_FILE]);
            }
        }
		
		//mobility
		uint64_t moves = rookMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		
        UPDATE_COEFFICIENTS(rookMobilityBonus, 1, -=, [moveCount]);
        
        //rook rams
		uint64_t connections = rookMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		uint64_t connectedRooks = connections & board->pieces[BLACK_ROOK];
		connectedRooksByRow -= __builtin_popcountll(connectedRooks & board_rank[row]);
		connectedRooksByColumn -= __builtin_popcountll(connectedRooks & board_file[column]);

		mask &= mask - 1;
	}

    UPDATE_COEFFICIENTS(connectedRookBonus, connectedRooksByColumn, +=, [CONNECTED_COLUMN]);
    UPDATE_COEFFICIENTS(connectedRookBonus, connectedRooksByRow, +=, [CONNECTED_ROW]);
}

void initQueenCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
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
        UPDATE_COEFFICIENTS(genericPieceValues, 1, +=, [QUEEN / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, +=, [QUEEN / 2][FLIP_SQUARE(sq)]);

		//mobility
		uint64_t moves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int moveCount = __builtin_popcountll(moves);

        UPDATE_COEFFICIENTS(queenMobilityBonus, 1, +=, [moveCount]);
        
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
        UPDATE_COEFFICIENTS(genericPieceValues, 1, -=, [QUEEN / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, -=, [QUEEN / 2][MIRROR_SQUARE(sq)]);

		//mobility
		uint64_t moves = queenMoves(board->pieces_side[BLACK], board->pieces_side[WHITE], sq);
		int moveCount = __builtin_popcountll(moves);
		
        UPDATE_COEFFICIENTS(queenMobilityBonus, 1, -=, [moveCount]);
        
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
    
    UPDATE_COEFFICIENTS(connectedQueenBonus, connectedSlidersByRow, +=, [CONNECTED_ROW]);
    UPDATE_COEFFICIENTS(connectedQueenBonus, connectedSlidersByColumn, +=, [CONNECTED_COLUMN]);
    UPDATE_COEFFICIENTS(connectedQueenBonus, connectedSlidersByDiagonal, +=, [CONNECTED_DIAGONAL]);
}

void initKingCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{

	uint64_t mask = board->pieces[WHITE_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);

		//piece/square value
        UPDATE_COEFFICIENTS(genericPieceValues, 1, +=, [KING / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, +=, [KING / 2][FLIP_SQUARE(sq)]);

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
        UPDATE_COEFFICIENTS(virtualMobilityBonus, 1, +=, [virtualMoveCount]);

		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
		
		//piece/square value
        UPDATE_COEFFICIENTS(genericPieceValues, 1, -=, [KING / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, -=, [KING / 2][MIRROR_SQUARE(sq)]);

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);

        UPDATE_COEFFICIENTS(virtualMobilityBonus, 1, -=, [virtualMoveCount]);

		mask &= mask - 1;
	}
}

void initCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
    *mgScore = 0;
    *egScore = 0;

    if(ISWHITE(board->turn))
    {
        UPDATE_COEFFICIENTS(tempo, 1, +=,);
    }
    else
    {
        UPDATE_COEFFICIENTS(tempo, 1, -=,);
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

void enforceZeroCenter(double* table, int size)
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
        enforceZeroCenter(&currentParameters->rawPieceTables[phase][PAWN / 2][8], 48);

        for(int pc = 1; pc < 6; pc++)
            enforceZeroCenter(currentParameters->rawPieceTables[phase][pc], 64);

        enforceZeroCenter(currentParameters->knightMobilityBonus[phase], 9);
        enforceMonotonicIncreasing(currentParameters->knightMobilityBonus[phase], 9);

        enforceZeroCenter(currentParameters->bishopMobilityBonus[phase], 14);
        enforceMonotonicIncreasing(currentParameters->bishopMobilityBonus[phase], 14);

        enforceZeroCenter(currentParameters->rookMobilityBonus[phase], 15);
        enforceMonotonicIncreasing(currentParameters->rookMobilityBonus[phase], 15);
        
        enforceZeroCenter(currentParameters->queenMobilityBonus[phase], 28);
        enforceMonotonicIncreasing(currentParameters->queenMobilityBonus[phase], 28);

        enforceZeroCenter(currentParameters->virtualMobilityBonus[phase], 28);
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