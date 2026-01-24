#include "../include/evolve.h"
#include "../include/debug.h"
#include "../include/bitboard.h"
#include "../include/moves.h"
#include <time.h>
#include <math.h>

void hill_climb(engine* starting_weights, int maxIterations, int numTweaksPerIteration, int maxDepth, int maxTime)
{
    for(int i = 0; i < maxIterations; i++)
    {
        engine* newWeights = createTweakedCopy(starting_weights);
        for(int j = 0; j < numTweaksPerIteration; j++)
        {
            printf("\rStarting iteration %d/%d, tweak %d/%d", i + 1, maxIterations, j + 1, numTweaksPerIteration);
            engine* newWeights2 = createTweakedCopy(starting_weights);
            if(compareEngines(newWeights, newWeights2, maxDepth, maxTime) <= 0)
            {
                FREE(newWeights);
                newWeights = newWeights2;
            }
            else
            {
                FREE(newWeights2);
            }
        }
        if(compareEngines(starting_weights, newWeights, maxDepth, maxTime) <= 0)
        {
            FREE(starting_weights);
            starting_weights = newWeights;
            saveEngineWeights(starting_weights);
        }
        else
        {
            FREE(newWeights);
        }
    }
    printf("\n");
}

/**
 * Box-Muller transform.
 * The learning rate can be interpreted as the standard deviation of the normal distribution.
 */
double generateRandomNumber() 
{
    double u1; 
    do { u1 = (double)rand() / (double) RAND_MAX; } while(u1 == 0);
    return LEARNING_RATE * sqrt(-2.0 * log(u1)) * cos(pi_2 *  (double)rand()/(double)RAND_MAX);
}

engine* createTweakedCopy(engine* original_weight)
{
    if(!original_weight) return NULL;
    engine* w = CALLOC(1, sizeof(engine));
    if(!w) return NULL;

    double newValue;
    for(int i = 0; i < 64; i++)
    {
        w->pawnPieceWeights[i] = original_weight->pawnPieceWeights[i];
        w->knightPieceWeights[i] = original_weight->knightPieceWeights[i];
        w->bishopPieceWeights[i] = original_weight->bishopPieceWeights[i];
        w->rookPieceWeights[i] = original_weight->rookPieceWeights[i];
        w->queenPieceWeights[i] = original_weight->queenPieceWeights[i];
        w->kingPieceWeights[i] = original_weight->kingPieceWeights[i];

        if(i > 7 && i < 56 && (float)rand()/(float)RAND_MAX < MUTATION_CHANCE_64) 
        {
            do { newValue = w->pawnPieceWeights[i]+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
            w->pawnPieceWeights[i] = newValue;
        }
        if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE_64)
        {
            do { newValue = w->knightPieceWeights[i]+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
            w->knightPieceWeights[i] = newValue;
        }
        if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE_64)
        {
            do { newValue = w->bishopPieceWeights[i]+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
            w->bishopPieceWeights[i] = newValue;
        }
        if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE_64) 
        {
            do { newValue = w->rookPieceWeights[i]+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
            w->rookPieceWeights[i] = newValue;
        }
        if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE_64) 
        {
            do { newValue = w->queenPieceWeights[i]+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
            w->queenPieceWeights[i] = newValue;
        }
        if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE_64) 
        {
            do { newValue = w->kingPieceWeights[i]+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
            w->kingPieceWeights[i] = newValue;
        }
    }
    
    w->pawnWeight = original_weight->pawnWeight;
    w->rookWeight = original_weight->rookWeight;
    w->knightWeight = original_weight->knightWeight;
    w->bishopWeight = original_weight->bishopWeight;
    w->queenWeight = original_weight->queenWeight;
    w->kingWeight = original_weight->kingWeight;
    
    if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE) 
    {
        do { newValue = w->pawnWeight+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
        w->pawnWeight = newValue;
    }
    if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE) 
    {
        do { newValue = w->knightWeight+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
        w->knightWeight = newValue;
    }
    if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE) 
    {
        do { newValue = w->bishopWeight+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
        w->bishopWeight = newValue;
    }
    if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE) 
    {
        do { newValue = w->rookWeight+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
        w->rookWeight = newValue;
    }
    if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE) 
    {
        do { newValue = w->queenWeight+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
        w->queenWeight = newValue;
    }
    if((float)rand()/(float)RAND_MAX < MUTATION_CHANCE) 
    {
        do { newValue = w->kingWeight+generateRandomNumber(); }while(!(newValue > MIN_WEIGHT && newValue < MAX_WEIGHT));
        w->kingWeight = newValue;
    }

    return w;
}

int compareEngines(engine* w1, engine* w2, int maxDepth, int maxTime)
{
    return play_engine_game(w1, w2, maxDepth, maxTime) - play_engine_game(w2, w1, maxDepth, maxTime);
}

int play_engine_game(engine* whiteWeights, engine* blackWeights, int maxDepth, int maxTime)
{
    bitboard* board = create_board();
    while(board->victor == 0)
    {
        move* bestMove;
        if(board->turn == WHITE) bestMove = calculateBestMove(board, whiteWeights, maxDepth, maxTime);
        else bestMove = calculateBestMove(board, blackWeights, maxDepth, maxTime);
        moveFromStruct(board, bestMove);
    }
    
    int returnValue = 0;
    if(board->victor == WHITE) returnValue = 1;
    else if(board->victor == BLACK) returnValue = -1;

    destroy_board(board);
    return returnValue;
}

int saveEngineWeights(engine* e)
{
    FILE* outputFile = fopen("weights/engine.txt", "w");
    if(!outputFile) 
    {
        DEBUG("Failed to save engine weights to file.")
        return -1;
    }

    fprintf(outputFile, "%lf %lf %lf %lf %lf %lf\n", e->pawnWeight, 
                                                   e->bishopWeight,
                                                   e->knightWeight,
                                                   e->rookWeight,
                                                   e->queenWeight,
                                                   e->kingWeight);
    for(int i = 0 ; i < 64; i++)
    {
        fprintf(outputFile, "%lf %lf %lf %lf %lf %lf\n", e->pawnPieceWeights[i], 
                                                         e->bishopPieceWeights[i],
                                                         e->knightPieceWeights[i],
                                                         e->rookPieceWeights[i],
                                                         e->queenPieceWeights[i],
                                                         e->kingPieceWeights[i]);
    }

    fclose(outputFile);
    return 0;
}

int loadEngineWeights(engine* e)
{
    FILE* inputFile = fopen("weights/engine.txt", "r");
    if(!inputFile) 
    {
        DEBUG("Failed to load engine weights from file.")
        return -1;
    }

    char buffer[128] = {0};
    fgets(buffer, 128, inputFile);
    sscanf(buffer, "%lf %lf %lf %lf %lf %lf", &e->pawnWeight, 
                                              &e->bishopWeight,
                                              &e->knightWeight,
                                              &e->rookWeight,
                                              &e->queenWeight,
                                              &e->kingWeight);
    for(int i = 0; i < 64; i++)
    {
        fgets(buffer, 128, inputFile);
        sscanf(buffer, "%lf %lf %lf %lf %lf %lf", &e->pawnPieceWeights[i], 
                                                  &e->bishopPieceWeights[i],
                                                  &e->knightPieceWeights[i],
                                                  &e->rookPieceWeights[i],
                                                  &e->queenPieceWeights[i],
                                                  &e->kingPieceWeights[i]);
    }
    return 0;
}