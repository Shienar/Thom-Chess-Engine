#include "./board/moves.h"
#include "./board/bitboard.h"
#include "debug.h"
#include "./hashtables/transpositiontable.h"
#include "./analyze/book.h"
#include "./analyze/neuralnet.h"
#include "./pyrrhic/tbprobe.h"
#include <stdio.h>
#include <string.h>

/**
 * TODO:
 * 
 * - Intermediate network layer outputs should get stored as 16-bit ints to avoid overflow in the player NNUE.
 * - Training!
 */

int main(int argc, char** argv)
{
    /**
     * Arguments
     */
    int verbose = 0;
    int player_color = WHITE;
    int depth = 4;
    int onlyEngines = 0;
    int onlyHumans = 0;
    int maxTime = 3;
    int printHistory = 0;
    int fenLineNumber = -1;
    int shouldTrain = 0;
    int shouldCreateTrainingData = 0;
    int shouldUpdateTrainingData = 0;
    int useBook = 1;
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--help") == 0)
        {
            printf("\n\n");
            printf("--help\t\tPrints out this message\n");
            printf("--debug\t\tEnable debug messages\n");
            printf("--leaks\t\tTrack memory leaks. Heavily reduces performance\n");
            printf("-v\t\tVerbose board information\n");
            printf("--history\tPrint's recent moves\n");
            printf("--black\t\tPlay as black\n");
            printf("--human\t\tHuman v human game\n");
            printf("--engine\t\tEngine v Engine game\n");
            printf("--depth\t\tChange the depth, takes one integer parameter\n");
            printf("--time\t\tSpecify maximum computation time per turn in seconds.\n");
            printf("--fen\t\tLoad a fen position from file. Specify the line number\n");
            printf("--nobook\t\tPrevents loading an opening book\n");
            printf("--init\t\tInitializes a new neural network if there is none and exits immediately afterwards.\n");
            printf("--train\t\tTrains the neural network. Specify that maximum number of iterations\n");
            printf("--generate\t\tCreates training data for the neural network. Specify the number of entries.\n");
            printf("--updatedata\t\tUpdates the existing training data to match the current engine.\n");
            printf("--singlethread\t\tDisables helper threads.\n");
            printf("\n\n");
            exit(0);
        }
        else if(strcmp(argv[i], "-v") == 0) verbose = 1;
        else if(strcmp(argv[i], "--black") == 0) player_color = BLACK;
        else if(strcmp(argv[i], "--depth") == 0) { i++; depth = atoi(argv[i]); }
        else if(strcmp(argv[i], "--debug") == 0) enableDebugMessages();
        else if(strcmp(argv[i], "--leaks") == 0) enableLeakTracking();
        else if(strcmp(argv[i], "--history") == 0) printHistory = 1;
        else if(strcmp(argv[i], "--human") == 0) { onlyHumans = 1; onlyEngines = 0; }
        else if(strcmp(argv[i], "--engine") == 0) { onlyHumans = 0; onlyEngines = 1; }
        else if(strcmp(argv[i], "--time") == 0) { i++; maxTime = atoi(argv[i]); }
        else if(strcmp(argv[i], "--fen") == 0) { i++; fenLineNumber = atoi(argv[i]); }
        else if(strcmp(argv[i], "--train") == 0) { i++; shouldTrain = atoi(argv[i]); }
        else if(strcmp(argv[i], "--generate") == 0) { i++; shouldCreateTrainingData = atoi(argv[i]); }
        else if(strcmp(argv[i], "--updatedata") == 0) shouldUpdateTrainingData = 1;
        else if(strcmp(argv[i], "--nobook") == 0) useBook = 0;
        else if(strcmp(argv[i], "--init") == 0) { load_playingWeights(); save_playingWeights(); exit(0);}
        else if(strcmp(argv[i], "--singlethread") == 0) useHelperThreads = 0;
    }
    
    srand(time(NULL));

    if(shouldTrain)
    {
        load_trainingWeights();
        trainingAccumulator = CALLOC(1, sizeof(accumulator_training));
        backpropagate(0, shouldTrain, 1e-6, trainingAccumulator);
        save_trainingWeights();

        load_playingWeights();
        quantizeWeights(trainingNNUE, playerNNUE);
        save_playingWeights();

        FREE(trainingNNUE);
        FREE(playerNNUE);

        dump_allocations();
        exit(0);
    }
    else if(shouldCreateTrainingData)
    {
        trainingAccumulator = CALLOC(1, sizeof(accumulator_training));
        generateTrainingData(depth, maxTime, shouldCreateTrainingData, trainingAccumulator);
        dump_allocations();
        exit(0);
    }
    else if(shouldUpdateTrainingData)
    {
        updateTrainingData(depth, maxTime);
        exit(0);
    }

    bitboard* board;
    if(fenLineNumber > 0)
    {
        board = create_board_from_fen("import/FEN.txt", fenLineNumber);
        if(!board)
        {
            dump_allocations();
            exit(1);
        }
    }
    else board = create_board();

    tb_init("./sygyzy/");

    if(useBook && !onlyHumans) loadBook();
    transpositionTable = create_hashTable_tt();
    
    load_playingWeights();
    playerAccumulator = CALLOC(1, sizeof(accumulator_playing));
    loadInputAccumulator(board, playerAccumulator, PLAYING, WHITE|BLACK);

    char buffer[6] = {'\0'};
    int error = 0;

    board_print(board, 0, printHistory);

    while(1)
    {
        if(onlyHumans || (board->turn == player_color && !onlyEngines))
        {
            fgets(buffer, 6, stdin);
            if(buffer[0] == 'q')
            {
                printf("Exiting program...");
                exit(1);
            }
            else if(buffer[0] == 'u')
            {
                move* tempMove = unmove(board);
                if(tempMove) FREE(tempMove);
                board_print(board, verbose, printHistory);
            }
            else
            {
                error = moveFromString(board, buffer);
                if(!error) board_print(board, verbose, printHistory);
            }
        }
        else
        {   
            do
            {
                move* bestMove = calculateBestMove(board, depth, maxTime);
                error = moveFromStruct(board, bestMove);
            }while(error != 0);
            board_print(board, verbose, printHistory);
        }
        
        if(board->victor == WHITE)
        {
            printf("White wins!\n\n");
            break;
        }
        else if(board->victor == BLACK)
        {
            printf("Black wins!\n\n");
            break;
        }
        else if (ISDRAW(board->victor))
        {
            printf("Draw!");
            if(board->victor&STALEMATED_WHITE) printf(" (White stalemated)\n\n");
            else if(board->victor&STALEMATED_BLACK) printf(" (Black stalemated)\n\n");
            else if(board->victor&THREEFOLD) printf(" (Threefold Repetition)\n\n");
            else if(board->victor&FIFTYMOVERULE) printf(" (50-move rule)\n\n");
            else if(board->victor&INSUFFICIENT_MATERIAL) printf(" (Insufficient Material)\n\n");
            break;
        }
    }

    FREE(playerNNUE);
    destroy_hashTable_tt(transpositionTable);
    destroy_board(board);
    tb_free();
    dump_allocations();
}