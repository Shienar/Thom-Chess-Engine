#include "tuner.h"
#include "analyze/engine.h"
#include <math.h>
#include <float.h>

//Print out all of the parameters in C array format.
//Just look at the declaration at the top of hce.c if this doesn't make sense.
void print_parameters(FILE* output, evalParameters_fp* currentParameters, evalParameters_fp* deltaParameters)
{
    evalParameters params = {0};
    for(int i = 0; i < PARAMETER_COUNT; i++)
    {
        currentParameters->parameters[i] += deltaParameters->parameters[i];
        deltaParameters->parameters[i] = 0.0;
        params.parameters[i] = round(currentParameters->parameters[i]);
    }
    
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

    fprintf(output, "\t.tempo = {%5d,%5d}\n", params.tempo[MIDDLEGAME], params.tempo[ENDGAME]);

    fprintf(output, "};\n");
    fflush(output);
}

evalParameters_fp currentParameters;
tuningEntry* tuner_entries = NULL;
int tuner_entry_count = 0;

void initPSQTCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
    uint64_t mask = board->pieces_side[WHITE];
    while(mask)
    {
        int sq = __builtin_ctzll(mask);
        int pieceIndex = findPieceOnSquare(board, sq) / 2;

        coefficients->genericPieceValues[MIDDLEGAME][pieceIndex]++;
        coefficients->rawPieceTables[MIDDLEGAME][pieceIndex][FLIP_SQUARE(sq)]++;

        coefficients->genericPieceValues[ENDGAME][pieceIndex]++;
        coefficients->rawPieceTables[ENDGAME][pieceIndex][FLIP_SQUARE(sq)]++;

        *mgScore += params.genericPieceValues[MIDDLEGAME][pieceIndex];
        *mgScore += params.rawPieceTables[MIDDLEGAME][pieceIndex][FLIP_SQUARE(sq)];
        
        *egScore += params.genericPieceValues[ENDGAME][pieceIndex];
        *egScore += params.rawPieceTables[ENDGAME][pieceIndex][FLIP_SQUARE(sq)];

        mask &= (mask - 1);
    }
    mask = board->pieces_side[BLACK];
    while(mask)
    {
        int sq = __builtin_ctzll(mask);
        int pieceIndex = findPieceOnSquare(board, sq) / 2;

        coefficients->genericPieceValues[MIDDLEGAME][pieceIndex]--;
        coefficients->rawPieceTables[MIDDLEGAME][pieceIndex][MIRROR_SQUARE(sq)]--;

        coefficients->genericPieceValues[ENDGAME][pieceIndex]--;
        coefficients->rawPieceTables[ENDGAME][pieceIndex][MIRROR_SQUARE(sq)]--;

        *mgScore -= params.genericPieceValues[MIDDLEGAME][pieceIndex];
        *mgScore -= params.rawPieceTables[MIDDLEGAME][pieceIndex][MIRROR_SQUARE(sq)];
        
        *egScore -= params.genericPieceValues[ENDGAME][pieceIndex];
        *egScore -= params.rawPieceTables[ENDGAME][pieceIndex][MIRROR_SQUARE(sq)];

        mask &= (mask - 1);
    }
}

void initTempoCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
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
}

//Includes virtual mobility
void initMobilityCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
	uint64_t whitePieces = board->pieces_side[WHITE];
	uint64_t blackPieces = board->pieces_side[BLACK];

	//Knight Mobility
	uint64_t pieces = board->pieces[WHITE_KNIGHT];
	while(pieces)
	{
		uint64_t moves = knightMoves(whitePieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);

		*mgScore += params.knightMobilityBonus[MIDDLEGAME][moveCount];
		*egScore += params.knightMobilityBonus[ENDGAME][moveCount];

        coefficients->knightMobilityBonus[MIDDLEGAME][moveCount]++;
        coefficients->knightMobilityBonus[ENDGAME][moveCount]++;

		pieces &= pieces - 1;
	}
	pieces = board->pieces[BLACK_KNIGHT];
	while(pieces)
	{
		uint64_t moves = knightMoves(blackPieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);

		*mgScore -= params.knightMobilityBonus[MIDDLEGAME][moveCount];
		*egScore -= params.knightMobilityBonus[ENDGAME][moveCount];
        
        coefficients->knightMobilityBonus[MIDDLEGAME][moveCount]--;
        coefficients->knightMobilityBonus[ENDGAME][moveCount]--;

		pieces &= pieces - 1;
	}

	//Bishop Mobility
	pieces = board->pieces[WHITE_BISHOP];
	while(pieces)
	{
		uint64_t moves = bishopMoves(whitePieces, blackPieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);

		*mgScore += params.bishopMobilityBonus[MIDDLEGAME][moveCount];
		*egScore += params.bishopMobilityBonus[ENDGAME][moveCount];
        
        coefficients->bishopMobilityBonus[MIDDLEGAME][moveCount]++;
        coefficients->bishopMobilityBonus[ENDGAME][moveCount]++;
		
		pieces &= pieces - 1;
	}
	pieces = board->pieces[BLACK_BISHOP];
	while(pieces)
	{
		uint64_t moves = bishopMoves(blackPieces, whitePieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);

		*mgScore -= params.bishopMobilityBonus[MIDDLEGAME][moveCount];
		*egScore -= params.bishopMobilityBonus[ENDGAME][moveCount];
        
        coefficients->bishopMobilityBonus[MIDDLEGAME][moveCount]--;
        coefficients->bishopMobilityBonus[ENDGAME][moveCount]--;

		pieces &= pieces - 1;
	}

	//Rook Mobility
	pieces = board->pieces[WHITE_ROOK];
	while(pieces)
	{
		uint64_t moves = rookMoves(whitePieces, blackPieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);
		
		*mgScore += params.rookMobilityBonus[MIDDLEGAME][moveCount];
		*egScore += params.rookMobilityBonus[ENDGAME][moveCount];
        
        coefficients->rookMobilityBonus[MIDDLEGAME][moveCount]++;
        coefficients->rookMobilityBonus[ENDGAME][moveCount]++;

		pieces &= pieces - 1;
	}
	pieces = board->pieces[BLACK_ROOK];
	while(pieces)
	{
		uint64_t moves = rookMoves(blackPieces, whitePieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);
		
		*mgScore -= params.rookMobilityBonus[MIDDLEGAME][moveCount];
		*egScore -= params.rookMobilityBonus[ENDGAME][moveCount];
        
        coefficients->rookMobilityBonus[MIDDLEGAME][moveCount]--;
        coefficients->rookMobilityBonus[ENDGAME][moveCount]--;

		pieces &= pieces - 1;
	}

	//Queen Mobility
	pieces = board->pieces[WHITE_QUEEN];
	while(pieces)
	{
		uint64_t moves = rookMoves(whitePieces, blackPieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);
		
		*mgScore += params.queenMobilityBonus[MIDDLEGAME][moveCount];
		*egScore += params.queenMobilityBonus[ENDGAME][moveCount];
        
        coefficients->queenMobilityBonus[MIDDLEGAME][moveCount]++;
        coefficients->queenMobilityBonus[ENDGAME][moveCount]++;
		
		pieces &= pieces - 1;
	}
	pieces = board->pieces[BLACK_QUEEN];
	while(pieces)
	{
		uint64_t moves = rookMoves(blackPieces, whitePieces, __builtin_ctzll(pieces));
		int moveCount = __builtin_popcountll(moves);
		
		*mgScore -= params.queenMobilityBonus[MIDDLEGAME][moveCount];
		*egScore -= params.queenMobilityBonus[ENDGAME][moveCount];
        
        coefficients->queenMobilityBonus[MIDDLEGAME][moveCount]--;
        coefficients->queenMobilityBonus[ENDGAME][moveCount]--;

		pieces &= pieces - 1;
	}

	//Virtual Mobility
	pieces = board->pieces[WHITE_KING];
	while(pieces)
	{
		uint64_t virtualMoves = queenMoves(whitePieces, blackPieces, __builtin_ctzll(pieces));
		int virtualMoveCount = __builtin_popcountll(virtualMoves);
		
		*mgScore += params.virtualMobilityBonus[MIDDLEGAME][virtualMoveCount];
		*egScore += params.virtualMobilityBonus[ENDGAME][virtualMoveCount];
        
        coefficients->virtualMobilityBonus[MIDDLEGAME][virtualMoveCount]++;
        coefficients->virtualMobilityBonus[ENDGAME][virtualMoveCount]++;

		pieces &= pieces - 1;
	}
	pieces = board->pieces[BLACK_KING];
	while(pieces)
	{
		uint64_t virtualMoves = queenMoves(blackPieces, whitePieces, __builtin_ctzll(pieces));
		int virtualMoveCount = __builtin_popcountll(virtualMoves);

		*mgScore -= params.virtualMobilityBonus[MIDDLEGAME][virtualMoveCount];
		*egScore -= params.virtualMobilityBonus[ENDGAME][virtualMoveCount];
        
        coefficients->virtualMobilityBonus[MIDDLEGAME][virtualMoveCount]--;
        coefficients->virtualMobilityBonus[ENDGAME][virtualMoveCount]--;

		pieces &= pieces - 1;
	}
}

void initSimplePieceDetailCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
    //Bishop pair bonuses
	if(__builtin_popcountll(board->pieces[WHITE_BISHOP]) >= 2)
	{
        coefficients->bishopPairBonus[MIDDLEGAME]++;
        coefficients->bishopPairBonus[ENDGAME]++;

        *mgScore += params.bishopPairBonus[MIDDLEGAME];
        *egScore += params.bishopPairBonus[ENDGAME];
	}
	if(__builtin_popcountll(board->pieces[BLACK_BISHOP]) >= 2)
	{
        coefficients->bishopPairBonus[MIDDLEGAME]--;
        coefficients->bishopPairBonus[ENDGAME]--;

        *mgScore -= params.bishopPairBonus[MIDDLEGAME];
        *egScore -= params.bishopPairBonus[ENDGAME];
	}

    //Rook open file bonuses
    uint64_t allyRooks = board->pieces[WHITE_ROOK];
    uint64_t enemyRooks = board->pieces[BLACK_ROOK];

    while(allyRooks)
    {
        int column = getColumn(__builtin_ctzll(allyRooks));
        uint64_t pawns_col = board->pieces[WHITE_PAWN] & board_file[column];

        if(!pawns_col)
        {
            coefficients->openFileRookBonus[MIDDLEGAME][column]++;
            coefficients->openFileRookBonus[ENDGAME][column]++;

            *mgScore += params.openFileRookBonus[MIDDLEGAME][column];
            *egScore += params.openFileRookBonus[ENDGAME][column];
        }

        allyRooks &= (allyRooks - 1);
    }

    while(enemyRooks)
    {
        int column = getColumn(__builtin_ctzll(enemyRooks));
		int mirroredColumn = MIRROR_SQUARE(column);
        uint64_t pawns_col = board->pieces[BLACK_PAWN] & board_file[column];
    
        if(!pawns_col)
        {
            coefficients->openFileRookBonus[MIDDLEGAME][mirroredColumn]--;
            coefficients->openFileRookBonus[ENDGAME][mirroredColumn]--;

            *mgScore -= params.openFileRookBonus[MIDDLEGAME][mirroredColumn];
            *egScore -= params.openFileRookBonus[ENDGAME][mirroredColumn];
        }

        enemyRooks &= (enemyRooks - 1);
    }

}

void initPawnCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
    uint64_t allyPawns = board->pieces[WHITE_PAWN];
    uint64_t enemyPawns = board->pieces[BLACK_PAWN];

    uint64_t mask = allyPawns;
    while(mask)
    {
		int sq = __builtin_ctzll(mask);
        int column = getColumn(sq);
		int row = getRow(sq);
        
        //Passed Pawns
        if((enemyPawns & board_file[column]) == 0)
        {
            coefficients->passedPawnBonus[MIDDLEGAME][row]++;
            coefficients->passedPawnBonus[ENDGAME][row]++;
            
            *mgScore += params.passedPawnBonus[MIDDLEGAME][row];
            *egScore += params.passedPawnBonus[ENDGAME][row];
        }

        //Doubled pawns
        if(__builtin_popcountll(allyPawns & board_file[column]) > 1)
        {
            coefficients->doubledPawnBonus[MIDDLEGAME][column]++;
            coefficients->doubledPawnBonus[ENDGAME][column]++;
            
            *mgScore += params.doubledPawnBonus[MIDDLEGAME][column];
            *egScore += params.doubledPawnBonus[ENDGAME][column];
        }

        //Isolated Pawns
        if((allyPawns & bordering_files[column]) == 0)
        {
            coefficients->isolatedPawnBonus[MIDDLEGAME][column]++;
            coefficients->isolatedPawnBonus[ENDGAME][column]++;
            
            *mgScore += params.isolatedPawnBonus[MIDDLEGAME][column];
            *egScore += params.isolatedPawnBonus[ENDGAME][column];
        }

        mask &= mask - 1;
    }
    
    mask = enemyPawns;
    while(mask)
    {
		int sq = __builtin_ctzll(mask);
        int column = getColumn(sq);
		int row = getRow(sq);

		int mirroredColumn = MIRROR_SQUARE(column);
		int mirroredRow = MIRROR_SQUARE(row);

        
        //Passed Pawns
        if((allyPawns & board_file[column]) == 0)
        {
            coefficients->passedPawnBonus[MIDDLEGAME][mirroredRow]--;
            coefficients->passedPawnBonus[ENDGAME][mirroredRow]--;
            
            *mgScore -= params.passedPawnBonus[MIDDLEGAME][mirroredRow];
            *egScore -= params.passedPawnBonus[ENDGAME][mirroredRow];
        }
        
        //Doubled pawns
        if(__builtin_popcountll(enemyPawns & board_file[column]) > 1)
        {
            coefficients->doubledPawnBonus[MIDDLEGAME][mirroredColumn]--;
            coefficients->doubledPawnBonus[ENDGAME][mirroredColumn]--;
            
            *mgScore -= params.doubledPawnBonus[MIDDLEGAME][mirroredColumn];
            *egScore -= params.doubledPawnBonus[ENDGAME][mirroredColumn];
        }

        
        //Isolated Pawns
        if((enemyPawns & bordering_files[column]) == 0)
        {
            coefficients->isolatedPawnBonus[MIDDLEGAME][mirroredColumn]--;
            coefficients->isolatedPawnBonus[ENDGAME][mirroredColumn]--;
            
            *mgScore -= params.isolatedPawnBonus[MIDDLEGAME][mirroredColumn];
            *egScore -= params.isolatedPawnBonus[ENDGAME][mirroredColumn];
        }

        mask &= mask - 1;
    }
}

void initCoefficients(bitboard* board, evalParameters* coefficients, evalParameters_fp params, double* mgScore, double* egScore)
{
    *mgScore = 0;
    *egScore = 0;

    initPSQTCoefficients(board, coefficients, params, mgScore, egScore);
    initTempoCoefficients(board, coefficients, params, mgScore, egScore);
    initMobilityCoefficients(board, coefficients, params, mgScore, egScore);
    initSimplePieceDetailCoefficients(board, coefficients, params, mgScore, egScore);
    initPawnCoefficients(board, coefficients, params, mgScore, egScore);

    //Filter out things not being tuned.

    //memset(coefficients->genericPieceValues, 0, sizeof(coefficients->genericPieceValues));
    //memset(coefficients->rawPieceTables, 0, sizeof(coefficients->rawPieceTables));
    
    //memset(coefficients->knightMobilityBonus, 0, sizeof(coefficients->knightMobilityBonus));
    //memset(coefficients->bishopMobilityBonus, 0, sizeof(coefficients->bishopMobilityBonus));
    //memset(coefficients->rookMobilityBonus, 0, sizeof(coefficients->rookMobilityBonus));
    //memset(coefficients->queenMobilityBonus, 0, sizeof(coefficients->queenMobilityBonus));
    //memset(coefficients->virtualMobilityBonus, 0, sizeof(coefficients->virtualMobilityBonus));
    
    //memset(coefficients->bishopPairBonus, 0, sizeof(coefficients->bishopPairBonus));
    //memset(coefficients->openFileRookBonus, 0, sizeof(coefficients->openFileRookBonus));
    
    //memset(coefficients->passedPawnBonus, 0, sizeof(coefficients->passedPawnBonus));
    //memset(coefficients->doubledPawnBonus, 0, sizeof(coefficients->doubledPawnBonus));
    //memset(coefficients->isolatedPawnBonus, 0, sizeof(coefficients->isolatedPawnBonus));
    
    //memset(coefficients->tempo, 0, sizeof(coefficients->tempo));
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

double updateEvaluation(tuningEntry* oldEntry, evalParameters_fp deltaWeights)
{
    for(int i = 0; i < oldEntry->activeTupleCount; i++)
    {
        int index = oldEntry->activeTuples[i].index;
        int coeff = oldEntry->activeTuples[i].coefficient;
        
        if(is_param_eg.parameters[index])
            oldEntry->eg_score += coeff * deltaWeights.parameters[index];
        else
            oldEntry->mg_score += coeff * deltaWeights.parameters[index];
    }

	oldEntry->heuristic_eval = (oldEntry->phase[MIDDLEGAME] * oldEntry->mg_score + oldEntry->phase[ENDGAME] * oldEntry->eg_score) / 256;
    return oldEntry->heuristic_eval;
}

void updateSingleGradient(tuningEntry* entry, evalParameters_fp* gradient, evalParameters_fp deltaWeights, double K)
{
    int eval = updateEvaluation(entry, deltaWeights);
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

void computeGradient(tuningEntry* entries, evalParameters_fp* gradient, evalParameters_fp deltaWeights,  double K)
{
    memset(gradient, 0, sizeof(evalParameters_fp));

    #pragma omp parallel shared(gradient)
    {
        evalParameters_fp local = {0};

        #pragma omp for
        for(int i = 0; i < tuner_entry_count; i++)
            updateSingleGradient(&entries[i], &local, deltaWeights, K);

        for(int i = 0; i < PARAMETER_COUNT; i++)
            gradient->parameters[i] += local.parameters[i];
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
    
    evalParameters_fp deltaWeights = {0};
    evalParameters_fp firstMoments = {0};
    evalParameters_fp secondMoments = {0};

    double lr = max_lr;
    int end_lr_epoch = _min(epochs, 2500);

    printf("Initial Error = %e\n", getError(tuner_entries, K));
    print_parameters(output, &currentParameters, &deltaWeights);

    for(int epoch = 1; epoch < epochs; epoch++)
    {
        if(epoch < end_lr_epoch)
        {
            lr = min_lr + 0.5 * (max_lr -  min_lr) * (1.0 + cos(PI * (epoch) / end_lr_epoch));
            lr = clamp(lr, min_lr, max_lr);
        }

        evalParameters_fp gradient = {0};
        computeGradient(tuner_entries, &gradient, deltaWeights, K);

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

            deltaWeights.parameters[i] -= (lr * correctedFirstMoment) / (sqrt(correctedSecondMoment) + 1e-8);
        }

        double error = getError(tuner_entries, K);

        printf("\rEpoch %d (LR=%g): Error = %e", epoch, lr, error);

        if(epoch % 25 == 0)
        {
            printf("\n");
            print_parameters(output, &currentParameters, &deltaWeights);
        }
    }

    printf("\nTuning complete!\n");

    fclose(output);
}