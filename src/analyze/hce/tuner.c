#include "tuner.h"
#include "analyze/search.h"
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
        params.parameters[i] = (eval_t)P(round(currentParameters->parameters[i].mg), round(currentParameters->parameters[i].eg));
    
    const char* piece_names[6] = {"PAWN", "KNIGHT", "BISHOP", "ROOK", "QUEEN", "KING"};

    rewind(output);
    fprintf(output, "evalParameters hce_params = {\n");
    
    fprintf(output, "\t.genericPieceValues = {\n\t\t");
    for(int piece = 0; piece < PIECE_TYPE_COUNT; piece++)
        fprintf(output, "P(%5d,%5d)%s ", params.genericPieceValues[piece].mg, params.genericPieceValues[piece].eg, (piece == (PIECE_TYPE_COUNT) - 1) ? "" : ",");
    fprintf(output, "\n\t},\n");

    fprintf(output, "\t.rawPieceTables = {\n");       
    for(int pc = 0; pc < 6; pc++)
    {
        fprintf(output, "\t\t\t[%s / 2] = {\n", piece_names[pc]);
        for(int row = 0; row < ROW_COUNT; row++) 
        {
            fprintf(output, "\t\t\t\t");
            for(int col = 0; col < COLUMN_COUNT; col++) 
            {
                int idx = row * COLUMN_COUNT + col;
                fprintf(output, "P(%5d,%5d)%s ", params.rawPieceTables[pc][idx].mg, params.rawPieceTables[pc][idx].eg, (idx == 63) ? "" : ",");
            }
            fprintf(output, "\n");
        }
        fprintf(output, "\t\t\t}%s\n", (pc == 5) ? "" : ",");
        }
    fprintf(output, "\t},\n");
    
    fprintf(output, "\t.knightMobilityBonus = {");
    for(int i = 0; i < 9; i++)
    {
        if(!(i % 8))
            fprintf(output, "\n\t\t");
        fprintf(output, "P(%5d,%5d)%s ", params.knightMobilityBonus[i].mg, params.knightMobilityBonus[i].eg, (i == 8) ? "" : ",");
    }
    fprintf(output, "\n\t},\n");
    
    fprintf(output, "\t.bishopMobilityBonus = {");
    for(int i = 0; i < 14; i++)
    {
        if(!(i % 8))
            fprintf(output, "\n\t\t");
        fprintf(output, "P(%5d,%5d)%s ", params.bishopMobilityBonus[i].mg, params.bishopMobilityBonus[i].eg, (i == 13) ? "" : ",");
    }
    fprintf(output, "\n\t},\n");
    
    fprintf(output, "\t.rookMobilityBonus = {");
    for(int i = 0; i < 15; i++)
    {
        if(!(i % 8))
            fprintf(output, "\n\t\t");
        fprintf(output, "P(%5d,%5d)%s ", params.rookMobilityBonus[i].mg, params.rookMobilityBonus[i].eg, (i == 14) ? "" : ",");
    }
    fprintf(output, "\n\t},\n");
    
    fprintf(output, "\t.queenMobilityBonus = {");
    for(int i = 0; i < 28; i++)
    {
        if(!(i % 8))
            fprintf(output, "\n\t\t");
        fprintf(output, "P(%5d,%5d)%s ", params.queenMobilityBonus[i].mg, params.queenMobilityBonus[i].eg, (i == 27) ? "" : ",");
    }
    fprintf(output, "\n\t},\n");
    
    fprintf(output, "\t.virtualMobilityBonus = {");
    for(int i = 0; i < 28; i++)
    {
        if(!(i % 8))
            fprintf(output, "\n\t\t");
        fprintf(output, "P(%5d,%5d)%s ", params.virtualMobilityBonus[i].mg, params.virtualMobilityBonus[i].eg, (i == 27) ? "" : ",");
    }
    fprintf(output, "\n\t},\n");

    fprintf(output, "\t.minorPawnCover = P(%5d,%5d),\n", params.minorPawnCover.mg, params.minorPawnCover.eg);

    fprintf(output, "\t.passedPawnBonus = {\n\t\t");
    for(int column = 0; column < COLUMN_COUNT; column++)
        fprintf(output, "P(%5d,%5d)%s ", params.passedPawnBonus[column].mg, params.passedPawnBonus[column].eg, (column == 7) ? "" : ",");
    fprintf(output, "\n\t},\n");

    fprintf(output, "\t.connectedPawnBonus = {\n\t\t");
    for(int row = 0; row < ROW_COUNT; row++)
        fprintf(output, "P(%5d,%5d)%s ", params.connectedPawnBonus[row].mg, params.connectedPawnBonus[row].eg, (row == 7) ? "" : ",");
    fprintf(output, "\n\t},\n");

    fprintf(output, "\t.doubledPawnBonus = {\n\t\t");
    for(int column = 0; column < COLUMN_COUNT; column++)
        fprintf(output, "P(%5d,%5d)%s ", params.doubledPawnBonus[column].mg, params.doubledPawnBonus[column].eg, (column == 7) ? "" : ",");
    fprintf(output, "\n\t},\n");

    fprintf(output, "\t.isolatedPawnBonus = {\n\t\t");
    for(int column = 0; column < COLUMN_COUNT; column++)
        fprintf(output, "P(%5d,%5d)%s ", params.isolatedPawnBonus[column].mg, params.isolatedPawnBonus[column].eg, (column == 7) ? "" : ",");
    fprintf(output, "\n\t},\n");
    
    fprintf(output, "\t.knightOutpostBonus = P(%5d,%5d),\n", params.knightOutpostBonus.mg, params.knightOutpostBonus.eg);

    fprintf(output, "\t.bishopPairBonus = P(%5d,%5d),\n", params.bishopPairBonus.mg, params.bishopPairBonus.eg);
    fprintf(output, "\t.badBishopBonus = P(%5d,%5d),\n", params.badBishopBonus.mg, params.badBishopBonus.eg);

    fprintf(output, "\t.openRookFileBonus = { P(%5d,%5d), P(%5d,%5d) },\n", params.openRookFileBonus[0].mg, params.openRookFileBonus[0].eg, 
                                                                            params.openRookFileBonus[1].mg, params.openRookFileBonus[1].eg);
    
    fprintf(output, "\t.connectedRookBonus = { P(%5d,%5d), P(%5d,%5d) },\n", params.connectedRookBonus[0].mg, params.connectedRookBonus[0].eg, 
                                                                             params.connectedRookBonus[1].mg, params.connectedRookBonus[1].eg);
                                                                            
    fprintf(output, "\t.connectedQueenBonus = { P(%5d,%5d), P(%5d,%5d), P(%5d,%5d) },\n", params.connectedQueenBonus[0].mg, params.connectedQueenBonus[0].eg,
                                                                                          params.connectedQueenBonus[1].mg, params.connectedQueenBonus[1].eg,
                                                                                          params.connectedQueenBonus[2].mg, params.connectedQueenBonus[2].eg);

                                                                                          
    fprintf(output, "\t.kingPawnShieldBonus = {\n\t\t");
    for(int column = 0; column < COLUMN_COUNT; column++)
        fprintf(output, "P(%5d,%5d)%s ", params.kingPawnShieldBonus[column].mg, params.kingPawnShieldBonus[column].eg, (column == 7) ? "" : ",");
    fprintf(output, "\n\t},\n");
    
    fprintf(output, "\t.kingPawnStormBonus = {\n\t\t");
    for(int column = 0; column < COLUMN_COUNT; column++)
        fprintf(output, "P(%5d,%5d)%s ", params.kingPawnStormBonus[column].mg, params.kingPawnStormBonus[column].eg, (column == 7) ? "" : ",");
    fprintf(output, "\n\t},\n");

    fprintf(output, "\t.openKingFile = { P(%5d,%5d), P(%5d,%5d) },\n", params.openKingFile[0].mg, params.openKingFile[0].eg, 
                                                                       params.openKingFile[1].mg, params.openKingFile[1].eg);

    fprintf(output, "\t.kingSafety = {\n");
    for(int i = 0; i < 10; i++)
    {
        fprintf(output, "\t\t");
        for(int j = 0; j < 10; j++)
        {
            int index = 10 * i + j;
            fprintf(output, "P(%5d,%5d)%s ", params.kingSafety[index].mg, params.kingSafety[index].eg, (index == 99) ? "" : ",");
        }
        fprintf(output, "\n");
    }
    fprintf(output, "\t},\n");

    fprintf(output, "\t.tempo = P(%5d,%5d),\n", params.tempo.mg, params.tempo.eg);

    fprintf(output, "};\n");
    fflush(output);
}

void initPawnCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, evalfp_t* score, evalContext* context)
{
	uint64_t mask = board->pieces[WHITE_PAWN];
    
	int protectedCount = __builtin_popcountll((mask >> 8) & (board->pieces[WHITE_BISHOP] | board->pieces[WHITE_KNIGHT]));

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

void initKnightCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, evalfp_t* score, evalContext* context)
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

		//king safety
		context->attackWeight[WHITE] += KNIGHT_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[BLACK]);
        
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
		
		//king safety
		context->attackWeight[BLACK] += KNIGHT_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[WHITE]);

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

void initBishopCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, evalfp_t* score, evalContext* context)
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

		//king safety
		context->attackWeight[WHITE] += BISHOP_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[BLACK]);

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

		//king safety
		context->attackWeight[BLACK] += BISHOP_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[WHITE]);

        UPDATE_COEFFICIENTS(bishopMobilityBonus, 1, -=, [moveCount]);

		mask &= mask - 1;
	}
    
    UPDATE_COEFFICIENTS(badBishopBonus, badPawns, +=,);
}

void initRookCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, evalfp_t* score, evalContext* context)
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
		
		//king safety
		context->attackWeight[WHITE] += ROOK_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[BLACK]);

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
		
		//king safety
		context->attackWeight[BLACK] += ROOK_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[WHITE]);

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

void initQueenCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, evalfp_t* score, evalContext* context)
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

		//king safety
		context->attackWeight[WHITE] += QUEEN_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[BLACK]);
		
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
		
		//king safety
		context->attackWeight[BLACK] += QUEEN_ATTACK_WEIGHT * __builtin_popcountll(moves & context->kingZone[WHITE]);
		
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

void initKingCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, evalfp_t* score, evalContext* context)
{
	uint64_t mask = board->pieces[WHITE_KING];
	
    int semiOpenFileCount = 0;
	int openFileCount = 0;

	while(mask)
	{
        int sq = __builtin_ctzll(mask);
        int row = getRow(sq);
		int column = getColumn(sq);

		//piece/square value
        UPDATE_COEFFICIENTS(genericPieceValues, 1, +=, [KING / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, +=, [KING / 2][FLIP_SQUARE(sq)]);

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
        UPDATE_COEFFICIENTS(virtualMobilityBonus, 1, +=, [virtualMoveCount]);
        
        if(row <= 1)
        {
            //pawn shield
            int pawnShieldCount = __builtin_popcountll(kingPawnShieldMask[WHITE][column] & board->pieces[WHITE_PAWN]);
            UPDATE_COEFFICIENTS(kingPawnShieldBonus, pawnShieldCount, +=, [column]);

            //pawn storm
            for(uint64_t stormMask = kingPawnStormMask[column] & board->pieces[BLACK_PAWN]; stormMask > 0; stormMask &= stormMask - 1)
            {
                int pawnRow = getRow(__builtin_ctzll(stormMask));
                UPDATE_COEFFICIENTS(kingPawnStormBonus, 1, +=, [pawnRow]);
            }
            
            //(semi) open file
			for(int column = 0; column < 8; column++) 
			{
				for(int column_offset = -1; column_offset <= 1; column_offset++) 
				{
					int target_column = column + column_offset;
					if(target_column >= 0 && target_column <= 7) 
					{
						if((board->pieces[WHITE_PAWN] & board_file[target_column]) == 0)
						{
							if((board->pieces[BLACK_PAWN] & board_file[target_column]) == 0)
								openFileCount++;
							else
								semiOpenFileCount++;
						}
					}
				}
			}
        }
		mask &= mask - 1;
	}
	
	mask = board->pieces[BLACK_KING];
	while(mask)
	{
        int sq = __builtin_ctzll(mask);
        int row = getRow(sq);
		int column = getColumn(sq);

        int mirroredRow = MIRROR_SQUARE(row);
		
		//piece/square value
        UPDATE_COEFFICIENTS(genericPieceValues, 1, -=, [KING / 2]);
        UPDATE_COEFFICIENTS(rawPieceTables, 1, -=, [KING / 2][MIRROR_SQUARE(sq)]);

		//virtual mobility
		uint64_t virtualMoves = queenMoves(board->pieces_side[WHITE], board->pieces_side[BLACK], sq);
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
        UPDATE_COEFFICIENTS(virtualMobilityBonus, 1, -=, [virtualMoveCount]);

        if(mirroredRow <= 1)
        {
            //pawn shield
            int pawnShieldCount = __builtin_popcountll(kingPawnShieldMask[BLACK][column] & board->pieces[BLACK_PAWN]);
            UPDATE_COEFFICIENTS(kingPawnShieldBonus, pawnShieldCount, -=, [column]);

            //pawn storm
            for(uint64_t stormMask = kingPawnStormMask[column] & board->pieces[BLACK_PAWN]; stormMask > 0; stormMask &= stormMask - 1)
            {
                int pawnRow = getRow(__builtin_ctzll(stormMask));
                pawnRow = MIRROR_SQUARE(pawnRow);
                UPDATE_COEFFICIENTS(kingPawnStormBonus, 1, -=, [pawnRow]);
            }
            
            //(semi) open file
			for(int column = 0; column < 8; column++) 
			{
				for(int column_offset = -1; column_offset <= 1; column_offset++) 
				{
					int target_column = column + column_offset;
					if(target_column >= 0 && target_column <= 7)
					{
						if((board->pieces[BLACK_PAWN] & board_file[target_column]) == 0)
						{
							if((board->pieces[WHITE_PAWN] & board_file[target_column]) == 0)
								openFileCount--;
							else
								semiOpenFileCount--;
						}
					}
				}
			}
        }
		mask &= mask - 1;
	}

	//king safety
	context->attackWeight[WHITE] = _min(context->attackWeight[WHITE], 99);
	context->attackWeight[BLACK] = _min(context->attackWeight[BLACK], 99);
    UPDATE_COEFFICIENTS(kingSafety, 1, +=, [context->attackWeight[WHITE]]);
    UPDATE_COEFFICIENTS(kingSafety, 1, -=, [context->attackWeight[BLACK]]);

    UPDATE_COEFFICIENTS(openKingFile, openFileCount, +=, [OPEN_FILE]);
    UPDATE_COEFFICIENTS(openKingFile, semiOpenFileCount, +=, [SEMI_OPEN_FILE]);
}

void initCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, evalfp_t* score)
{
    score->mg = 0.0;
    score->eg = 0.0;
    
	evalContext context = {
		.kingZone = {
			[WHITE] = kingZone[WHITE][board->kingSquare[WHITE]],
			[BLACK] = kingZone[BLACK][board->kingSquare[BLACK]]
		}
	};

    if(ISWHITE(board->turn))
        UPDATE_COEFFICIENTS(tempo, 1, +=,);
    else
        UPDATE_COEFFICIENTS(tempo, 1, -=,);

    initPawnCoefficients(board, coefficients, params, score, &context);
    initKnightCoefficients(board, coefficients, params, score, &context);
    initBishopCoefficients(board, coefficients, params, score, &context);
    initRookCoefficients(board, coefficients, params, score, &context);
    initQueenCoefficients(board, coefficients, params, score, &context);
    initKingCoefficients(board, coefficients, params, score, &context);
}

void initTuples(tuningEntry* entry, evalParameters* coefficients)
{
    //This currently works since midgame & endgame coefficients are always equal. 
    int length = 0;
    for(int i = 0; i < PARAMETER_COUNT; i++)
        if(coefficients->parameters[i].mg != 0)
            length++;

    entry->activeTupleCount = length;
    entry->activeTuples = calloc(length, sizeof(tuningTuple));

    int insertIndex = 0;
    for(int i = 0; i < PARAMETER_COUNT; i++)
        if(coefficients->parameters[i].mg != 0)
            entry->activeTuples[insertIndex++] = (tuningTuple) { i, coefficients->parameters[i].mg };
}

void initSingleEntry(tuningEntry* entry, bitboard* board)
{
    int phase = 24;
    for(int pc = 2; pc < PIECE_COUNT - 2; pc++)
    {
        phase -= gamephasePieceValues[pc] * __builtin_popcountll(board->pieces[pc]);
    }
	phase = clamp(phase, 0, 24);

    entry->phaseFactors.mg = 1.0 - phase / 24.0;
    entry->phaseFactors.eg = phase / 24.0;

	entry->phase.eg = (phase * 256 + 12) / 24;
	entry->phase.mg = 256 - entry->phase.eg;

    //White relative
    evalParameters coefficients = {0};
    initCoefficients(board, &coefficients, currentParameters, &entry->score);

	entry->heuristic_eval = (entry->phase.mg * entry->score.mg + entry->phase.eg * entry->score.eg) / 256;

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

//Technically updating 2. Endgame + midgame.
void updateSingleGradient(tuningEntry* entry, evalParameters_fp* gradient, double K)
{
    int eval = entry->heuristic_eval;
    double s = sigmoidK(eval, K);
    double err = (entry->result - s) * s * (1 - s);
    double mgBase = err * entry->phaseFactors.mg;
    double egBase = err * entry->phaseFactors.eg;

    for(int i = 0; i < entry->activeTupleCount; i++)
    {
        int index = entry->activeTuples[i].index;
        int coeff = entry->activeTuples[i].coefficient;

        gradient->parameters[index].mg += mgBase * coeff;
        gradient->parameters[index].eg += egBase * coeff;
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
        {
            gradient->parameters[i].mg += local.parameters[i].mg;
            gradient->parameters[i].eg += local.parameters[i].eg;
        }
    }
}

void enforceZeroCenter(evalfp_t* table, int size)
{
    double sum = 0.0;
    for (int i = 0; i < size; i++)
        sum += table[i].mg;

    double average = sum / size;
    for (int i = 0; i < size; i++)
        table[i].mg -= average;
        
    sum = 0.0;
    for (int i = 0; i < size; i++)
        sum += table[i].eg;

    average = sum / size;
    for (int i = 0; i < size; i++)
        table[i].eg -= average;
}

void enforceMonotonicIncreasing(evalfp_t* table,  int size)
{
    for (int i = 1; i < size; i++)
    {
        table[i].mg = _max(table[i - 1].mg, table[i].mg);
        table[i].eg = _max(table[i - 1].eg, table[i].eg);
    }
}

void enforceMonotonicDecreasing(evalfp_t* table,  int size)
{
    for (int i = 1; i < size; i++)
    {
        table[i].mg = _min(table[i - 1].mg, table[i].mg);
        table[i].eg = _min(table[i - 1].eg, table[i].eg);
    }
}

void enforceZeroMin(evalfp_t* table, int size)
{
    double minValue_mg = DBL_MAX;
    double minValue_eg = DBL_MAX;
    for(int i = 0; i < size; i++)
    {
        minValue_mg = _min(minValue_mg, table[i].mg);
        minValue_eg = _min(minValue_eg, table[i].eg);
    }

    for(int i = 0; i < size; i++)
    {
        table[i].mg -= minValue_mg;
        table[i].eg -= minValue_eg;
    }
}

void refreshEvaluations(evalParameters_fp* currentParameters, evalParameters_fp* deltaParameters)
{
    for(int i = 0; i < PARAMETER_COUNT; i++)
    {
        currentParameters->parameters[i].mg += deltaParameters->parameters[i].mg;
        currentParameters->parameters[i].eg += deltaParameters->parameters[i].eg;
    }

    //When initial weights are all zero (excepting 100/300/300/500/900 piece values), they tend to bleed together.
    //
    //I don't want a piece's mobility weights or square tables being in the range of [150, 250] with a raw piece value that is 
    //200 lower than what it should be.
    //
    //This is an attempt to enforce some kind of intuitive understanding of what
    //the weights are supposed to be doing.
   
    enforceZeroCenter(&currentParameters->rawPieceTables[PAWN / 2][8], 48);

    for(int pc = 1; pc < 6; pc++)
        enforceZeroCenter(currentParameters->rawPieceTables[pc], 64);

    enforceZeroCenter(currentParameters->knightMobilityBonus, 9);
    enforceMonotonicIncreasing(currentParameters->knightMobilityBonus, 9);

    enforceZeroCenter(currentParameters->bishopMobilityBonus, 14);
    enforceMonotonicIncreasing(currentParameters->bishopMobilityBonus, 14);

    enforceZeroCenter(currentParameters->rookMobilityBonus, 15);
    enforceMonotonicIncreasing(currentParameters->rookMobilityBonus, 15);
    
    enforceZeroCenter(currentParameters->queenMobilityBonus, 28);
    enforceMonotonicIncreasing(currentParameters->queenMobilityBonus, 28);

    enforceZeroCenter(currentParameters->virtualMobilityBonus, 28);
    enforceMonotonicDecreasing(currentParameters->virtualMobilityBonus, 28);

    enforceMonotonicIncreasing(&currentParameters->passedPawnBonus[1], 6);

    enforceZeroMin(currentParameters->kingSafety, 100);
    enforceMonotonicIncreasing(currentParameters->kingSafety, 100);

    #pragma omp parallel for
    for(int entryNum = 0; entryNum < tuner_entry_count; entryNum++)
    {
        tuningEntry* entry = &tuner_entries[entryNum];
        entry->score.mg = 0;
        entry->score.eg = 0;
        for(int i = 0; i < entry->activeTupleCount; i++)
        {
            int index = entry->activeTuples[i].index;
            int coeff = entry->activeTuples[i].coefficient;
            
            entry->score.mg += coeff * currentParameters->parameters[index].mg;
            entry->score.eg += coeff * currentParameters->parameters[index].eg;
        }

        entry->heuristic_eval = (entry->phase.mg * entry->score.mg + entry->phase.eg * entry->score.eg) / 256;
    }
}

void Tune(const char* dataPath, const char* outputPath, double forcedK, uint64_t epochs, double max_lr, double min_lr)
{
    printf("Tuning for %" PRId64"  epochs at LR=[%g ... %g]...\n", epochs, max_lr, min_lr);

    for(int i = 0; i < PARAMETER_COUNT; i++)
    {
        currentParameters.parameters[i].mg = (double) hce_params.parameters[i].mg;
        currentParameters.parameters[i].eg = (double) hce_params.parameters[i].eg;
    }

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
            gradient.parameters[i].mg *= (-K / (200.0 * tuner_entry_count));
            gradient.parameters[i].eg *= (-K / (200.0 * tuner_entry_count));

            firstMoments.parameters[i].mg = ADAM_BETA1 * firstMoments.parameters[i].mg + (1.0 - ADAM_BETA1) * gradient.parameters[i].mg;
            secondMoments.parameters[i].mg = ADAM_BETA2 * secondMoments.parameters[i].mg + (1.0 - ADAM_BETA2) * gradient.parameters[i].mg * gradient.parameters[i].mg;

            firstMoments.parameters[i].eg = ADAM_BETA1 * firstMoments.parameters[i].eg + (1.0 - ADAM_BETA1) * gradient.parameters[i].eg;
            secondMoments.parameters[i].eg = ADAM_BETA2 * secondMoments.parameters[i].eg + (1.0 - ADAM_BETA2) * gradient.parameters[i].eg * gradient.parameters[i].eg;

            evalfp_t correctedFirstMoment = P(firstMoments.parameters[i].mg / biasCorrection1, firstMoments.parameters[i].eg / biasCorrection1);
            evalfp_t correctedSecondMoment = P(secondMoments.parameters[i].mg / biasCorrection2, secondMoments.parameters[i].eg / biasCorrection2);

            gradient.parameters[i].mg = -(lr * correctedFirstMoment.mg) / (sqrt(correctedSecondMoment.mg) + 1e-8);
            gradient.parameters[i].eg = -(lr * correctedFirstMoment.eg) / (sqrt(correctedSecondMoment.eg) + 1e-8);
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