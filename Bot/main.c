#include "moves.h"
#include "bitboard.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include <windows.h>

/**
 * To-do list: 
 *  - Freeing list crashes only within engine.c alpha-beta code.
 *  - three-fold repetition
 *      - hash table of previous moves.
 */

int main(int argc, char** argv)
{
    /**
     * Arguments
     */
    int verbose = 0;
    int player_color = WHITE;
    int depth = 6;
    int onlyEngines = 0;
    int onlyHumans = 0;
    int threadCount = 1;
    double delay = 0;
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--help") == 0)
        {
            printf("\n\n");
            printf("--help\t\tPrints out this message\n");
            printf("-v\t\tVerbose board information\n");
            printf("--black\t\tPlay as black\n");
            printf("--depth\t\tChange the depth, takes one integer parameter\n");
            printf("-h\t\tHuman v human game\n");
            printf("-e\t\tEngine v Engine game\n");
            printf("--threads\tChange the number of threads for min/max search, takes one integer parameter, max 4.\n");
            printf("--delay\t\tSpecify the number of milliseconds the computer will wait after each move.\n");
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
        else if(strcmp(argv[i], "-h") == 0)
        {
            onlyHumans = 1;
            onlyEngines = 0;
        }
        else if(strcmp(argv[i], "-e") == 0)
        {
            onlyHumans = 0;
            onlyEngines = 1;
        }
        else if(strcmp(argv[i], "--threads") == 0)
        {
            i++;
            threadCount = atoi(argv[i]);
            threadCount = min(threadCount, 4);
        }
        else if(strcmp(argv[i], "--delay") == 0)
        {
            i++;
            delay = atof(argv[i]);
        }
    }

    bitboard board = {0};
    board_reset(&board);

    weights boardWeights = {0};
    initPieceWeights(&boardWeights, 0);

    char buffer[6] = {'\0'};
    int error = 0;

    board_print(&board, 0);

    while(1)
    {
        if(onlyHumans || (board.turn == player_color && !onlyEngines))
        {
            fgets(buffer, 6, stdin);
            if(buffer[0] == 'q')
            {
                printf("Exiting program...");
                exit(1);
            }
            error = moveFromString(&board, buffer);
            if(!error) board_print(&board, verbose);
        }
        else
        {   
            do
            {
                move* bestMove = calculateBestMove(&board, &boardWeights, depth, threadCount);
                error = moveFromStruct(&board, bestMove);
            }while(error != 0);
            board_print(&board, verbose);
            if(delay) Sleep(delay);
        }
        
        if(board.victor == WHITE)
        {
            printf("White wins!\n\n");
            break;
        }
        else if(board.victor == BLACK)
        {
            printf("Black wins!\n\n");
            break;
        }
        else if (board.victor == (WHITE|BLACK))
        {
            printf("Draw!\n\n");
            break;
        }
    }
}