#include "../board/bitboard.h"
#include "../board/moves.h" 
#include <string.h>
#include "../debug.h"

int generateMoveList(move* movesList, bitboard* board, int capturesOnly)
{
    if(board->victor) 
    {
        DEBUG_ERROR("Cannot generate moves for terminated game; Victor=%d", board->victor);
        return 0;
    }

    int size = 0;
    uint64_t allies, enemies, knights, bishop, rook, queen, king;
    if(ISWHITE(board->turn))
    {
        allies = board->pieces_side[WHITE];
        enemies = board->pieces_side[BLACK];
        knights = board->pieces[WHITE_KNIGHT];
        bishop = board->pieces[WHITE_BISHOP];
        rook = board->pieces[WHITE_ROOK];
        queen = board->pieces[WHITE_QUEEN];
        king = board->pieces[WHITE_KING];
    }
    else
    {
        allies = board->pieces_side[BLACK];
        enemies = board->pieces_side[WHITE];
        knights = board->pieces[BLACK_KNIGHT];
        bishop = board->pieces[BLACK_BISHOP];
        rook = board->pieces[BLACK_ROOK];
        queen = board->pieces[BLACK_QUEEN];
        king = board->pieces[BLACK_KING];
    }

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
                    createMove(&movesList[size++], startSquare, endSquare, KNIGHT, piece, board);
                    createMove(&movesList[size++], startSquare, endSquare, BISHOP, piece,board); 
                    createMove(&movesList[size++], startSquare, endSquare, ROOK, piece, board);     
                    createMove(&movesList[size++], startSquare, endSquare, QUEEN, piece, board);
                }
                else createMove(&movesList[size++], startSquare, endSquare, 0, piece, board);

                mask&=(mask - 1);
            }

            mask = WHITE_PAWN_DOUBLEPUSH_MASK(board);
            while(mask)
            {
                int endSquare = __builtin_ctzll(mask);
                int startSquare = endSquare - 16;

                createMove(&movesList[size++], startSquare, endSquare, 0, piece, board);
                
                mask&=(mask - 1);
            }
        }

        mask = WHITE_PAWN_LEFTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare - 7;

            if(endSquare >= 56)
            {
                //Promotion
                createMove(&movesList[size++], startSquare, endSquare, KNIGHT, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, BISHOP, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, ROOK, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, QUEEN, piece, board);
            }
            else createMove(&movesList[size++], startSquare, endSquare, 0, piece, board);
            
            mask&=(mask - 1);
        }

        mask = WHITE_PAWN_RIGHTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare - 9;

            if(endSquare >= 56)
            {
                //Promotion
                createMove(&movesList[size++], startSquare, endSquare, KNIGHT, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, BISHOP, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, ROOK, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, QUEEN, piece, board);
            }
            else createMove(&movesList[size++], startSquare, endSquare, 0, piece, board);
            
            mask&=(mask - 1);
        }

        
        if(board->enPassantSquare != -1)
        {
            mask = EN_PASSANT_ATTACKERS_WHITE(singleBitMask(board->enPassantSquare), board);
            while(mask)
            {
                int startSquare = __builtin_ctzll(mask);
                createMove(&movesList[size++], startSquare, board->enPassantSquare, 0, piece, board);
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
                    createMove(&movesList[size++], startSquare, endSquare, KNIGHT, piece,board);
                    createMove(&movesList[size++], startSquare, endSquare, BISHOP, piece, board); 
                    createMove(&movesList[size++], startSquare, endSquare, ROOK, piece, board);     
                    createMove(&movesList[size++], startSquare, endSquare, QUEEN, piece, board);
                }
                else createMove(&movesList[size++], startSquare, endSquare, 0, piece, board);

                mask&=(mask - 1);
            }

            mask = BLACK_PAWN_DOUBLEPUSH_MASK(board);
            while(mask)
            {
                int endSquare = __builtin_ctzll(mask);
                int startSquare = endSquare + 16;

                createMove(&movesList[size++], startSquare, endSquare, 0, piece, board);
                
                mask&=(mask - 1);
            }
        }

        mask = BLACK_PAWN_LEFTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare + 7;

            if(endSquare <= 7)
            {
                //Promotion
                createMove(&movesList[size++], startSquare, endSquare, KNIGHT, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, BISHOP, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, ROOK, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, QUEEN, piece, board);
            }
            else
            {
                createMove(&movesList[size++], startSquare, endSquare, 0, piece, board);
            }
            
            mask&=(mask - 1);
        }

        mask = BLACK_PAWN_RIGHTATTACKS(board);
        while(mask)
        {
            int endSquare = __builtin_ctzll(mask);
            int startSquare = endSquare + 9;

            if(endSquare <= 7)
            {
                //Promotion
                createMove(&movesList[size++], startSquare, endSquare, KNIGHT, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, BISHOP, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, ROOK, piece, board);
                createMove(&movesList[size++], startSquare, endSquare, QUEEN, piece, board);
            }
            else
            {
                createMove(&movesList[size++], startSquare, endSquare, 0, piece, board);
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
                createMove(&movesList[size++], startSquare, board->enPassantSquare, 0, piece, board);
                mask&=(mask - 1);
            }
        }
    }

    while(knights) 
    {
        int startSquare = __builtin_ctzll(knights);
        uint64_t mask = knightMoves(allies, startSquare);
        if (capturesOnly) mask &= enemies;
        
        while(mask) 
        {
            int endSquare = __builtin_ctzll(mask);
            createMove(&movesList[size++], startSquare, endSquare, 0, (KNIGHT | board->turn), board);
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
            createMove(&movesList[size++], startSquare, endSquare, 0, (BISHOP | board->turn), board);
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
            createMove(&movesList[size++], startSquare, endSquare, 0, (ROOK | board->turn), board);
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
            createMove(&movesList[size++], startSquare, endSquare, 0, (QUEEN | board->turn), board);
            moveMask&=(moveMask-1);
        }
        queen&=(queen-1);
    }

    if(king)
    {
        int startSquare = __builtin_ctzll(king);
        uint64_t moveMask = kingMoves(board, startSquare, board->turn);
        if (capturesOnly) moveMask &= enemies;
        while(moveMask) 
        {
            int endSquare = __builtin_ctzll(moveMask);
            createMove(&movesList[size++], startSquare, endSquare, 0, (KING | board->turn), board);
            moveMask&=(moveMask-1);
        }
    }
    
    return size;
}


int isThreatened(bitboard* board, int square, int squareColor)
{
    uint64_t bishopqueen = 0;
    uint64_t rookqueen = 0;
    uint64_t enemyPieces = 0;
    uint64_t allyPieces = 0;
    if(ISWHITE(squareColor))
    {
        enemyPieces = board->pieces_side[BLACK];
        allyPieces = board->pieces_side[WHITE];
        bishopqueen = board->pieces[BLACK_BISHOP]|board->pieces[BLACK_QUEEN];
        rookqueen = board->pieces[BLACK_ROOK]|board->pieces[BLACK_QUEEN];
        if(square < 48 && pawnAttacks[0][square]&board->pieces[BLACK_PAWN]) return THREAT_TYPE_PAWN;
        if(knightAttacks[square]&(board->pieces[BLACK_KNIGHT])) return THREAT_TYPE_KNIGHT;
        if(kingAttacks[square]&(board->pieces[BLACK_KING])) return THREAT_TYPE_KING;
    }
    else if(ISBLACK(squareColor))
    {
        enemyPieces = board->pieces_side[WHITE];
        allyPieces = board->pieces_side[BLACK];
        bishopqueen = board->pieces[WHITE_BISHOP]|board->pieces[WHITE_QUEEN];
        rookqueen = board->pieces[WHITE_ROOK]|board->pieces[WHITE_QUEEN];
        if(square > 15 && pawnAttacks[1][square]&board->pieces[WHITE_PAWN]) return THREAT_TYPE_PAWN;
        if(knightAttacks[square]&(board->pieces[WHITE_KNIGHT])) return THREAT_TYPE_KNIGHT;
        if(kingAttacks[square]&(board->pieces[WHITE_KING])) return THREAT_TYPE_KING;
    }
    if(bishopMoves(allyPieces, enemyPieces, square)&(bishopqueen)) return THREAT_TYPE_BISHOPQUEEN;
    if(rookMoves(allyPieces, enemyPieces, square)&(rookqueen)) return THREAT_TYPE_ROOKQUEEN;

    return THREAT_TYPE_NONE;
}

uint64_t pyrrhicPawnAttacks(int square, int color)
{
    color ^= 1;
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
        returnedValue&=(~board->pieces_side[WHITE]);

        //uint64_t betweenMask = 0x60; //Squares 5 and 6 between king on 4 and rook on 7

        if(KINGSIDE_CASTLE_WHITE(board->flags) && !(board->flags&16) && !(board->pieces_all&0x60) && 
                                                        !isThreatened(board, square, color) && 
                                                        !isThreatened(board, square+1, color) && 
                                                        !isThreatened(board, square+2, color)) returnedValue|=0x40;

        //betweenMask = 0xE; //Square 1 and 2 and 3 between rook on 0 and king on 4

        if(QUEENSIDE_CASTLE_WHITE(board->flags) && !(board->flags&16) && !(board->pieces_all&0xE) && 
                                                        !isThreatened(board, square, color) && 
                                                        !isThreatened(board, square-1, color) && 
                                                        !isThreatened(board, square-2, color)) returnedValue|=0x4;
        
    }
    else
    {
        returnedValue&=(~board->pieces_side[BLACK]);

        //uint64_t betweenMask = 0x6000000000000000; //Squares 61 and 62 between king on 60 and rook on 63

        //Black kingside
        if(KINGSIDE_CASTLE_BLACK(board->flags) && !(board->flags&32) && !(board->pieces_all&0x6000000000000000) && 
                                                            !isThreatened(board, square, color) &&
                                                            !isThreatened(board, square+1, color) && 
                                                            !isThreatened(board, square+2, color)) returnedValue|=0x4000000000000000;

        //betweenMask = 0x0E00000000000000; //Square 57 and 58 and 59 between rook on 56 and king on 60
        if(QUEENSIDE_CASTLE_BLACK(board->flags&8) && !(board->flags&32) && !(board->pieces_all&0x0E00000000000000) &&
                                                            !isThreatened(board, square, color) &&
                                                            !isThreatened(board, square-1, color) && 
                                                            !isThreatened(board, square-2, color)) returnedValue|=0x0400000000000000;
    }
    
    return returnedValue;
}

int movePiece(bitboard *board, move* m)
{
    assert(board);
    assert(m->startSquare >= 0 && m->startSquare <= 63 && m->endSquare >= 0 && m->endSquare <= 63);

    switch(PIECE(m->piece))
    {
        case PAWN:
            int difference = abs(m->startSquare - m->endSquare);
            
            if(difference == 8 || difference == 16)
            {
                if((ISWHITE(m->piece) && m->endSquare > 55) || (ISBLACK(m->piece) && m->endSquare < 8))
                {
                    //Promotion
                    if(m->promoteTo == 0) m->promoteTo = QUEEN;
                    board_clear_square(board, m->startSquare);
                    board_set(board, m->endSquare, (COLOR(m->piece)|m->promoteTo));
                }
                else
                {
                    board_move_piece_quietly(board, m->startSquare, m->endSquare);
                }
                
                //Update en passant square if necessary.
                if(difference == 16)
                {
                    if(ISWHITE(m->piece)) 
                    {
                        board->hashCode ^= getEnPassantHash(board);
                        board->enPassantSquare = m->endSquare - 8;
                        board->hashCode ^= getEnPassantHash(board);
                    }
                    else 
                    {
                        board->hashCode ^= getEnPassantHash(board);
                        board->enPassantSquare = m->endSquare + 8;
                        board->hashCode ^= getEnPassantHash(board);
                    }
                }
            }
            else if(difference == 7 || difference == 9)
            {
                //Diagonal Capture

                //Check for en passant.
                if(m->endSquare == board->enPassantSquare)
                {
                    if(ISWHITE(m->piece)) 
                    {
                        m->capturedPieceSquare = board->enPassantSquare - 8;
                        m->capturedPiece = findPieceOnSquare(board, m->capturedPieceSquare);
                        board_clear_square(board, board->enPassantSquare - 8);
                    }
                    else 
                    {
                        m->capturedPieceSquare = board->enPassantSquare + 8;
                        m->capturedPiece = findPieceOnSquare(board, m->capturedPieceSquare);
                        board_clear_square(board, board->enPassantSquare + 8);
                    }
                }
                else m->capturedPiece = findPieceOnSquare(board, m->endSquare);

                if((ISWHITE(m->piece) && m->endSquare > 55) || (ISBLACK(m->piece) && m->endSquare < 8))
                {
                    //Promotion
                    if(m->promoteTo == 0) m->promoteTo = QUEEN;
                    board_clear_square(board, m->startSquare);
                    board_clear_square(board, m->endSquare);
                    board_set(board, m->endSquare, (COLOR(m->piece)|m->promoteTo));
                }
                else
                {
                    board_move_piece(board, m->startSquare, m->endSquare);
                }      
            }
            break;
        case ROOK:
            if(m->startSquare == 0 && QUEENSIDE_CASTLE_WHITE(board->flags)) { BAN_QUEENCASTLE_W(board->flags); board->hashCode ^= zobrist_keys[768]; }
            else if(m->startSquare == 7 && KINGSIDE_CASTLE_WHITE(board->flags)) { BAN_KINGCASTLE_W(board->flags); board->hashCode ^= zobrist_keys[769]; }
            else if(m->startSquare == 56 && QUEENSIDE_CASTLE_BLACK(board->flags)) { BAN_QUEENCASTLE_B(board->flags); board->hashCode ^= zobrist_keys[771]; }
            else if(m->startSquare == 63 && KINGSIDE_CASTLE_BLACK(board->flags)) { BAN_KINGCASTLE_B(board->flags); board->hashCode ^= zobrist_keys[770]; }
        case KNIGHT:
        case BISHOP:
        case QUEEN:
            m->capturedPiece = findPieceOnSquare(board, m->endSquare);
            board_move_piece(board, m->startSquare, m->endSquare);
            break;
        case KING:
            if(ISBLACK(m->piece)) 
            {
                board->kingSquare_b = m->endSquare;
                if(QUEENSIDE_CASTLE_BLACK(board->flags)) BAN_QUEENCASTLE_B(board->flags);
                if(KINGSIDE_CASTLE_BLACK(board->flags)) BAN_KINGCASTLE_B(board->flags);
            }
            else 
            {
                board->kingSquare_w = m->endSquare;
                if(QUEENSIDE_CASTLE_WHITE(board->flags)) BAN_QUEENCASTLE_W(board->flags);
                if(KINGSIDE_CASTLE_WHITE(board->flags)) BAN_KINGCASTLE_W(board->flags);
            }

            m->capturedPiece = findPieceOnSquare(board, m->endSquare);
            board_move_piece(board, m->startSquare, m->endSquare);

            //Castling
            if(m->startSquare - m->endSquare == 2) board_move_piece_quietly(board, m->startSquare - 4, m->endSquare + 1);
            else if(m->startSquare - m->endSquare == -2) board_move_piece_quietly(board, m->startSquare + 3, m->endSquare - 1);
            break;
        default:
            DEBUG_ERROR("Attempted to move invalid piece type.");
            return -1;
            break;
    }
    
    m->repetitionIndex = board->repetitionIndex;
    moves_push(board, *m);

    if(ISROOK(m->capturedPiece))
    {
            if(m->capturedPieceSquare == 0 && QUEENSIDE_CASTLE_WHITE(board->flags)) { BAN_QUEENCASTLE_W(board->flags); board->hashCode ^= zobrist_keys[768]; }
            else if(m->capturedPieceSquare == 7 && KINGSIDE_CASTLE_WHITE(board->flags)) { BAN_KINGCASTLE_W(board->flags); board->hashCode ^= zobrist_keys[769]; }
            else if(m->capturedPieceSquare == 56 && QUEENSIDE_CASTLE_BLACK(board->flags)) { BAN_QUEENCASTLE_B(board->flags); board->hashCode ^= zobrist_keys[771]; }
            else if(m->capturedPieceSquare == 63 && KINGSIDE_CASTLE_BLACK(board->flags)) { BAN_KINGCASTLE_B(board->flags); board->hashCode ^= zobrist_keys[770]; }
    }
    if(!(ISPAWN(m->piece) && abs(m->startSquare - m->endSquare) == 16) && board->enPassantSquare != -1) 
    {
        board->hashCode ^= getEnPassantHash(board);
        board->enPassantSquare = -1;
    }

    //50 move rule counting
    if(ISPAWN(m->piece) || m->capturedPiece != EMPTY_PIECE) board->movesSinceLastChange = 0;
    else board->movesSinceLastChange++;
    board->halfMoveCount++;

    //Calculate checks and change turn.
    if(board->turn == WHITE)
    {
        if(IS_IN_CHECK_W(board->flags)) UNCHECK_W(board->flags);
        if(isThreatened(board, board->kingSquare_b, BLACK)) CHECK_B(board->flags);
    }
    else
    {
        if(IS_IN_CHECK_B(board->flags)) UNCHECK_B(board->flags);
        if(isThreatened(board, board->kingSquare_w, WHITE)) CHECK_W(board->flags);
    }
    board->turn ^= 1;
    board->hashCode ^= zobrist_keys[780];
    
    if(m->capturedPiece != EMPTY_PIECE || ISPAWN(m->piece)) board->repetitionIndex = 0;
    board->repetitionHashCodes[board->repetitionIndex++] = board->hashCode;

    return 0;
}

void applyNullMove(bitboard* board)
{
    assert(board);
    board->turn ^= 1;
    board->hashCode ^= zobrist_keys[780];
}

move getStructFromString(bitboard* board, char* str)
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
    if(piece == EMPTY_PIECE)
    {
        DEBUG_ERROR("Could not find piece on start square.");
        return (move){0};
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

    move m = {0};
    createMove(&m, startSquare, endSquare, promoteTo, piece, board);

    move moveList[MAX_POSITION_MOVES];
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
        return (move){0};
    }
    
    return m;
}

int moveFromStruct(bitboard* board, move m)
{   
    assert(board);
    assert(!board->victor);
    assert(!ISBLACK(m.piece) || board->turn != WHITE);
    assert(!ISWHITE(m.piece) || board->turn != BLACK);
    assert(m.startSquare >= 0 && m.startSquare <= 63 && m.endSquare >= 0 && m.endSquare <= 63);
    assert(IS_VALID_MOVE(m));

    if(movePiece(board, &m) != 0) 
    {
        DEBUG_ERROR("Failed to move piece from struct.");
        return -1;
    }
    
    if((ISWHITE(board->turn) && isThreatened(board, board->kingSquare_b, BLACK)) || (ISBLACK(board->turn) && isThreatened(board, board->kingSquare_w, WHITE)))
    {
        //Psuedo-legal move generator created an illegal move. Fail silently.
        unmove(board);
        return -1;
    }

    //3-fold repetition check
    if(containsRepetition(board)) board->victor = VICTOR_DRAW_THREEFOLD;
    else if(board->movesSinceLastChange >= 100) board->victor = VICTOR_DRAW_FIFTY_MOVE_RULE; //Variable stores half-moves
    else if((board->pieces[BLACK_KING]|board->pieces[WHITE_KING]) == board->pieces_all) board->victor = VICTOR_DRAW_INSUFFICIENT_MATERIAL;
    else 
    {
        //Look for legal moves - calculate checkmate / stalemate.
        int existsLegalMove = 0;
        move moveList[MAX_POSITION_MOVES];
        int entryCount = generateMoveList(moveList, board, 0);
        if(!entryCount) existsLegalMove = 0;
        else
        {
            for(int index = 0; index < entryCount; index++)
            {
                if(movePiece(board, &moveList[index]) != 0) continue;

                if((ISWHITE(board->turn) && !isThreatened(board, board->kingSquare_b, BLACK)) || (ISBLACK(board->turn) && !isThreatened(board, board->kingSquare_w, WHITE)))
                {
                    existsLegalMove = 1;
                }

                unmove(board);
                if(existsLegalMove) break;
            }
        }
        

        if(!existsLegalMove)
        {
            if(board->turn == WHITE)
            {
                if(IS_IN_CHECK_W(board->flags)) board->victor = VICTOR_BLACK;
                else board->victor = VICTOR_DRAW_STALEMATE_WHITE;
            }
            else
            {
                if(IS_IN_CHECK_B(board->flags)) board->victor = VICTOR_WHITE;
                else board->victor = VICTOR_DRAW_STALEMATE_BLACK;
            }
        }
        /* Other Drawn INSUFFICIENT_MATERIALs */
        //King + Minor Piece vs King
        else if(board->pieces_side[BLACK] == board->pieces[BLACK_KING] && board->pieces[WHITE_PAWN] == 0 && board->pieces[WHITE_ROOK] == 0 && board->pieces[WHITE_QUEEN] == 0)
        {
            if(__builtin_popcountll(board->pieces[WHITE_BISHOP]|board->pieces[WHITE_KNIGHT]) <= 1) board->victor = VICTOR_DRAW_INSUFFICIENT_MATERIAL;
        }
        else if(board->pieces_side[WHITE] == board->pieces[WHITE_KING] && board->pieces[BLACK_PAWN] == 0 && board->pieces[BLACK_ROOK] == 0 && board->pieces[BLACK_QUEEN] == 0)
        {
            if(__builtin_popcountll(board->pieces[BLACK_BISHOP]|board->pieces[BLACK_KNIGHT]) <= 1) board->victor = VICTOR_DRAW_INSUFFICIENT_MATERIAL;
        }
        //King + Bishops vs King + Bishops (Same color bishops)
        else if(board->pieces_all == (board->pieces[WHITE_KING]|board->pieces[WHITE_BISHOP]|board->pieces[BLACK_KING]|board->pieces[BLACK_BISHOP]) && 
                ((board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP]) == ((board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP])&LIGHT_SQUARES) ||
                 (board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP]) == ((board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP])&DARK_SQUARES))) 
        {
            board->victor = VICTOR_DRAW_INSUFFICIENT_MATERIAL;
        }
    }

    return 0;
}

move unmove(bitboard *board)
{
    assert(board);

    move m = moves_pop(board);
    if(!m.startSquare && !m.endSquare)
    {
        DEBUG_ERROR("No move history to undo.");
        return (move){0};
    }
    board->repetitionIndex = m.repetitionIndex;

    if(m.promoteTo)
    {
        board_clear_square(board, m.endSquare);
        board_set(board, m.startSquare, m.piece);
    }
    else board_move_piece_quietly(board, m.endSquare, m.startSquare);

    if(m.capturedPiece != EMPTY_PIECE) board_set(board, m.capturedPieceSquare, m.capturedPiece);

    if(ISKING(m.piece))
    {
        if(ISWHITE(m.piece)) board->kingSquare_w = m.startSquare;
        else board->kingSquare_b = m.startSquare;
        
        //undo castle
        if(abs(m.endSquare - m.startSquare) == 2)
        {
            if(m.endSquare == 2) board_move_piece_quietly(board, 3, 0);
            else if(m.endSquare == 6) board_move_piece_quietly(board, 5, 7);
            else if(m.endSquare == 58) board_move_piece_quietly(board, 59, 56);
            else if(m.endSquare == 62) board_move_piece_quietly(board, 61, 63);
        }
    }
    
    
    if(KINGSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[768];
    if(QUEENSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[769];
    if(KINGSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[770];
    if(QUEENSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[771];

    board->flags = m.prevFlags;
    
    if(KINGSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[768];
    if(QUEENSIDE_CASTLE_WHITE(board->flags)) board->hashCode ^= zobrist_keys[769];
    if(KINGSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[770];
    if(QUEENSIDE_CASTLE_BLACK(board->flags)) board->hashCode ^= zobrist_keys[771];

    board->movesSinceLastChange = m.previousMovesSinceLastChange;

    board->hashCode ^= getEnPassantHash(board);
    board->enPassantSquare = m.prevEnPassantSquare;
    board->hashCode ^= getEnPassantHash(board);

    board->turn ^= 1;
    board->hashCode ^= zobrist_keys[780];
    
    board->victor = 0;
    board->halfMoveCount--;

    return m;
}