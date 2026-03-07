#include "../include/moves.h"
#include "../include/debug.h"
#include "../include/bitboard.h"
#include "../include/transpositiontable.h"
#include "../include/book.h"
#include "../include/neuralnet.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

/**
 * GOALS:
 *  - PeSTO's Evaluation function is a placeholder that should eventually be replaced with a NNUE.
 * 
 * TODO:
 *  - Neural nets
 *  - Endgame tablebase (Probe Syzygy)
 */

int main(int argc, char** argv)
{
    load_network();
    exit(1);
    /**
     * Arguments
     */
    int verbose = 0;
    int player_color = WHITE;
    int depth = 6;
    int onlyEngines = 0;
    int onlyHumans = 0;
    double maxTime = 3;
    int printHistory = 0;
    int fenLineNumber = -1;
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--help") == 0)
        {
            printf("\n\n");
            printf("--help\t\tPrints out this message\n");
            printf("-v\t\tVerbose board information\n");
            printf("--black\t\tPlay as black\n");
            printf("--depth\t\tChange the depth, takes one integer parameter\n");
            printf("--debug\t\tEnable debug messages\n");
            printf("--leaks\t\tTrack memory leaks. Heavily reduces performance\n");
            printf("--human\t\tHuman v human game\n");
            printf("--history\tPrint's recent moves\n");
            printf("--engine\t\tEngine v Engine game\n");
            printf("--time\t\tSpecify maximum computation time per turn in seconds.\n");
            printf("--fen\t\tLoad a fen position from file. Specify the line number\n");
            printf("\n\n");
            exit(0);
        }
        else if(strcmp(argv[i], "-v") == 0)
        {
            verbose = 1;
        }
        else if(strcmp(argv[i], "--black") == 0)
        {
            player_color = BLACK;
        }
        else if(strcmp(argv[i], "--depth") == 0)
        {
            i++;
            depth = atoi(argv[i]);
        }
        else if(strcmp(argv[i], "--debug") == 0)
        {
            enableDebugMessages();
        }
        else if(strcmp(argv[i], "--leaks") == 0)
        {
            enableLeakTracking();
        }
        else if(strcmp(argv[i], "--history") == 0)
        {
            printHistory = 1;
        }
        else if(strcmp(argv[i], "--human") == 0)
        {
            onlyHumans = 1;
            onlyEngines = 0;
        }
        else if(strcmp(argv[i], "--engine") == 0)
        {
            onlyHumans = 0;
            onlyEngines = 1;
        }
        else if(strcmp(argv[i], "--time") == 0)
        {
            i++;
            maxTime = atof(argv[i]);
        }
        else if(strcmp(argv[i], "--fen") == 0)
        {
            i++;
            fenLineNumber = atoi(argv[i]);
        }
    }
    
    srand(time(NULL));

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
    else
    {
        board = create_board();
    }

    init_tables();
    loadBook();
    transpositionTable = create_hashTable_tt();

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

    destroy_hashTable_tt(transpositionTable);
    destroy_board(board);
    dump_allocations();
}