#include "bitboard.h"
#include "moves.h"
#include <stdlib.h>
#include <string.h>
#include "debug.h"

move* generateMoveList(bitboard* board)
{
    move** movesArrayList = calloc(16, sizeof(move));
    int size = 0;
    for(int currentSquare = 0; currentSquare < 64; currentSquare++)
    {
        if((board->turn == WHITE && board->pieces_w&(1ull<<currentSquare)) ||
            (board->turn == BLACK && board->pieces_b&(1ull<<currentSquare)))
        {
            int piece = findPieceOnSquare(board, currentSquare);
            move* tempMoves = generatePieceMoves(board, piece, currentSquare, piece);
            if(tempMoves[0].startSquare != -1)
            {
                movesArrayList[size] = tempMoves;
                size++;
            }
        }
    }

    //Game over, no moves available.
    //stalemate/checkmate gets decided elsewhere.
    if(size == 0) 
    {
        free(movesArrayList);
        return NULL;
    }

    int totalMoveCount = 0;
    for(int i = 0; i < size; i++)
    {
        int j = 0;
        while(movesArrayList[i][j].startSquare != -1) 
        {
            totalMoveCount++;
            j++;
        }
    }

    move* totalMoveList = calloc(totalMoveCount + 1, sizeof(move));
    int nextInsertIndex = 0;
    for(int i = 0; i < size; i++)
    {
        int j = 0;
        while(movesArrayList[i][j].startSquare != -1)
        {
            memcpy(&totalMoveList[nextInsertIndex], &movesArrayList[i][j], sizeof(move));
            j++;
            nextInsertIndex++;
        }
        free(movesArrayList[i]);
    }
    free(movesArrayList);

    move terminatingMove = {0};
    terminatingMove.startSquare = -1;
    totalMoveList[totalMoveCount] = terminatingMove;

    return totalMoveList;
}

move* generatePieceMoves(bitboard* board, int piece, int square, int color)
{
    move* moveArray = calloc(32, sizeof(move));
    int size = 0;
    uint64_t moveMask;
    if(ISPAWN(piece))
    {
        moveMask = pawnMoves(board, square, color);
        int currentSquare = 0;

        while(moveMask)
        {
            if(moveMask&1)
            {
                if((ISWHITE(color) && currentSquare >= 56) || (ISBLACK(color) && currentSquare <= 7))
                {
                    //Promotion (Knight)
                    move newMove_Knight = {0};
                    newMove_Knight.color = color;
                    newMove_Knight.startSquare = square;
                    newMove_Knight.endSquare = currentSquare;
                    newMove_Knight.piece = PAWN;
                    newMove_Knight.promoteTo = KNIGHT;

                    moveArray[size] = newMove_Knight;
                    size++;
                    
                    //Promotion (Bishop)
                    move newMove_Bishop = {0};
                    newMove_Bishop.color = color;
                    newMove_Bishop.startSquare = square;
                    newMove_Bishop.endSquare = currentSquare;
                    newMove_Bishop.piece = PAWN;
                    newMove_Bishop.promoteTo = BISHOP;

                    moveArray[size] = newMove_Bishop;
                    size++;
                    
                    //Promotion (Rook)
                    move newMove_Rook = {0};
                    newMove_Rook.color = color;
                    newMove_Rook.startSquare = square;
                    newMove_Rook.endSquare = currentSquare;
                    newMove_Rook.piece = PAWN;
                    newMove_Rook.promoteTo = ROOK;

                    moveArray[size] = newMove_Rook;
                    size++;
                    
                    //Promotion (Queen)
                    move newMove_Queen = {0};
                    newMove_Queen.color = color;
                    newMove_Queen.startSquare = square;
                    newMove_Queen.endSquare = currentSquare;
                    newMove_Queen.piece = PAWN;
                    newMove_Queen.promoteTo = QUEEN;

                    moveArray[size] = newMove_Queen;
                    size++;

                }
                else
                {
                    move newMove = {0};
                    newMove.color = color;
                    newMove.startSquare = square;
                    newMove.endSquare = currentSquare;
                    newMove.piece = PAWN;

                    moveArray[size] = newMove;
                    size++;
                }
            }
            moveMask = moveMask >> 1;
            currentSquare++;
        }
    }
    else 
    {
        if(ISKNIGHT(piece))
        {
            moveMask = knightMoves(board, square, color);
        }
        else if(ISBISHOP(piece))
        {
            moveMask = bishopMoves(board, square, color);
        }
        else if(ISROOK(piece))
        {
            moveMask = rookMoves(board, square, color); 
        }
        else if(ISQUEEN(piece))
        {
            moveMask = queenMoves(board, square, color);
        }
        else if(ISKING(piece))
        {
            moveMask = kingMoves(board, square, color);
        }
        int currentSquare = 0;

        while(moveMask)
        {
            if(moveMask&1)
            {
                move newMove = {0};
                newMove.color = color;
                newMove.startSquare = square;
                newMove.endSquare = currentSquare;
                newMove.piece = piece;

                moveArray[size] = newMove;
                size++;
            }
            moveMask = moveMask >> 1;
            currentSquare++;
        }
    }
    
    move* legalMoveArray = calloc(size, sizeof(move));
    bitboard tempBoard = {0};
    int legalSize = 0;
    for(int i = 0; i < size; i++)
    {
        memcpy(&tempBoard, board, sizeof(bitboard));
        //Check if move is legal.
        if(movePiece(&tempBoard, moveArray[i].startSquare, moveArray[i].endSquare, moveArray[i].piece|moveArray[i].color, moveArray[i].promoteTo) == 0)
        {
            if((ISWHITE(color) && isThreatened(&tempBoard, tempBoard.kingSquare_w, WHITE)) ||
                (ISBLACK(color) && isThreatened(&tempBoard, tempBoard.kingSquare_b, BLACK)))
            {
                continue;
            }

            legalMoveArray[legalSize] = moveArray[i];
            legalSize++;
        }
    }

    legalMoveArray = realloc(legalMoveArray, (legalSize+1)*sizeof(move));
    if(legalMoveArray == NULL) return NULL;

    move terminatingMove = {0};
    terminatingMove.startSquare = -1;
    legalMoveArray[legalSize] = terminatingMove;

    return legalMoveArray;
}

int isPinned(bitboard* board, int questionedSquare, int kingSquare, int kingColor)
{
    uint64_t rookqueen = 0;
    uint64_t bishopqueen = 0;

    if(ISBLACK(kingColor))
    {
        rookqueen = board->rook_w|board->queen_w;
        bishopqueen = board->bishop_w|board->queen_w;
    }
    else if(ISWHITE(kingColor))
    {
        rookqueen = board->rook_b|board->queen_b;
        bishopqueen = board->bishop_b|board->queen_b;
    }

    if(getRow(questionedSquare) == getRow(kingSquare))
    {
        //Check for discovered rook/queen attack
        if(questionedSquare < kingSquare)
        {
            //King is on a higher column, same row.

            //Check for pieces between questionedSquare and king square.
            uint64_t greaterMask = (1ull << (kingSquare + 1)) - 1;
            uint64_t lesserMask = (1ull << questionedSquare) - 1;
            uint64_t squaresBetweenMask = greaterMask ^ lesserMask;

            if(!(board->pieces_all&squaresBetweenMask))
            {
                //No pieces between questionedSquare and kingSquare
                //Find the closest attacking rook/queen in other direction.
                if(questionedSquare%8 >= 0)
                {
                    int checkedSquare = questionedSquare - 1;
                    while(checkedSquare%8 >= 0)
                    {
                        if(((1ull<<checkedSquare)&rookqueen) != 0)
                        {
                            //Opposing Rook or queen is looking at king across questionedSquare.
                            return PIN_TYPE_LEFT;
                        }
                        else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                        {
                            //Blocking piece
                            break;
                        }
                        checkedSquare--;
                    }
                }
                //Nothing found pinning the questioned square.
                return PIN_TYPE_NONE;
            }
        }
        else
        {
            //King is on a lower column, same row

            //Check for pieces between questionedSquare and king square.
            uint64_t greaterMask = (1ull << (questionedSquare + 1)) - 1;
            uint64_t lesserMask = (1ull << kingSquare) - 1;
            uint64_t squaresBetweenMask = greaterMask ^ lesserMask;

            if(!(board->pieces_all&squaresBetweenMask))
            {
                //Find the closest attacking rook/queen in other direction.
                if(questionedSquare%8 <= 7)
                {
                    int checkedSquare = questionedSquare + 1;
                    while(checkedSquare%8 <= 7)
                    {
                        if(((1ull<<checkedSquare)&rookqueen) != 0)
                        {
                            //White rook or queen is looking at black king.
                            return PIN_TYPE_RIGHT;
                        }
                        else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                        {
                            //Blocking piece
                            break;
                        }
                        checkedSquare++;
                    }
                }
                //Nothing found pinning the questioned square.
                return PIN_TYPE_NONE;
            }
        }
    }
    else if (getColumn(questionedSquare) == getColumn(kingSquare))
    {
        if(questionedSquare < kingSquare)
        {
            //King is above questioned square.

            //Search for blocking pieces between the two squares.
            uint64_t squaresBetweenMask = 0;
            for(int i = questionedSquare + 8; i < kingSquare; i+=8)
            {
                squaresBetweenMask |= (1ull << i);
            }

            if(!(board->pieces_all&squaresBetweenMask))
            {
                //Find the closest attacking rook/queen in other direction.
                if(questionedSquare/8 >= 0)
                {
                    int checkedSquare = questionedSquare - 8;
                    while(checkedSquare/8 >= 0)
                    {
                        if(((1ull<<checkedSquare)&rookqueen) != 0)
                        {
                            //Attacking rook or queen is looking at king across questioned square.
                            return PIN_TYPE_BELOW;
                        }
                        else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                        {
                            //Blocking piece
                            break;
                        }
                        checkedSquare-=8;
                    }
                }
                //Nothing found.
                return PIN_TYPE_NONE;
            }
        }
        else
        {
            //King is below questioned square.

            //Search for blocking pieces between the two squares.
            uint64_t squaresBetweenMask = 0;
            for(int i = kingSquare + 8; i < questionedSquare; i+=8)
            {
                squaresBetweenMask |= (1ull << i);
            }

            if(!(board->pieces_all&squaresBetweenMask))
            {
                //Find the closest attacking rook/queen in other direction.
                if(questionedSquare/8 <= 7)
                {
                    int checkedSquare = questionedSquare + 8;
                    while(checkedSquare/8 <= 7)
                    {
                        if(((1ull<<checkedSquare)&rookqueen) != 0)
                        {
                            //White rook or queen is looking at black king.
                            return PIN_TYPE_ABOVE;
                        }
                        else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                        {
                            //Blocking piece
                            break;
                        }
                        checkedSquare+=8;
                    }
                }
                //Nothing found
                return PIN_TYPE_NONE;
            }
        }
    }
    else
    {
        int startRow = getRow(questionedSquare);
        int kingRow = getRow(kingSquare);
        int startColumn = getColumn(questionedSquare);
        int kingColumn = getColumn(kingSquare);

        //Check if black king is on same diagonal as questionedSquare to check for discovered queen/bishop attack.
        if(abs(startRow - kingRow) == abs(startColumn - kingColumn))
        {
            if(startRow < kingRow)
            {
                if(startColumn < kingColumn)
                {
                    //questionedSquare is to the bottomleft of black king.

                    //Search for blocking pieces between the two squares.
                    uint64_t squaresBetweenMask = 0;
                    for(int i = questionedSquare + 9; i < kingSquare; i+=9)
                    {
                        squaresBetweenMask |= (1ull << i);
                    }

                    if(!(board->pieces_all&squaresBetweenMask))
                    {
                        //Find the closest attacking bishop/queen to the bottomleft.
                        if(questionedSquare >= 9 && getColumn(questionedSquare) > 1)
                        {
                            int checkedSquare = questionedSquare - 9;
                            while(checkedSquare >= 0 && getColumn(checkedSquare) < getColumn(questionedSquare) && getRow(checkedSquare) < getRow(questionedSquare))
                            {
                                if(((1ull<<checkedSquare)&bishopqueen) != 0)
                                {
                                    //White rook or queen is looking at black king.
                                    return PIN_TYPE_DOWNLEFT;
                                }
                                else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                                {
                                    //Blocking piece
                                    break;
                                }
                                checkedSquare-=9;
                            }
                        }
                        //Nothing found
                        return PIN_TYPE_NONE;
                    }
                }
                else
                {
                    //questionedSquare is to the bottomright of black king.

                    //Search for blocking pieces between the two squares.
                    uint64_t squaresBetweenMask = 0;
                    for(int i = questionedSquare + 7; i < kingSquare; i+=7)
                    {
                        squaresBetweenMask |= (1ull << i);
                    }

                    if(!(board->pieces_all&squaresBetweenMask))
                    {
                        //Find the closest attacking bishop/queen to the bottomright.
                        if(questionedSquare >= 8 && getColumn(questionedSquare) < 8) 
                        {
                            int checkedSquare = questionedSquare - 7;
                            while(checkedSquare >= 0 && getColumn(checkedSquare) > getColumn(questionedSquare) && getRow(checkedSquare) < getRow(questionedSquare))
                            {
                                if(((1ull<<checkedSquare)&bishopqueen) != 0)
                                {
                                    //White rook or queen is looking at black king.
                                    return PIN_TYPE_DOWNRIGHT;
                                }
                                else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                                {
                                    //Blocking piece
                                    break;
                                }
                                checkedSquare-=7;
                            }
                        }
                        //Nothing found
                        return PIN_TYPE_NONE;
                    }
                }
            }
            else
            {
                if(startColumn < kingColumn)
                {
                    //questionedSquare is to the topleft of black king.

                    //Search for blocking pieces between the two squares.
                    uint64_t squaresBetweenMask = 0;
                    for(int i = kingSquare + 7; i < questionedSquare; i+=7)
                    {
                        squaresBetweenMask |= (1ull << i);
                    }

                    if(!(board->pieces_all&squaresBetweenMask))
                    {
                        //Find the closest attacking bishop/queen to the topleft.
                        if(questionedSquare < 56 && getColumn(questionedSquare) > 1)
                        {
                            int checkedSquare = questionedSquare + 7;
                            while(checkedSquare <= 63 && getColumn(checkedSquare) < getColumn(questionedSquare) && getRow(checkedSquare) > getRow(questionedSquare))
                            {
                                if(((1ull<<checkedSquare)&bishopqueen) != 0)
                                {
                                    //White rook or queen is looking at black king.
                                    return PIN_TYPE_UPLEFT;
                                }
                                else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                                {
                                    //Blocking piece
                                    break;
                                }
                                checkedSquare+=7;
                            }
                        }
                        //Nothing found
                        return PIN_TYPE_NONE;
                    }
                }
                else
                {
                    //questionedSquare is to the topright of black king.

                    //Search for blocking pieces between the two squares.
                    uint64_t squaresBetweenMask = 0;
                    for(int i = kingSquare + 9; i < questionedSquare; i+=9)
                    {
                        squaresBetweenMask |= (1ull << i);
                    }
                    if(!(board->pieces_all&squaresBetweenMask))
                    {
                        //Find the closest attacking bishop/queen to the topright.
                        if(questionedSquare < 56 && getColumn(questionedSquare) < 8)
                        {
                            int checkedSquare = questionedSquare + 9;
                            while(checkedSquare <= 63 && getColumn(checkedSquare) > getColumn(questionedSquare) && getRow(checkedSquare) > getRow(questionedSquare))
                            {
                                if(((1ull<<checkedSquare)&bishopqueen) != 0)
                                {
                                    //White rook or queen is looking at black king.
                                    return PIN_TYPE_UPRIGHT;
                                }
                                else if(((1ull<<checkedSquare)&board->pieces_all) != 0)
                                {
                                    //Blocking piece
                                    break;
                                }
                                checkedSquare+=9;
                            }
                        }
                        //Nothing found
                        return PIN_TYPE_NONE;
                    }
                }
            }
        }
    }
    return PIN_TYPE_NONE;
}

int isThreatened(bitboard* board, int square, int squareColor)
{
    uint64_t bishopqueen;
    uint64_t rookqueen;
    if(ISWHITE(squareColor))
    {
        bishopqueen = board->bishop_b|board->queen_b;
        rookqueen = board->rook_b|board->queen_b;
        if(((1ull<<(square+7)&board->pawn_b) || (1ull<<(square+9)&board->pawn_b))) return 1;
        if(knightMoves(board, square, squareColor)&(board->knight_b)) return 1;
    }
    else if(ISBLACK(squareColor))
    {
        bishopqueen = board->bishop_w|board->queen_w;
        rookqueen = board->rook_w|board->queen_w;
        if(((1ull<<(square-7)&board->pawn_w) || (1ull<<(square-9)&board->pawn_w))) return 1;
        if(knightMoves(board, square, squareColor)&(board->knight_w)) return 1;
    }
    if(bishopMoves(board, square, squareColor)&(bishopqueen)) return 1;
    if(rookMoves(board, square, squareColor)&(rookqueen)) return 1;

    return 0;
}

uint64_t pawnMoves(bitboard* board, int square, int color)
{
    uint64_t returnValue = 0;

    if(ISWHITE(color))
    {
        //Frontleft capture check
        if(board->pieces_b&(1ull<<(square+7))) returnValue|=(1ull<<(square+7));
        
        //Frontright capture check
        if(board->pieces_b&(1ull<<(square+9))) returnValue|=(1ull<<(square+9));

        //One move forward check
        if(!(board->pieces_all&(1ull<<(square+8))))
        {
            returnValue|=(1ull<<(square+8));

            //Two moves forward check
            if(getRow(square) == 2 && !(board->pieces_all&(1ull<<(square+16)))) returnValue|=(1ull<<(square+16));
        }

        //En passant
        if(ISPAWN(board->lastPieceMove) && abs(square - board->lastSquareMove) == 1 && abs(board->lastSquareMove - board->startOfLastSquareMove) == 16)
        {
            returnValue|=(1ull<<(board->lastSquareMove + 8));
        }
    }
    else
    {
        //Frontleft capture check
        if(board->pieces_w&(1ull<<(square-7))) returnValue|=(1ull<<(square-7));
        
        //Frontright capture check
        if(board->pieces_w&(1ull<<(square-9))) returnValue|=(1ull<<(square-9));

        //One move forward check
        if(!(board->pieces_all&(1ull<<(square-8))))
        {
            returnValue|=(1ull<<(square-8));

            //Two moves forward check
            if(getRow(square) == 7 && !(board->pieces_all&(1ull<<(square-16)))) returnValue|=(1ull<<(square-16));
        }
        
        //En passant
        if(ISPAWN(board->lastPieceMove) && abs(square - board->lastSquareMove) == 1 && abs(board->lastSquareMove - board->startOfLastSquareMove) == 16)
        {
            returnValue|=(1ull<<(board->lastSquareMove - 8));
        }
    }

    return returnValue;
}

uint64_t knightMoves(bitboard* board, int square, int color)
{
    uint64_t returnedValue = 0;
    int row = getRow(square);
    int column = getColumn(square);

    /*
     *      - 2 - 3 -
     *      1 - - - 4
     *      - - N - -
     *      8 - - - 5
     *      - 7 - 6 -
     * 
     *  1: endSquare = square + 6
     *  2: endSquare = square + 15
     *  3: endSquare = square + 17
     *  4: endSquare = square + 10
     *  5: endSquare = square - 6
     *  6: endSquare = square - 15
     *  7: endSquare = square - 17
     *  8: endSquare = square - 10
     */

    if(column - 2 >= 1 && row + 1 <= 8) returnedValue|=(1ull<<(square+6));
    if(column - 1 >= 1 && row + 2 <= 8) returnedValue|=(1ull<<(square+15));
    if(column + 1 <= 8 && row + 2 <= 8) returnedValue|=(1ull<<(square+17));
    if(column + 2 <= 8 && row + 1 <= 8) returnedValue|=(1ull<<(square+10));
    if(column + 2 <= 8 && row - 1 >= 1) returnedValue|=(1ull<<(square-6));
    if(column + 1 <= 8 && row - 2 >= 1) returnedValue|=(1ull<<(square-15));
    if(column - 1 >= 1 && row - 2 >= 1) returnedValue|=(1ull<<(square-17));
    if(column - 2 <= 8 && row - 1 >= 1) returnedValue|=(1ull<<(square-10));

    if(ISWHITE(color))
    {
        returnedValue^=(returnedValue&board->pieces_w);
    }
    else
    {
        returnedValue^=(returnedValue&board->pieces_b);
    }
     
    return returnedValue;
}

uint64_t bishopMoves(bitboard* board, int square, int color)
{
    uint64_t potentialMoves = 0;

    //Topleft
    int questionedSquare = square + 7;
    while(questionedSquare >= 0 && questionedSquare <= 63 && getColumn(questionedSquare) < getColumn(square) && getRow(questionedSquare) > getRow(square))
    {
        uint64_t questionedMask = (1ull<<questionedSquare);
        if((ISWHITE(color) && (board->pieces_b&questionedMask)) || (ISBLACK(color) && (board->pieces_w&questionedMask)))
        {
            //Opposite color target
            potentialMoves|=questionedMask;
            break;
        }
        else if(board->pieces_all&questionedMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=questionedMask;
        }

        questionedSquare+=7;
    }

    //Topright
    questionedSquare = square + 9;
    while(questionedSquare >= 0 && questionedSquare <= 63 && getColumn(questionedSquare) > getColumn(square) && getRow(questionedSquare) > getRow(square))
    {
        uint64_t questionedMask = (1ull<<questionedSquare);
        if((ISWHITE(color) && (board->pieces_b&questionedMask)) || (ISBLACK(color) && (board->pieces_w&questionedMask)))
        {
            //Opposite color target
            potentialMoves|=questionedMask;
            break;
        }
        else if(board->pieces_all&questionedMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=questionedMask;
        }

        questionedSquare+=9;
    }

    //Bottomleft
    questionedSquare = square - 9;
    while(questionedSquare >= 0 && questionedSquare <= 63 && getColumn(questionedSquare) < getColumn(square) && getRow(questionedSquare) < getRow(square))
    {
        uint64_t questionedMask = (1ull<<questionedSquare);
        if((ISWHITE(color) && (board->pieces_b&questionedMask)) || (ISBLACK(color) && (board->pieces_w&questionedMask)))
        {
            //Opposite color target
            potentialMoves|=questionedMask;
            break;
        }
        else if(board->pieces_all&questionedMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=questionedMask;
        }

        questionedSquare-=9;
    }

    //Bottomright
    questionedSquare = square - 7;
    while(questionedSquare >= 0 && questionedSquare <= 63 && getColumn(questionedSquare) > getColumn(square) && getRow(questionedSquare) < getRow(square))
    {
        uint64_t questionedMask = (1ull<<questionedSquare);
        if((ISWHITE(color) && (board->pieces_b&questionedMask)) || (ISBLACK(color) && (board->pieces_w&questionedMask)))
        {
            //Opposite color target
            potentialMoves|=questionedMask;
            break;
        }
        else if(board->pieces_all&questionedMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=questionedMask;
        }

        questionedSquare-=7;
    }

    return potentialMoves;
}

uint64_t rookMoves(bitboard* board, int square, int color)
{
    uint64_t potentialMoves = 0;
    uint64_t tempMask = 0;
    //Above
    for(int column = getColumn(square) + 1; column <= 8; column++)
    {
        tempMask = (1ull<<(square + (column - getColumn(square))));
        if((ISWHITE(color) && board->pieces_b&tempMask) || (ISBLACK(color) && board->pieces_w&tempMask))
        {
            //Opposite color target
            potentialMoves|=tempMask;
            break;
        }
        else if(board->pieces_all&tempMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=tempMask;
        }
    }

    //Below
    for(int column = getColumn(square) - 1; column >= 1; column--)
    {
        tempMask = (1ull<<(square + (column - getColumn(square))));
        if((ISWHITE(color) && board->pieces_b&tempMask) || (ISBLACK(color) && board->pieces_w&tempMask))
        {
            //Opposite color target
            potentialMoves|=tempMask;
            break;
        }
        else if(board->pieces_all&tempMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=tempMask;
        }
    }

    //Right
    for(int row = getRow(square) + 1; row <= 8; row++)
    {
        tempMask = (1ull<<(square + 8*(row - getRow(square))));
        if((ISWHITE(color) && board->pieces_b&tempMask) || (ISBLACK(color) && board->pieces_w&tempMask))
        {
            //Opposite color target
            potentialMoves|=tempMask;
            break;
        }
        else if(board->pieces_all&tempMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=tempMask;
        }
    }

    //Left
    for(int row = getRow(square) - 1; row >= 1; row--)
    {
        tempMask = (1ull<<(square + 8*(row - getRow(square))));
        if((ISWHITE(color) && board->pieces_b&tempMask) || (ISBLACK(color) && board->pieces_w&tempMask))
        {
            //Opposite color target
            potentialMoves|=tempMask;
            break;
        }
        else if(board->pieces_all&tempMask)
        {
            //Same color target
            break;
        }
        else
        {
            //Empty target
            potentialMoves|=tempMask;
        }
    }

    return potentialMoves;
}

uint64_t queenMoves(bitboard* board, int square, int color)
{
    return rookMoves(board, square, color)|bishopMoves(board, square, color);
}

uint64_t kingMoves(bitboard* board, int square, int color)
{
    uint64_t returnedValue = 0;
    int row = getRow(square);
    int column = getColumn(square);

    /*
     *     
     *      1 2 3
     *      4 K 5
     *      6 7 8
     * 
     *  1: endSquare = square + 7
     *  2: endSquare = square + 8
     *  3: endSquare = square + 9
     *  4: endSquare = square - 1
     *  5: endSquare = square + 1
     *  6: endSquare = square - 9
     *  7: endSquare = square - 8
     *  8: endSquare = square - 7
     */
    
    if(column - 1 >= 1 && row + 1 <= 8) returnedValue|=(1ull<<(square+7));
    if(row + 1 <= 8) returnedValue|=(1ull<<(square+8));
    if(column + 1 <= 8 && row + 1 <= 8) returnedValue|=(1ull<<(square+9));
    if(column - 1 >= 1) returnedValue|=(1ull<<(square-1));
    if(column + 1 <= 8) returnedValue|=(1ull<<(square+1));
    if(column - 1 >= 1 && row - 1 >= 1) returnedValue|=(1ull<<(square-9));
    if(row - 1 >= 1) returnedValue|=(1ull<<(square-8));
    if(column + 1 <= 8 && row - 1 >= 1) returnedValue|=(1ull<<(square-7));

    if(ISWHITE(color))
    {
        returnedValue^=(returnedValue&board->pieces_w);
    }
    else
    {
        returnedValue^=(returnedValue&board->pieces_b);
    }
    
    return returnedValue;
}

int movePawn(bitboard *board, int startSquare, int endSquare, int color, int promoteTo)
{
    if(board == NULL)
    {
        DEBUG("Cannot move pawn. Board is NULL")
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Pawn start square %d - out of bounds [0, 63]", startSquare)
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Pawn end square %d - out of bounds [0, 63]", endSquare)
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for pawn move.")
        return -1;
    }

    //Auto-queen
    if(promoteTo == 0) promoteTo = QUEEN;

    int difference = abs(startSquare - endSquare);

    if(difference == 8 || difference == 16)
    {
        //1 or 2 moves forward

        //Set new board positions.
        board_clear_square(board, startSquare, (color|PAWN));

        if(endSquare > 55)
        {
            //Promotion
            board_set(board, endSquare, (color|promoteTo));
        }
        else
        {
            board_set(board, endSquare, (color|PAWN));
        }
    }
    else if(difference == 7 || difference == 9)
    {
        //Diagonal Capture

        //Check for en passant.
        if(ISPAWN(board->lastPieceMove) && (getColumn(endSquare) == getColumn(board->lastSquareMove)) && 
            ((ISWHITE(color) && (board->lastSquareMove - endSquare == -8)) ||
            (ISBLACK(color) && (board->lastSquareMove - endSquare == 8))))
        {
            //Check if target pawn is pinned to turn's king.
            if(ISWHITE(color) && isPinned(board, board->lastSquareMove, board->kingSquare_w, WHITE))
            {
                char kingSquareName[3] = {0};
                getSquareName(board->kingSquare_w, kingSquareName);
                char pawnSquareName[3] = {0};
                getSquareName(board->lastSquareMove, pawnSquareName);
                DEBUG("Cannot capture en-passant. Other pawn on %s (%d) is pinned to white king on %s (%d)", pawnSquareName, board->lastSquareMove, kingSquareName, board->kingSquare_w)
                return -1;
            }
            else if(ISBLACK(color) && isPinned(board, board->lastSquareMove, board->kingSquare_b, BLACK))
            {
                char kingSquareName[3] = {0};
                getSquareName(board->kingSquare_b, kingSquareName);
                char pawnSquareName[3] = {0};
                getSquareName(board->lastSquareMove, pawnSquareName);
                DEBUG("Cannot capture en-passant. Other pawn on %s (%d) is pinned to black king on %s (%d)", pawnSquareName, board->lastSquareMove, kingSquareName, board->kingSquare_b)
                return -1;
            }

            //Check if capturing other pawn reveals a discovered check.
            if(ISWHITE(color) && isPinned(board, board->lastSquareMove, board->kingSquare_b, BLACK))
            {
                board->in_check_b = 1;
            }
            else if(ISBLACK(color) && isPinned(board, board->lastSquareMove, board->kingSquare_w, WHITE))
            {
                board->in_check_w = 1;
            }

            //En passant capture
            board_clear_square(board, board->lastSquareMove, (PAWN));
        }

        //Set new board positions.
        board_clear_square(board, startSquare, (color|PAWN));

        if(endSquare > 55)
        {
            //Promotion
            board_set(board, endSquare, (color|promoteTo));
        }
        else
        {
            board_set(board, endSquare, (color|PAWN));
        }      
    }

    //Calculate direct check attacks.
    if(ISWHITE(color) && board->king_b&(pawnMoves(board, endSquare, WHITE)))
    {
        board->in_check_b = 1;
    }
    else if(ISBLACK(color) && board->king_w&(pawnMoves(board, endSquare, BLACK)))
    {
        board->in_check_w = 1;
    }

    return 0;
}

int moveKnight(bitboard *board, int startSquare, int endSquare, int color)
{

    if(board == NULL)
    {
        DEBUG("Cannot move knight. Board is NULL")
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Knight start square %d - out of bounds [0, 63]", startSquare)
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Knight end square %d - out of bounds [0, 63]", endSquare)
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for knight move.")
        return -1;
    }

    //Set new board positions after ensuring position is valid.
    board_clear_square(board, startSquare, (color|KNIGHT));
    board_set(board, endSquare, (color|KNIGHT));

    //Calculate direct check attacks.
    if(ISWHITE(color) && board->king_b&(knightMoves(board, endSquare, WHITE)))
    {
        board->in_check_b = 1;
    }
    else if(ISBLACK(color) && board->king_w&(knightMoves(board, endSquare, BLACK)))
    {
        board->in_check_w = 1;
    }

    return 0;
}

int moveBishop(bitboard *board, int startSquare, int endSquare, int color)
{
    if(board == NULL)
    {
        DEBUG("Cannot move bishop. Board is NULL")
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Bishop start square %d - out of bounds [0, 63]", startSquare)
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Bishop end square %d - out of bounds [0, 63]", endSquare)
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for bishop move.")
        return -1;
    }
    
    board_clear_square(board, startSquare, (color|BISHOP));
    board_set(board, endSquare, (color|BISHOP));

    //Calculate direct check attacks.
    if(ISWHITE(color) && board->king_b&(bishopMoves(board, endSquare, WHITE)))
    {
        board->in_check_b = 1;
    }
    else if(ISBLACK(color) && board->king_w&(bishopMoves(board, endSquare, BLACK)))
    {
        board->in_check_w = 1;
    }

    return 0;
}

int moveRook(bitboard *board, int startSquare, int endSquare, int color)
{
    if(board == NULL)
    {
        DEBUG("Cannot move rook. Board is NULL")
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Rook start square %d - out of bounds [0, 63]", startSquare)
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Rook end square %d - out of bounds [0, 63]", endSquare)
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for rook move.")
        return -1;
    }

    board_clear_square(board, startSquare, (color|ROOK));
    board_set(board, endSquare, (color|ROOK));

    if(ISWHITE(color))
    {
        if(startSquare == 0 && board->canQueensideCastle_w) 
        {
            board->canQueensideCastle_w = 0;
        }
        else if(startSquare == 7 && board->canKingsideCastle_w) 
        {
            board->canKingsideCastle_w = 0;
        }

        if(board->king_b&(rookMoves(board, endSquare, WHITE))) board->in_check_b = 1;
    }
    else if(ISBLACK(color))
    {
        if(startSquare == 56 && board->canQueensideCastle_b) 
        {
            board->canQueensideCastle_b = 0;
        }
        else if(startSquare == 63 && board->canKingsideCastle_b) 
        {
            board->canKingsideCastle_b = 0;
        }

        if(board->king_w&(rookMoves(board, endSquare, BLACK))) board->in_check_w = 1;
    }
    
    return 0;
}

int moveQueen(bitboard *board, int startSquare, int endSquare, int color)
{
    if(board == NULL)
    {
        DEBUG("Cannot move queen. Board is NULL")
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Queen start square %d - out of bounds [0, 63]", startSquare)
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Queen end square %d - out of bounds [0, 63]", endSquare)
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for queen move.")
        return -1;
    }

    board_clear_square(board, startSquare, (color|QUEEN));
    board_set(board, endSquare, (color|QUEEN));

    //Calculate direct check attacks.
    if(ISWHITE(color) && board->king_b&(queenMoves(board, endSquare, WHITE)))
    {
        board->in_check_b = 1;
    }
    else if(ISBLACK(color) && board->king_w&(queenMoves(board, endSquare, BLACK)))
    {
        board->in_check_w = 1;
    }

    return 0;
}

int moveKing(bitboard *board, int startSquare, int endSquare, int color)
{
    if(board == NULL)
    {
        DEBUG("Cannot move king. Board is NULL")
        return -1;
    }
    else if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("King start square %d - out of bounds [0, 63]", startSquare)
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("King end square %d - out of bounds [0, 63]", endSquare)
        return -1;
    } 
    else if(!ISBLACK(color) && !ISWHITE(color))
    {
        DEBUG("Invalid color for king move.")
        return -1;
    }

    if(isThreatened(board, endSquare, color))
    {
        char endSquareName[3] = {0};
        getSquareName(endSquare, endSquareName);
        DEBUG("King cannot reach defended square %s (%d)", endSquareName, endSquare)
        return -1;
    }

    if(ISBLACK(color)) 
    {
        board->kingSquare_b = endSquare;

        if(board->canQueensideCastle_b || board->canKingsideCastle_b)
        {
            board->canQueensideCastle_b = 0;
            board->canKingsideCastle_b = 0;
        }
    }
    else
    {
        board->kingSquare_w = endSquare;

        if(board->canQueensideCastle_w || board->canKingsideCastle_w)
        {
            board->canQueensideCastle_w = 0;
            board->canKingsideCastle_w = 0;
        }
    }

    board_clear_square(board, startSquare, (color|KING));
    board_set(board, endSquare, (color|KING));

    return 0;
}

int movePiece(bitboard *board, int startSquare, int endSquare, int piece, int promoteTo)
{
    int error = 0;
    if(ISPAWN(piece))
    {
        error = movePawn(board, startSquare, endSquare, piece&0xF0, promoteTo);
    }
    else if(ISKNIGHT(piece))
    {
        error = moveKnight(board, startSquare, endSquare, piece&0xF0);
    }
    else if(ISBISHOP(piece))
    {
        error = moveBishop(board, startSquare, endSquare, piece&0xF0);
    }
    else if(ISROOK(piece))
    {
        error = moveRook(board, startSquare, endSquare, piece&0xF0);
    }
    else if(ISQUEEN(piece))
    {
        error = moveQueen(board, startSquare, endSquare, piece&0xF0);
    }
    else if(ISKING(piece))
    {
        error = moveKing(board, startSquare, endSquare, piece&0xF0);
    }
    else
    {
        DEBUG("Attempted to move invalid piece type.")
        return -1;
    }
    
    board->startOfLastSquareMove = startSquare;
    board->lastSquareMove = endSquare;
    board->lastPieceMove = piece;

    return error;
}

int moveFromString(bitboard* board, char* str)
{
    //String format: [2 char - startsquare][2 char - endsquare][1 char - promotion (q, n, r, b)]
    char start[3] = {'\0'};
    strncpy(start, str, 2);
    char end[3] = {'\0'};
    strncpy(end, str + 2, 2);
    char promotion = str[4];

    int startSquare = getSquareNumber(start);
    int endSquare = getSquareNumber(end);
    int promoteTo = 0;
    int piece = findPieceOnSquare(board, startSquare);
    if(!piece)
    {
        DEBUG("Could not find piece on start square.")
        return -1;
    }
    
    if(ISBLACK(piece) && board->turn == WHITE)
    {
        DEBUG("Attempted to move black piece on white's turn.")
        return -1;
    }
    else if(ISWHITE(piece) && board->turn == BLACK)
    {
        DEBUG("Attempted to move white piece on black's turn.")
        return -1;
    }

    move* potentialMoveList = generatePieceMoves(board, piece, startSquare, piece&0xF0);
    if(potentialMoveList == NULL)
    {
        DEBUG("Failed to generate potential moves.")
        return -1;
    }
    int moveIndex = 0;
    int isLegal = 0;
    while(potentialMoveList[moveIndex].startSquare != -1)
    { 
        if(potentialMoveList[moveIndex].startSquare == startSquare && potentialMoveList[moveIndex].endSquare == endSquare)
        {
            isLegal = 1;
            //break;
        }
        moveIndex++;
    }
    free(potentialMoveList);
    if(!isLegal)
    {
        DEBUG("Piece move is not legal.")
        return -1;
    }

    /*
    //Players aren't allowed to move pinned pieces out of the pin.
    if((board->turn == WHITE && isPinned(board, startSquare, board->kingSquare_w, WHITE) && isPinned(board, startSquare, board->kingSquare_w, WHITE) != isPinned(board, endSquare, board->kingSquare_w, WHITE)) ||
        (board->turn == BLACK && isPinned(board, startSquare, board->kingSquare_b, BLACK) &&  isPinned(board, startSquare, board->kingSquare_b, BLACK) != isPinned(board, endSquare, board->kingSquare_b, BLACK)))
    {
        DEBUG("Cannot move piece on %s (%d) out of a pin.", start, startSquare)
        return -1;
    }
    */

    switch(promotion)
    {
        case 'q':
            promoteTo = QUEEN;
            break;
        case 'r':
            promoteTo = ROOK;
            break;
        case 'n':
            promoteTo = KNIGHT;
            break;
        case 'b':
            promoteTo = BISHOP;
            break;        
        default:
            break;
    }
    if(movePiece(board, startSquare, endSquare, piece, promoteTo)) return -1;

    //Calculate discovered checks and change turn.
    if(board->turn == WHITE)
    {
        if(board->in_check_w) board->in_check_w = 0;
        if(isPinned(board, startSquare, board->kingSquare_b, BLACK)) board->in_check_b = 1;
        board->turn=BLACK;
    }
    else
    {
        if(board->in_check_b) board->in_check_b = 0;
        if(isPinned(board, startSquare, board->kingSquare_w, WHITE)) board->in_check_w = 1;
        board->turn=WHITE;
    }

    //Calculate checkmate/stalemate
    move* moveList = generateMoveList(board);
    if(!moveList)
    {
        if(board->turn == WHITE)
        {
            if(board->in_check_w)
            {
                //White has been checkmated
                printf("\nBlack wins!\n\n");
            }
            else
            {
                //White has been stalemated
                printf("\nDraw! (White stalemated)\n\n");
            }
        }
        else
        {
            if(board->in_check_b)
            {
                //Black has been checkmated
                printf("\nWhite wins!\n\n");
            }
            else
            {
                //Black has been stalemated
                printf("\nDraw! (Black stalemated)\n\n");
            }
        }
        free(moveList);
        exit(0);
    }
    free(moveList);

    return 0;
}