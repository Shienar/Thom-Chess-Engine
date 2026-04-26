#include "./board/moves.h"
#include "./board/bitboard.h"
#include "debug.h"
#include "./hashtables/transpositiontable.h"
#include "./analyze/book.h"
#include "./analyze/neuralnet.h"
#include "./pyrrhic/tbprobe.h"
#include "./analyze/engine.h"
#include <stdio.h>
#include <string.h>
#include <omp.h>

/**
 * TODO:
 *  - Larger bitboard size seems to be causing stack memory issues in 
 *  some functions. Memory is silently breaking and causing inconsistent
 *  issues. This mainly happens in the calculateBestMove() function.
 * 
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
    int useBook = 1; 
    int saveEveryNBlocks = 10;
    int shouldPerft = 0;
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--help") == 0)
        {
            printf("\n\n");
            printf("--help\t\tPrints out this message\n");
            printf("--debug\t\tEnable debug messages\n");
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
            printf("--train\t\ttrains the neural network. Pass in iteration count and how often you want to save.\n");
            printf("--singlethread\t\tDisables helper threads.\n");
            printf("--perft\t\tMove generation performance test.\n");
            printf("\n\n");
            exit(0);
        }
        else if(strcmp(argv[i], "-v") == 0) verbose = 1;
        else if(strcmp(argv[i], "--black") == 0) player_color = BLACK;
        else if(strcmp(argv[i], "--depth") == 0) { i++; depth = min(atoi(argv[i]), MAX_DEPTH - 5); }
        else if(strcmp(argv[i], "--debug") == 0) enableDebugMessages();
        else if(strcmp(argv[i], "--history") == 0) printHistory = 1;
        else if(strcmp(argv[i], "--human") == 0) { onlyHumans = 1; onlyEngines = 0; }
        else if(strcmp(argv[i], "--engine") == 0) { onlyHumans = 0; onlyEngines = 1; }
        else if(strcmp(argv[i], "--time") == 0) { i++; maxTime = atoi(argv[i]); }
        else if(strcmp(argv[i], "--fen") == 0) { i++; fenLineNumber = atoi(argv[i]); }
        else if(strcmp(argv[i], "--train") == 0) { i++; shouldTrain = atoi(argv[i]); i++; saveEveryNBlocks = atoi(argv[i]); }
        else if(strcmp(argv[i], "--nobook") == 0) useBook = 0;
        else if(strcmp(argv[i], "--init") == 0) { 
            load_trainingWeights();
            load_playingWeights(); 
            quantizeWeights(trainingNNUE, playerNNUE);
            save_playingWeights(); 
            free(trainingNNUE);
            free(playerNNUE);
            exit(0);
        }
        else if(strcmp(argv[i], "--perft") == 0) shouldPerft = 1;
        else if(strcmp(argv[i], "--singlethread") == 0) useHelperThreads = 0;
    }

    omp_set_num_threads(HELPER_THREAD_COUNT); 
    srand(time(NULL));

    initMagics();
    initZobristPieceKeys();
    initPawnAttacks();
    initKnightMoveTable();
    initKingMoveTable();

    if(shouldTrain)
    {
        load_trainingWeights();
        trainingAccumulator = calloc(1, sizeof(accumulator_training));

        train(saveEveryNBlocks, shouldTrain, 1e-3, trainingAccumulator);
        
        save_trainingWeights();

        load_playingWeights();
        quantizeWeights(trainingNNUE, playerNNUE);
        save_playingWeights();

        free(trainingNNUE);
        free(playerNNUE);

        exit(0);
    }

    bitboard* board;
    if(fenLineNumber > 0)
    {
        board = create_board_from_fen("import/FEN.txt", fenLineNumber);
        if(!board) exit(1);
    }
    else board = create_board();

    
    if(shouldPerft)
    {
        clock_t startTime = clock();
        int result = perft(board, depth, depth, verbose);
        clock_t duration = clock() - startTime;
        double seconds = (double) duration / CLOCKS_PER_SEC;
        double NPS = result / seconds;

        printf("Searched through %d nodes in %f seconds at %f NPS.\n", result, seconds, NPS);
        free(board);
        exit(0);
    }


    if(useBook && !onlyHumans) loadBook();
    if(!onlyHumans) 
    {
        transpositionTable = create_hashTable_tt();
        load_playingWeights();
        playerAccumulator = calloc(1, sizeof(accumulator_playing));
        playingRefreshTable = createPlayingRefreshTable();
        loadInputAccumulator(board, playerAccumulator, PLAYING, WHITE|BLACK);
        tb_init("./sygyzy/");
    }

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
                unmove(board);
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
                move bestMove = calculateBestMove(board, depth, maxTime, PLAYING);
                error = moveFromStruct(board, bestMove);
            }while(error != 0);
            board_print(board, verbose, printHistory);
        }
        
        if(board->victor)
        {
            if(board->victor == VICTOR_WHITE)
            {
                printf("White wins!\n\n");
                break;
            }
            else if(board->victor == VICTOR_BLACK)
            {
                printf("Black wins!\n\n");
                break;
            }
            else
            {
                printf("Draw!");
                if(board->victor == VICTOR_DRAW_STALEMATE_WHITE) printf(" (White stalemated)\n\n");
                else if(board->victor == VICTOR_DRAW_STALEMATE_BLACK) printf(" (Black stalemated)\n\n");
                else if(board->victor == VICTOR_DRAW_THREEFOLD) printf(" (Threefold Repetition)\n\n");
                else if(board->victor == VICTOR_DRAW_FIFTY_MOVE_RULE) printf(" (50-move rule)\n\n");
                else if(board->victor == VICTOR_DRAW_INSUFFICIENT_MATERIAL) printf(" (Insufficient Material)\n\n");
                break;
            }
        }
    }

    if(!onlyHumans)
    {
        free(playerNNUE);
        free(playerAccumulator);
        destroyRefreshTable(playingRefreshTable, PLAYING);
        destroy_hashTable_tt(transpositionTable);
        tb_free();
    }
    free(board);
}