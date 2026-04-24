#include "../board/bitboard.h"
#include "../board/moves.h" 
#include <string.h>
#include "../debug.h"

int generateMoveList(move* movesList, bitboard* board, int capturesOnly)
{
    if(board->victor) 
    {
        DEBUG("Cannot generate moves for terminated game; Victor=x%02x", board->victor);
        return 0;
    }

    int size = 0;
    generatePawnMoves(movesList, &size, board, capturesOnly);
    uint64_t allies, enemies, knights, bishop, rook, queen, king;
    if(ISWHITE(board->turn))
    {
        allies = board->pieces_w;
        enemies = board->pieces_b;
        knights = board->knight_w;
        bishop = board->bishop_w;
        rook = board->rook_w;
        queen = board->queen_w;
        king = board->king_w;
    }
    else
    {
        allies = board->pieces_b;
        enemies = board->pieces_w;
        knights = board->knight_b;
        bishop = board->bishop_b;
        rook = board->rook_b;
        queen = board->queen_b;
        king = board->king_b;
    }

    while(knights) 
    {
        int startSquare = __builtin_ctzll(knights);
        uint64_t mask = knightMoves(allies, startSquare);
        if (capturesOnly) mask &= enemies;
        
        while(mask) 
        {
            int endSquare = __builtin_ctzll(mask);
            int target = findPieceOnSquare(board, endSquare); 
            createMove(&movesList[size++], startSquare, endSquare, 0, (KNIGHT | board->turn), target, (target ? endSquare : 0));
            mask&=(mask-1);
        }
        knights&=(knights-1);
    }

    while(bishop) 
    {
        int startSquare = __builtin_ctzll(bishop);
        uint64_t moveMask = bishopMoves(allies, enemies, startSquare);
        if (capturesOnly) moveMask &= enemies;

        while(moveMask) 
        {
            int endSquare = __builtin_ctzll(moveMask);
            int target = findPieceOnSquare(board, endSquare);
            createMove(&movesList[size++], startSquare, endSquare, 0, (BISHOP | board->turn), target, (target ? endSquare : 0));
            moveMask&=(moveMask-1);
        }
        bishop&=(bishop-1);
    }

    while(rook) 
    {
        int startSquare = __builtin_ctzll(rook);
        uint64_t moveMask = rookMoves(allies, enemies, startSquare); 
        if (capturesOnly) moveMask &= enemies;

        while(moveMask) 
        {
            int endSquare = __builtin_ctzll(moveMask);
            int target = findPieceOnSquare(board, endSquare);
            createMove(&movesList[size++], startSquare, endSquare, 0, (ROOK | board->turn), target, (target ? endSquare : 0));
            moveMask&=(moveMask-1);
        }
        rook&=(rook-1);
    }
    
    while(queen) 
    {
        int startSquare = __builtin_ctzll(queen);
        uint64_t moveMask = queenMoves(allies, enemies, startSquare); 
        if (capturesOnly) moveMask &= enemies;

        while(moveMask) 
        {
            int endSquare = __builtin_ctzll(moveMask);
            int target = findPieceOnSquare(board, endSquare);
            createMove(&movesList[size++], startSquare, endSquare, 0, (QUEEN | board->turn), target, (target ? endSquare : 0));
            moveMask&=(moveMask-1);
        }
        queen&=(queen-1);
    }

    int startSquare = __builtin_ctzll(king);
    uint64_t moveMask = kingMoves(board, startSquare, board->turn);
    if (capturesOnly) moveMask &= enemies;
    while(moveMask) 
    {
        int endSquare = __builtin_ctzll(moveMask);
        int target = findPieceOnSquare(board, endSquare);
        createMove(&movesList[size++], startSquare, endSquare, 0, (KING | board->turn), target, (target ? endSquare : 0));
        moveMask&=(moveMask-1);
    }
    
    return size;
}

void generatePawnMoves(move* moveList, int* size, bitboard* board, int capturesOnly)
{
    assert(board);
    
    uint64_t mask = 0;
    int piece = PAWN|(board->turn);

    if(ISWHITE(piece))
    {
        if(!capturesOnly)
        {
            mask = WHITE_PAWN_PUSH_MASK(board);
            while(mask)
            {
                int endSquare = __builtin_ctzll(mask);
                int startSquare = endSquare - 8;

                if(endSquare >= 56)
                {
                    //Promotion
                    createMove(&moveList[(*size)++], startSquare, endSquare, KNIGHT, piece, 0, 0);
                    createMove(&moveList[(*size)++], startSquare, endSquare, BISHOP, piece, 0, 0); 
                    createMove(&moveList[(*size)++], startSquare, endSquare, ROOK, piece, 0, 0);     
                    createMove(&moveList[(*size)++], startSquare, endSquare, QUEEN, piece, 0, 0);
                }
                else createMove(&moveList[(*size)++], startSquare, endSquare, 0, piece, 0, 0);

                mask&=(mask - 1);
            }

            mask = WHITE_PAWN_DOUBLEPUSH_MASK(board);
            while(mask)
            {
                int endSquare = __builtin_ctzll(mask);
                int startSquare = endSquare - 16;

                createMove(&moveList[(*size)++], startSquare, endSquare, 0, piece, 0, 0);
                
                mask&=(mask - 1);
            }
        }

        mask = WHITE_PAWN_LEFTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare - 7;
            int capturedPiece = findPieceOnSquare(board, endSquare);

            if(endSquare >= 56)
            {
                //Promotion
                createMove(&moveList[(*size)++], startSquare, endSquare, KNIGHT, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, BISHOP, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, ROOK, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, QUEEN, piece, capturedPiece, endSquare);
            }
            else createMove(&moveList[(*size)++], startSquare, endSquare, 0, piece, capturedPiece, endSquare);
            
            mask&=(mask - 1);
        }

        mask = WHITE_PAWN_RIGHTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare - 9;
            int capturedPiece = findPieceOnSquare(board, endSquare);

            if(endSquare >= 56)
            {
                //Promotion
                createMove(&moveList[(*size)++], startSquare, endSquare, KNIGHT, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, BISHOP, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, ROOK, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, QUEEN, piece, capturedPiece, endSquare);
            }
            else createMove(&moveList[(*size)++], startSquare, endSquare, 0, piece, capturedPiece, endSquare);
            
            mask&=(mask - 1);
        }

        
        if(board->enPassantSquare != -1)
        {
            uint64_t epMask = 1ull << board->enPassantSquare;
            mask = EN_PASSANT_ATTACKERS_WHITE(epMask, board);
            while(mask)
            {
                int startSquare = __builtin_ctzll(mask);
                createMove(&moveList[(*size)++], startSquare, board->enPassantSquare, 0, piece, PAWN|BLACK, board->enPassantSquare - 8);
                mask&=(mask - 1);
            }
        }

    }
    else
    {
        if(!capturesOnly)
        {
            mask = BLACK_PAWN_PUSH_MASK(board);
            while(mask)
            {
                int endSquare = __builtin_ctzll(mask);
                int startSquare = endSquare + 8;

                if(endSquare <= 7)
                {
                    //Promotion
                    createMove(&moveList[(*size)++], startSquare, endSquare, KNIGHT, piece, 0, 0);
                    createMove(&moveList[(*size)++], startSquare, endSquare, BISHOP, piece, 0, 0); 
                    createMove(&moveList[(*size)++], startSquare, endSquare, ROOK, piece, 0, 0);     
                    createMove(&moveList[(*size)++], startSquare, endSquare, QUEEN, piece, 0, 0);
                }
                else createMove(&moveList[(*size)++], startSquare, endSquare, 0, piece, 0, 0);

                mask&=(mask - 1);
            }

            mask = BLACK_PAWN_DOUBLEPUSH_MASK(board);
            while(mask)
            {
                int endSquare = __builtin_ctzll(mask);
                int startSquare = endSquare + 16;

                createMove(&moveList[(*size)++], startSquare, endSquare, 0, piece, 0, 0);
                
                mask&=(mask - 1);
            }
        }

        mask = BLACK_PAWN_LEFTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare + 7;
            int capturedPiece = findPieceOnSquare(board, endSquare);

            if(endSquare <= 7)
            {
                //Promotion
                createMove(&moveList[(*size)++], startSquare, endSquare, KNIGHT, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, BISHOP, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, ROOK, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, QUEEN, piece, capturedPiece, endSquare);
            }
            else
            {
                createMove(&moveList[(*size)++], startSquare, endSquare, 0, piece, capturedPiece, endSquare);
            }
            
            mask&=(mask - 1);
        }

        mask = BLACK_PAWN_RIGHTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare + 9;
            int capturedPiece = findPieceOnSquare(board, endSquare);

            if(endSquare <= 7)
            {
                //Promotion
                createMove(&moveList[(*size)++], startSquare, endSquare, KNIGHT, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, BISHOP, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, ROOK, piece, capturedPiece, endSquare);
                createMove(&moveList[(*size)++], startSquare, endSquare, QUEEN, piece, capturedPiece, endSquare);
            }
            else
            {
                createMove(&moveList[(*size)++], startSquare, endSquare, 0, piece, capturedPiece, endSquare);
            }
            
            mask&=(mask - 1);
        }

        
        if(board->enPassantSquare != -1)
        {
            uint64_t epMask = 1ull << board->enPassantSquare;
            mask = EN_PASSANT_ATTACKERS_BLACK(epMask, board);
            while(mask)
            {
                int startSquare = __builtin_ctzll(mask);
                createMove(&moveList[(*size)++], startSquare, board->enPassantSquare, 0, piece, PAWN|WHITE, board->enPassantSquare + 8);
                mask&=(mask - 1);
            }
        }
    }
}

int isThreatened(bitboard* board, int square, int squareColor)
{
    uint64_t bishopqueen = 0;
    uint64_t rookqueen = 0;
    uint64_t enemyPieces = 0;
    uint64_t allyPieces = 0;
    if(ISWHITE(squareColor))
    {
        enemyPieces = board->pieces_b;
        allyPieces = board->pieces_w;
        bishopqueen = board->bishop_b|board->queen_b;
        rookqueen = board->rook_b|board->queen_b;
        if(square < 48 && pawnAttacks[0][square]&board->pawn_b) return THREAT_TYPE_PAWN;
        if(knightAttacks[square]&(board->knight_b)) return THREAT_TYPE_KNIGHT;
        if(kingAttacks[square]&(board->king_b)) return THREAT_TYPE_KING;
    }
    else if(ISBLACK(squareColor))
    {
        enemyPieces = board->pieces_w;
        allyPieces = board->pieces_b;
        bishopqueen = board->bishop_w|board->queen_w;
        rookqueen = board->rook_w|board->queen_w;
        if(square > 15 && pawnAttacks[1][square]&board->pawn_w) return THREAT_TYPE_PAWN;
        if(knightAttacks[square]&(board->knight_w)) return THREAT_TYPE_KNIGHT;
        if(kingAttacks[square]&(board->king_w)) return THREAT_TYPE_KING;
    }
    if(bishopMoves(allyPieces, enemyPieces, square)&(bishopqueen)) return THREAT_TYPE_BISHOPQUEEN;
    if(rookMoves(allyPieces, enemyPieces, square)&(rookqueen)) return THREAT_TYPE_ROOKQUEEN;

    return THREAT_TYPE_NONE;
}

uint64_t pyrrhicPawnAttacks(int square, int color)
{
    color = (color == 1) ? 0 : 1;
    return pawnAttacks[color][square];
}

uint64_t knightMoves(uint64_t allyPieces, int square)
{
    return knightAttacks[square]&(~allyPieces);
}

uint64_t bishopMoves(uint64_t allyPieces, uint64_t enemyPieces, int square)
{
    uint64_t occupancy = (allyPieces|enemyPieces) & bishopMagics[square].mask;
    return bishopMagics[square].attacks[(occupancy * bishopMagics[square].magic) >> bishopMagics[square].shiftOffset] & (~allyPieces);
}

uint64_t rookMoves(uint64_t allyPieces, uint64_t enemyPieces, int square)
{
    uint64_t occupancy = (allyPieces|enemyPieces) & rookMagics[square].mask;
    return rookMagics[square].attacks[(occupancy * rookMagics[square].magic) >> rookMagics[square].shiftOffset] & (~allyPieces);
}

uint64_t queenMoves(uint64_t allyPieces, uint64_t enemyPieces, int square)
{
    return rookMoves(allyPieces, enemyPieces, square)|bishopMoves(allyPieces, enemyPieces, square);
}

uint64_t pyrrhicKingAttacks(int square)
{
    return kingAttacks[square];
}

uint64_t kingMoves(bitboard* board, int square, int color)
{
    uint64_t returnedValue = kingAttacks[square];

    if(ISWHITE(color))
    {
        returnedValue&=(~board->pieces_w);

        //uint64_t betweenMask = 0x60; //Squares 5 and 6 between king on 4 and rook on 7

        //White kingside
        if((board->flags&1) && !(board->flags&16) && !(board->pieces_all&0x60) && 
                                                        !isThreatened(board, square, color) && 
                                                        !isThreatened(board, square+1, color) && 
                                                        !isThreatened(board, square+2, color)) returnedValue|=0x40;

        //betweenMask = 0xE; //Square 1 and 2 and 3 between rook on 0 and king on 4

        //White queenside
        if((board->flags&2) && !(board->flags&16) && !(board->pieces_all&0xE) && 
                                                        !isThreatened(board, square, color) && 
                                                        !isThreatened(board, square-1, color) && 
                                                        !isThreatened(board, square-2, color)) returnedValue|=0x4;
        
    }
    else
    {
        returnedValue&=(~board->pieces_b);

        //uint64_t betweenMask = 0x6000000000000000; //Squares 61 and 62 between king on 60 and rook on 63

        //Black kingside
        if((board->flags&4) && !(board->flags&32) && !(board->pieces_all&0x6000000000000000) && 
                                                            !isThreatened(board, square, color) &&
                                                            !isThreatened(board, square+1, color) && 
                                                            !isThreatened(board, square+2, color)) returnedValue|=0x4000000000000000;

        //betweenMask = 0x0E00000000000000; //Square 57 and 58 and 59 between rook on 56 and king on 60
        //Black queenside
        if((board->flags&8) && !(board->flags&32) && !(board->pieces_all&0x0E00000000000000) &&
                                                            !isThreatened(board, square, color) &&
                                                            !isThreatened(board, square-1, color) && 
                                                            !isThreatened(board, square-2, color)) returnedValue|=0x0400000000000000;
    }
    
    return returnedValue;
}

int movePawn(bitboard *board, int startSquare, int endSquare, int color, int promoteTo)
{
    assert(board);
    assert(ISWHITE(color) || ISBLACK(color));
    if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Pawn start square %d - out of bounds [0, 63]", startSquare);
        abort();
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Pawn end square %d - out of bounds [0, 63]", endSquare);
        abort();
    }

    //Auto-queen
    if(promoteTo == 0) promoteTo = QUEEN;

    int difference = abs(startSquare - endSquare);

    
    if(difference == 8 || difference == 16)
    {
        //1 or 2 moves forward

        //Set new board positions.
        board_clear_square(board, startSquare);

        if((ISWHITE(color) && endSquare > 55) || (ISBLACK(color) && endSquare < 8))
        {
            //Promotion
            board_set(board, endSquare, (color|promoteTo));
        }
        else
        {
            board_set(board, endSquare, (color|PAWN));
        }
        
        //Update en passant square if necessary.
        if(difference == 16)
        {
            if(ISWHITE(color)) 
            {
                board->hashCode ^= getEnPassantHash(board);
                board->enPassantSquare = endSquare - 8;
                board->hashCode ^= getEnPassantHash(board);
            }
            else 
            {
                board->hashCode ^= getEnPassantHash(board);
                board->enPassantSquare = endSquare + 8;
                board->hashCode ^= getEnPassantHash(board);
            }
        }
    }
    else if(difference == 7 || difference == 9)
    {
        //Diagonal Capture

        //Check for en passant.
        if(endSquare == board->enPassantSquare)
        {
            if(ISWHITE(color)) board_clear_square(board, board->enPassantSquare - 8);
            else board_clear_square(board, board->enPassantSquare + 8);
        }

        //Set new board positions.
        board_clear_square(board, startSquare);

        if((ISWHITE(color) && endSquare > 55) || (ISBLACK(color) && endSquare < 8))
        {
            //Promotion
            board_set(board, endSquare, (color|promoteTo));
        }
        else
        {
            board_set(board, endSquare, (color|PAWN));
        }      
    }

    return 0;
}

int moveKnight(bitboard *board, int startSquare, int endSquare, int color)
{
    assert(board);
    assert(ISBLACK(color) || ISWHITE(color));
    if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Knight start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Knight end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    }

    //Set new board positions after ensuring position is valid.
    board_clear_square(board, startSquare);
    board_set(board, endSquare, (color|KNIGHT));
 
    return 0;
}

int moveBishop(bitboard *board, int startSquare, int endSquare, int color)
{
    assert(board);
    assert(ISBLACK(color) || ISWHITE(color));
    if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Bishop start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Bishop end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    }

    board_clear_square(board, startSquare);
    board_set(board, endSquare, (color|BISHOP));

    return 0;
}

int moveRook(bitboard *board, int startSquare, int endSquare, int color)
{
    assert(board);
    assert(ISBLACK(color) || ISWHITE(color));
    if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Rook start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Rook end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    }

    board_clear_square(board, startSquare);
    board_set(board, endSquare, (color|ROOK));
    
    return 0;
}

int moveQueen(bitboard *board, int startSquare, int endSquare, int color)
{
    
    assert(board);
    assert(ISBLACK(color) || ISWHITE(color));
    if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("Queen start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("Queen end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    } 

    board_clear_square(board, startSquare);
    board_set(board, endSquare, (color|QUEEN));

    return 0;
}

int moveKing(bitboard *board, int startSquare, int endSquare, int color)
{
    
    assert(board);
    assert(ISBLACK(color) || ISWHITE(color));
    if(startSquare < 0 || startSquare > 63)
    {
        DEBUG("King start square %d - out of bounds [0, 63]", startSquare);
        return -1;
    } 
    else if(endSquare < 0 || endSquare > 63)
    {
        DEBUG("King end square %d - out of bounds [0, 63]", endSquare);
        return -1;
    } 

    if(ISBLACK(color)) board->kingSquare_b = endSquare;
    else board->kingSquare_w = endSquare;


    board_clear_square(board, startSquare);
    board_set(board, endSquare, (color|KING));

    //Castling
    if(startSquare - endSquare == 2)
    {
        board_clear_square(board, startSquare - 4);
        board_set(board, endSquare + 1, (color|ROOK));
    }
    else if(startSquare - endSquare == -2)
    {
        board_clear_square(board, startSquare + 3);
        board_set(board, endSquare - 1, (color|ROOK));
    }

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
        DEBUG("Attempted to move invalid piece type.");
        return -1;
    }

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
    int piece = findPieceOnSquare(board, startSquare);
    if(!piece)
    {
        DEBUG("Could not find piece on start square.");
        return -1;
    }

    int promoteTo = 0;
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

    int capturedPiece;
    int capturedSquare;
    if(endSquare == board->enPassantSquare)
    {
        //en passant
        if(ISWHITE(board->turn))
        {
            capturedPiece = PAWN|BLACK;
            capturedSquare = board->enPassantSquare - 8;
        }
        else
        {
            capturedPiece = PAWN|WHITE;
            capturedSquare = board->enPassantSquare + 8;
        }
    }
    else
    {
        capturedPiece = findPieceOnSquare(board, endSquare);
        capturedSquare = endSquare;
    }

    move m = {0};
    createMove(&m, startSquare, endSquare, promoteTo, piece, capturedPiece, capturedSquare);
    move moveList[256];
    int count = generateMoveList(moveList, board, 0);
    int isPotentialMove = 0;
    for(int index = 0; index < count; index++)
    {
        if(m.startSquare == moveList[index].startSquare && m.endSquare == moveList[index].endSquare)
        {
            isPotentialMove = 1;
            break;
        }
    }

    if(!isPotentialMove)
    {
        printf("Piece move is not legal.\n");
        return -1;
    }

    return moveFromStruct(board, m);
}

int moveFromStruct(bitboard* board, move m)
{   
    assert(board);
    
    if(board->victor)
    {
        DEBUG("Cannot move from terminal gamestate; Victor=x%02x", board->victor);
        return -1;
    }
    else if(ISBLACK(m.piece) && board->turn == WHITE)
    {
        DEBUG("Attempted to move black piece on white's turn. (%d->%d)", m.startSquare, m.endSquare);
        return -1;
    }
    else if(ISWHITE(m.piece) && board->turn == BLACK)
    {
        DEBUG("Attempted to move white piece on black's turn. (%d->%d)", m.startSquare, m.endSquare);
        return -1;
    }
    else if(m.startSquare < 0 || m.startSquare > 63 || m.endSquare < 0 || m.endSquare > 63)
    {
        DEBUG("Piece cannot move out of bounds (%d -> %d).", m.startSquare, m.endSquare);
        return -1;
    }
    else if(m.startSquare == m.endSquare)
    {
        DEBUG("Piece cannot move in place on square %d.", m.startSquare);
        return -1;
    }

    moves_push(board, m);
    if(movePiece(board, m.startSquare, m.endSquare, m.piece, m.promoteTo) != 0) 
    {
        DEBUG("Failed to move piece from struct.");
        moveEntry* entry = moves_pop(board);
        if(entry) FREE(entry);
        return -1;
    }
    
    if((ISWHITE(board->turn) && isThreatened(board, board->kingSquare_w, WHITE)) || (ISBLACK(board->turn) && isThreatened(board, board->kingSquare_b, BLACK)))
    {
        moveEntry* entry = moves_pop(board);

        if(ISKING(entry->m.piece) && abs(entry->m.endSquare - entry->m.startSquare) == 2)
        {
            //Undo castle.
            if(entry->m.endSquare == 2)
            {
                //White queenside castle
                board_clear_square(board, 2);
                board_clear_square(board, 3);
                board_set(board, 0, ROOK|WHITE);
                board_set(board, 4, KING|WHITE);
                board->kingSquare_w = 4;
            }
            else if(entry->m.endSquare == 6)
            {
                //White kingside castle
                board_clear_square(board, 6);
                board_clear_square(board, 5);
                board_set(board, 7, ROOK|WHITE);
                board_set(board, 4, KING|WHITE);
                board->kingSquare_w = 4;
            }
            else if(entry->m.endSquare == 58)
            {
                //Black queenside castle
                board_clear_square(board, 58);
                board_clear_square(board, 59);
                board_set(board, 56, ROOK|BLACK);
                board_set(board, 60, KING|BLACK);
                board->kingSquare_b = 60;
            }
            else if(entry->m.endSquare == 62)
            {
                //Black kingside castle
                board_clear_square(board, 62);
                board_clear_square(board, 61);
                board_set(board, 63, ROOK|BLACK);
                board_set(board, 60, KING|BLACK);
                board->kingSquare_b = 60;
            }
        }
        else
        {
            board_clear_square(board, entry->m.endSquare);
            board_set(board, entry->m.startSquare, entry->m.piece);

            if(entry->m.capturedPiece) board_set(board, entry->m.capturedPieceSquare, entry->m.capturedPiece);

            if(ISKING(entry->m.piece))
            {
                if(ISWHITE(entry->m.piece)) board->kingSquare_w = entry->m.startSquare;
                else board->kingSquare_b = entry->m.startSquare;
            }
        }
        
        if(KINGSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[768];
        if(QUEENSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[769];
        if(KINGSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[770];
        if(QUEENSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[771];

        board->flags = entry->flags;
        
        if(KINGSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[768];
        if(QUEENSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[769];
        if(KINGSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[770];
        if(QUEENSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[771];

        board->movesSinceLastChange = entry->previousMovesSinceLastChange;

        board->hashCode ^= getEnPassantHash(board);
        board->enPassantSquare = entry->prevEnPassantSquare;
        board->hashCode ^= getEnPassantHash(board);
        FREE(entry);

        return -1;
    }
    
    if(!(ISPAWN(m.piece) && abs(m.startSquare - m.endSquare) == 16) && board->enPassantSquare != -1) 
    {
        board->hashCode ^= getEnPassantHash(board);
        board->enPassantSquare = -1;
    }

    //50 move rule counting
    if(ISPAWN(m.piece) || m.capturedPiece) board->movesSinceLastChange = 0;
    else board->movesSinceLastChange++;
    board->halfMoveCount++;

    //Calculate checks and change turn.
    if(board->turn == WHITE)
    {
        if(IS_IN_CHECK_W(board->flags)) UNCHECK_W(board->flags);
        if(isThreatened(board, board->kingSquare_b, BLACK)) CHECK_B(board->flags);
        board->turn=BLACK;
        board->hashCode ^= zobrist_keys[780];
    }
    else
    {
        if(IS_IN_CHECK_B(board->flags)) UNCHECK_B(board->flags);
        if(isThreatened(board, board->kingSquare_w, WHITE)) CHECK_W(board->flags);
        board->turn=WHITE;
        board->hashCode ^= zobrist_keys[780];
    }

    
    board->repetitionHashCodes[board->repetitionIndex++] = board->hashCode;

    //3-fold repetition check
    if(containsRepetition(board)) board->victor = DRAW|THREEFOLD;
    else if(board->movesSinceLastChange >= 100) board->victor = DRAW|FIFTYMOVERULE; //Variable stores half-moves
    else if((board->king_b|board->king_w) == board->pieces_all) board->victor = DRAW|INSUFFICIENT_MATERIAL; //King v King drawn INSUFFICIENT_MATERIAL
    else 
    {
        //Look for legal moves - calculate checkmate / stalemate.
        int existsLegalMove = 0;
        move moveList[256];
        int entryCount = generateMoveList(moveList, board, 0);
        if(!entryCount) existsLegalMove = 0;
        else
        {
            for(int index = 0; index < entryCount; index++)
            {
                moves_push(board, moveList[index]);
                if(movePiece(board, moveList[index].startSquare, moveList[index].endSquare, moveList[index].piece, moveList[index].promoteTo) != 0) 
                {
                    moveEntry* entry = moves_pop(board);
                    if(entry) FREE(entry);
                    continue;
                }

                if((ISWHITE(board->turn) && !isThreatened(board, board->kingSquare_w, WHITE)) || (ISBLACK(board->turn) && !isThreatened(board, board->kingSquare_b, BLACK)))
                {
                    existsLegalMove = 1;
                }

                moveEntry* entry = moves_pop(board);
                if(ISKING(entry->m.piece) && abs(entry->m.endSquare - entry->m.startSquare) == 2)
                {
                    //Undo castle.
                    if(entry->m.endSquare == 2)
                    {
                        //White queenside castle
                        board_clear_square(board, 2);
                        board_clear_square(board, 3);
                        board_set(board, 0, ROOK|WHITE);
                        board_set(board, 4, KING|WHITE);
                        board->kingSquare_w = 4;
                    }
                    else if(entry->m.endSquare == 6)
                    {
                        //White kingside castle
                        board_clear_square(board, 6);
                        board_clear_square(board, 5);
                        board_set(board, 7, ROOK|WHITE);
                        board_set(board, 4, KING|WHITE);
                        board->kingSquare_w = 4;
                    }
                    else if(entry->m.endSquare == 58)
                    {
                        //Black queenside castle
                        board_clear_square(board, 58);
                        board_clear_square(board, 59);
                        board_set(board, 56, ROOK|BLACK);
                        board_set(board, 60, KING|BLACK);
                        board->kingSquare_b = 60;
                    }
                    else if(entry->m.endSquare == 62)
                    {
                        //Black kingside castle
                        board_clear_square(board, 62);
                        board_clear_square(board, 61);
                        board_set(board, 63, ROOK|BLACK);
                        board_set(board, 60, KING|BLACK);
                        board->kingSquare_b = 60;
                    }
                }
                else
                {
                    board_clear_square(board, entry->m.endSquare);
                    board_set(board, entry->m.startSquare, entry->m.piece);

                    if(entry->m.capturedPiece) board_set(board, entry->m.capturedPieceSquare, entry->m.capturedPiece);

                    if(ISKING(entry->m.piece))
                    {
                        if(ISWHITE(entry->m.piece)) board->kingSquare_w = entry->m.startSquare;
                        else board->kingSquare_b = entry->m.startSquare;
                    }
                }
                
                if(KINGSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[768];
                if(QUEENSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[769];
                if(KINGSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[770];
                if(QUEENSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[771];

                board->flags = entry->flags;
                
                if(KINGSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[768];
                if(QUEENSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[769];
                if(KINGSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[770];
                if(QUEENSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[771];

                board->movesSinceLastChange = entry->previousMovesSinceLastChange;

                board->hashCode ^= getEnPassantHash(board);
                board->enPassantSquare = entry->prevEnPassantSquare;
                board->hashCode ^= getEnPassantHash(board);
            }
        }
        

        if(!existsLegalMove)
        {
            if(board->turn == WHITE)
            {
                if(IS_IN_CHECK_W(board->flags)) board->victor = BLACK;
                else board->victor = DRAW|STALEMATED_WHITE;
            }
            else
            {
                if(IS_IN_CHECK_B(board->flags)) board->victor = WHITE;
                else board->victor = DRAW|STALEMATED_BLACK;
            }
        }
        /* Other Drawn INSUFFICIENT_MATERIALs */
        //King + Minor Piece vs King
        else if(board->pieces_b == board->king_b && board->pawn_w == 0 && board->rook_w == 0 && board->queen_w == 0)
        {
            uint64_t mask = 1;
            int count = 0;
            for(int i = 0; i < 63; i++)
            {
                if((board->bishop_w|board->knight_w)&mask) count++;

                mask = mask<<1;
            }
            if(count == 1) board->victor = DRAW|INSUFFICIENT_MATERIAL;
        }
        else if(board->pieces_w == board->king_w && board->pawn_b == 0 && board->rook_b == 0 && board->queen_b == 0)
        {
            uint64_t mask = 1;
            int count = 0;
            for(int i = 0; i < 63; i++)
            {
                if((board->bishop_b|board->knight_b)&mask) count++;

                mask = mask<<1;
            }
            if(count == 1) board->victor = DRAW|INSUFFICIENT_MATERIAL;
        }
        //King + Bishops vs King + Bishops (Same color bishops)
        else if(board->pieces_all == (board->king_w|board->bishop_w|board->king_b|board->bishop_b) && 
                ((board->bishop_b|board->bishop_w) == ((board->bishop_b|board->bishop_w)&LIGHT_SQUARES) ||
                 (board->bishop_b|board->bishop_w) == ((board->bishop_b|board->bishop_w)&DARK_SQUARES))) 
        {
            board->victor = DRAW|INSUFFICIENT_MATERIAL;
        }
    }

    return 0;
}

moveEntry* unmove(bitboard *board)
{
    assert(board);

    board->repetitionIndex--;
    moveEntry* entry = moves_pop(board);
    if(!entry)
    {
        DEBUG("No move history to undo.");
        return NULL;
    }

    if(ISKING(entry->m.piece) && abs(entry->m.endSquare - entry->m.startSquare) == 2)
    {
        //Undo castle.
        if(entry->m.endSquare == 2)
        {
            //White queenside castle
            board_clear_square(board, 2);
            board_clear_square(board, 3);
            board_set(board, 0, ROOK|WHITE);
            board_set(board, 4, KING|WHITE);
            board->kingSquare_w = 4;
        }
        else if(entry->m.endSquare == 6)
        {
            //White kingside castle
            board_clear_square(board, 6);
            board_clear_square(board, 5);
            board_set(board, 7, ROOK|WHITE);
            board_set(board, 4, KING|WHITE);
            board->kingSquare_w = 4;
        }
        else if(entry->m.endSquare == 58)
        {
            //Black queenside castle
            board_clear_square(board, 58);
            board_clear_square(board, 59);
            board_set(board, 56, ROOK|BLACK);
            board_set(board, 60, KING|BLACK);
            board->kingSquare_b = 60;
        }
        else if(entry->m.endSquare == 62)
        {
            //Black kingside castle
            board_clear_square(board, 62);
            board_clear_square(board, 61);
            board_set(board, 63, ROOK|BLACK);
            board_set(board, 60, KING|BLACK);
            board->kingSquare_b = 60;
        }
    }
    else
    {
        board_clear_square(board, entry->m.endSquare);
        board_set(board, entry->m.startSquare, entry->m.piece);

        if(entry->m.capturedPiece) board_set(board, entry->m.capturedPieceSquare, entry->m.capturedPiece);

        if(ISKING(entry->m.piece))
        {
            if(ISWHITE(entry->m.piece)) board->kingSquare_w = entry->m.startSquare;
            else board->kingSquare_b = entry->m.startSquare;
        }
    }
    
    if(KINGSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[768];
    if(QUEENSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[769];
    if(KINGSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[770];
    if(QUEENSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[771];

    board->flags = entry->flags;
    
    if(KINGSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[768];
    if(QUEENSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[769];
    if(KINGSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[770];
    if(QUEENSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[771];

    board->movesSinceLastChange = entry->previousMovesSinceLastChange;

    board->hashCode ^= getEnPassantHash(board);
    board->enPassantSquare = entry->prevEnPassantSquare;
    board->hashCode ^= getEnPassantHash(board);

    if(board->turn == WHITE) 
    {
        board->turn = BLACK;
        board->hashCode ^= zobrist_keys[780];
    }
    else if (board->turn == BLACK) 
    {
        board->turn = WHITE;
        board->hashCode ^= zobrist_keys[780];
    }
    
    board->victor = 0;
    board->halfMoveCount--;


    return entry;
}