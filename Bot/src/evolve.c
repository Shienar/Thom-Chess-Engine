#include "../include/evolve.h"
#include "../include/debug.h"
#include "../include/bitboard.h"
#include "../include/moves.h"
#include <time.h>

void hill_climb(engine* starting_weights, int maxIterations, int numTweaksPerIteration)
{
    for(int i = 0; i < maxIterations; i++)
    {
        engine* newWeights = createTweakedCopy(starting_weights);
        for(int j = 0; j < numTweaksPerIteration - 1; j++)
        {
            engine* newWeights2 = createTweakedCopy(starting_weights);
            if(compareEngines(newWeights, newWeights2) < 0)
            {
                FREE(newWeights);
                newWeights = newWeights2;
            }
            else
            {
                FREE(newWeights2);
            }
        }
        if(compareEngines(starting_weights, newWeights) < 0)
        {
            FREE(starting_weights);
            starting_weights = newWeights;
        }
        else
        {
            FREE(newWeights);
        }
    }
}

engine* createTweakedCopy(engine* original_weight)
{
    srand(time(NULL));
    engine* w = CALLOC(1, sizeof(engine));
    if(!w) return NULL;

    for(int i = 0; i < 64; i++)
    {
        w->pawnPieceWeights[i] = original_weight->pawnPieceWeights[i];
        w->rookPieceWeights[i] = original_weight->rookPieceWeights[i];
        w->knightPieceWeights[i] = original_weight->knightPieceWeights[i];
        w->bishopPieceWeights[i] = original_weight->bishopPieceWeights[i];
        w->queenPieceWeights[i] = original_weight->queenPieceWeights[i];
        w->kingPieceWeights[i] = original_weight->kingPieceWeights[i];

        if(rand()/RAND_MAX < MUTATION_CHANCE) w->pawnPieceWeights[i]+=  (double)rand()/RAND_MAX*2.0 - 1;
        if(rand()/RAND_MAX < MUTATION_CHANCE) w->rookPieceWeights[i]+=  (double)rand()/RAND_MAX*2.0 - 1;
        if(rand()/RAND_MAX < MUTATION_CHANCE) w->knightPieceWeights[i]+=  (double)rand()/RAND_MAX*2.0 - 1;
        if(rand()/RAND_MAX < MUTATION_CHANCE) w->bishopPieceWeights[i]+=  (double)rand()/RAND_MAX*2.0 - 1;
        if(rand()/RAND_MAX < MUTATION_CHANCE) w->queenPieceWeights[i]+=  (double)rand()/RAND_MAX*2.0 - 1;
        if(rand()/RAND_MAX < MUTATION_CHANCE) w->kingPieceWeights[i]+=  (double)rand()/RAND_MAX*2.0 - 1;
    }
    
    w->pawnWeight = original_weight->pawnWeight;
    w->rookWeight = original_weight->rookWeight;
    w->knightWeight = original_weight->knightWeight;
    w->bishopWeight = original_weight->bishopWeight;
    w->queenWeight = original_weight->queenWeight;
    w->kingWeight = original_weight->kingWeight;
    
    if(rand()/RAND_MAX < MUTATION_CHANCE) w->pawnWeight+=  (double)rand()/RAND_MAX*2.0 - 1;
    if(rand()/RAND_MAX < MUTATION_CHANCE) w->rookWeight+=  (double)rand()/RAND_MAX*2.0 - 1;
    if(rand()/RAND_MAX < MUTATION_CHANCE) w->knightWeight+=  (double)rand()/RAND_MAX*2.0 - 1;
    if(rand()/RAND_MAX < MUTATION_CHANCE) w->bishopWeight+=  (double)rand()/RAND_MAX*2.0 - 1;
    if(rand()/RAND_MAX < MUTATION_CHANCE) w->queenWeight+=  (double)rand()/RAND_MAX*2.0 - 1;
    if(rand()/RAND_MAX < MUTATION_CHANCE) w->kingWeight+=  (double)rand()/RAND_MAX*2.0 - 1;

    return w;
}

int compareEngines(engine* w1, engine* w2)
{
    return play_engine_game(w1, w2) - play_engine_game(w2, w1);
}

int play_engine_game(engine* whiteWeights, engine* blackWeights)
{
    bitboard* board = create_board();
    while(board->victor == 0)
    {
        move* bestMove;
        if(board->turn == WHITE) bestMove = calculateBestMove(board, whiteWeights, 4, 4);
        else bestMove = calculateBestMove(board, blackWeights, 4, 4);
        moveFromStruct(board, bestMove);
    }
    
    int returnValue = 0;
    if(board->victor == WHITE) returnValue = 1;
    else if(board->victor == BLACK) returnValue = -1;

    destroy_board(board);
    return returnValue;
}