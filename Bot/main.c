#include "moves.h"
#include "bitboard.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"

/**
 * To-do list: 
 *  - pawn on a4 can capture h2
 *  - engine can refuse promotion
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
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-v") == 0)
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
                move bestMove = calculateBestMove(&board, &boardWeights, depth);
                error = moveFromStruct(&board, bestMove);
            }while(error != 0);
            board_print(&board, verbose);
        }
        
    }
}