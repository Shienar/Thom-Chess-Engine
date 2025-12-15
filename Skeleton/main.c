#include "moves.h"
#include "bitboard.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * TODO LIST
 *  - Min/max function with alpha/beta pruning.
 *  - Evaluation function
 *      - test initially with piece value counting.
 *      - Neural network
 *      - Monte Carlo Tree Search
 * 
 */

int main(int argc, char** argv)
{
    /**
     * Arguments
     */
    int verbose = 0;
    for(int i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-v") == 0)
        {
            verbose = 1;
        }
    }

    bitboard board = {0};
    board_reset(&board);

    char buffer[6] = {'\0'};
    board_print(&board, 0);

    int error = 0;

    while(1)
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
}