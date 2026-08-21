#include "debug.h"
#include "board/bitboard.h"
#include "board/moves.h"
#include "hashtables/hash.h"
#include <string.h>
#include <math.h>

uint64_t board_file[COLUMN_COUNT] = {
    (0x0101010101010101ULL << 0),
    (0x0101010101010101ULL << 1),
    (0x0101010101010101ULL << 2),
    (0x0101010101010101ULL << 3),
    (0x0101010101010101ULL << 4),
    (0x0101010101010101ULL << 5),
    (0x0101010101010101ULL << 6),
    (0x0101010101010101ULL << 7)
};

uint64_t board_rank[ROW_COUNT] = {
    (0xFFULL << 0),
    (0xFFULL << 8),
    (0xFFULL << 16),
    (0xFFULL << 24),
    (0xFFULL << 32),
    (0xFFULL << 40),
    (0xFFULL << 48),
    (0xFFULL << 56)
};

uint64_t bordering_files[COLUMN_COUNT] = {
    (0x0101010101010101ULL << 1),
    (0x0101010101010101ULL << 0) | (0x0101010101010101ULL << 2),
    (0x0101010101010101ULL << 1) | (0x0101010101010101ULL << 3),
    (0x0101010101010101ULL << 2) | (0x0101010101010101ULL << 4),
    (0x0101010101010101ULL << 3) | (0x0101010101010101ULL << 5),
    (0x0101010101010101ULL << 4) | (0x0101010101010101ULL << 6),
    (0x0101010101010101ULL << 5) | (0x0101010101010101ULL << 7),
    (0x0101010101010101ULL << 6)
};

void getSquareName(int square, char* target)
{
    assert(target);
    assert(square >= 0 && square <= 63);
    
    //Output example: e4
    target[0] = columnNames[getColumn(square)];
    target[1] = '1' + getRow(square);
    target[2] = '\0';
}

int getSquareNumber(char* squareName)
{ 
    return ((squareName[0] - 'a') + 8*(squareName[1] - '0' - 1));
}

int popLSB(uint64_t *bitboard)
{
    int LSB_Index = __builtin_ctzll(*bitboard);
    *bitboard &= (*bitboard - 1);
    return LSB_Index;
}

int isDraw(bitboard* board)
{
    //Repetition
    if(containsRepetition(board)) return VICTOR_DRAW_THREEFOLD;
    //50-move rule
    else if(board->movesSinceLastChange >= 100) return VICTOR_DRAW_FIFTY_MOVE_RULE; //Variable stores half-moves
    //Trivial insufficent material
    else if((board->pieces[BLACK_KING]|board->pieces[WHITE_KING]) == board->pieces_all) return VICTOR_DRAW_INSUFFICIENT_MATERIAL;
    //Other insufficient material
    else
    {
        //King + Minor Piece vs King
        if(board->pieces_side[BLACK] == board->pieces[BLACK_KING] && board->pieces[WHITE_PAWN] == 0 && board->pieces[WHITE_ROOK] == 0 && board->pieces[WHITE_QUEEN] == 0)
        {
            if(__builtin_popcountll(board->pieces[WHITE_BISHOP]|board->pieces[WHITE_KNIGHT]) <= 1) return VICTOR_DRAW_INSUFFICIENT_MATERIAL;
        }
        else if(board->pieces_side[WHITE] == board->pieces[WHITE_KING] && board->pieces[BLACK_PAWN] == 0 && board->pieces[BLACK_ROOK] == 0 && board->pieces[BLACK_QUEEN] == 0)
        {
            if(__builtin_popcountll(board->pieces[BLACK_BISHOP]|board->pieces[BLACK_KNIGHT]) <= 1) return VICTOR_DRAW_INSUFFICIENT_MATERIAL;
        }
        //King + Bishops vs King + Bishops (Same color bishops)
        else if(board->pieces_all == (board->pieces[WHITE_KING]|board->pieces[WHITE_BISHOP]|board->pieces[BLACK_KING]|board->pieces[BLACK_BISHOP]) && 
                ((board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP]) == ((board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP])&LIGHT_SQUARES) ||
                 (board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP]) == ((board->pieces[BLACK_BISHOP]|board->pieces[WHITE_BISHOP])&DARK_SQUARES))) 
        {
            return VICTOR_DRAW_INSUFFICIENT_MATERIAL;
        }
    }

    return 0;
}

//Only called when it has already been determined that there's no legal moves.
int getMateResult(bitboard* board)
{
    if(board->turn == WHITE)
    {
        if(IS_IN_CHECK_W(board->flags)) return VICTOR_BLACK;
        else return VICTOR_DRAW_STALEMATE_WHITE;
    }
    else
    {
        if(IS_IN_CHECK_B(board->flags)) return VICTOR_WHITE;
        else return VICTOR_DRAW_STALEMATE_BLACK;
    }
}

void export_fen_from_board(bitboard* board, char* outputFenString)
{
    assert(board && outputFenString);

    char rows[8][9] = {'\0'};

    for(int row = 0; row < ROW_COUNT; row++)
    {
        for(int column = 0; column < COLUMN_COUNT; column++)
        {
            int columnIndex = 0;
            for(columnIndex = 0; columnIndex < column; columnIndex++)
            {
                if(rows[row][columnIndex] == '\0') break;
            }
            
            int piece = findPieceOnSquare(board, row*8 + column);
            if(piece != EMPTY_PIECE)
            {
                char pieceChar = 0;
                if(ISPAWN(piece)) pieceChar = 'p';
                else if(ISBISHOP(piece)) pieceChar = 'b';
                else if(ISKNIGHT(piece)) pieceChar = 'n';
                else if(ISROOK(piece)) pieceChar = 'r';
                else if(ISQUEEN(piece)) pieceChar = 'q';
                else if(ISKING(piece)) pieceChar = 'k';

                if(ISWHITE(piece)) pieceChar-=32;
                rows[row][columnIndex] = pieceChar;
            }
            else
            {
                if(columnIndex == 0 || (columnIndex > 0 && rows[row][columnIndex - 1] >= 'A' && rows[row][columnIndex - 1] <= 'z')) rows[row][columnIndex] = '1';
                else if(rows[row][columnIndex - 1] >= '1' && rows[row][columnIndex - 1] <= '7') rows[row][columnIndex - 1]++;
            }
        }
    }

    char activeColor = (ISWHITE(board->turn)) ? 'w' : 'b';
    
    char castlingRights[5] = {'\0'};
    castlingRights[0] = '-';
    int writeIndex = 0;
    if(KINGSIDE_CASTLE_WHITE(board->flags)) { castlingRights[writeIndex] = 'K'; writeIndex++;}
    if(QUEENSIDE_CASTLE_WHITE(board->flags)) { castlingRights[writeIndex] = 'Q'; writeIndex++;}
    if(KINGSIDE_CASTLE_BLACK(board->flags)) { castlingRights[writeIndex] = 'k'; writeIndex++;}
    if(QUEENSIDE_CASTLE_BLACK(board->flags)) castlingRights[writeIndex] = 'q';

    char enPassantSquare[3] = {'\0'};
    if(board->enPassantSquare == NO_EP_SQUARE) enPassantSquare[0] = '-';
    else getSquareName(board->enPassantSquare, enPassantSquare);

    sprintf(outputFenString, "%s/%s/%s/%s/%s/%s/%s/%s %c %s %s %d %d", rows[7], rows[6], rows[5], rows[4], rows[3], rows[2], rows[1], rows[0],
                                                            activeColor,
                                                            castlingRights,
                                                            enPassantSquare,
                                                            board->movesSinceLastChange,
                                                            (int)board->halfMoveCount/2);
}

void load_fen_string_to_board(bitboard* board, const char* fenString)
{
    assert(board);
    assert(fenString);

    char fullBoardString[80] = {'\0'};
    char* boardStrings[8] = {NULL};
    char activeColor = '-';
    char castlingAvailability[5] = {'\0'};
    char enPassantTargetSquare[3] = {'\0'};
    int halfMoveClock, fullMoveCount = 0;
    sscanf(fenString, "%s %c %s %s %d %d\n", 
        fullBoardString, 
        &activeColor,
        castlingAvailability,
        enPassantTargetSquare,
        &halfMoveClock,
        &fullMoveCount);

    char* strtok_ptr = NULL;
    char* curBoardString = _strtok(fullBoardString, "/", &strtok_ptr);
    int curIndex = 7;
    while(curBoardString && curIndex >= 0)
    {
        boardStrings[curIndex--] = curBoardString;
        curBoardString = _strtok(NULL, "/", &strtok_ptr); 
    }

    for(int sq = 0; sq < 64; sq++) board->pieceArr[sq] = EMPTY_PIECE;
    for(int pc = 0; pc < PIECE_COUNT; pc++) board->pieces[pc] = 0;
    for(int side = 0; side < 2; side++) board->pieces_side[side] = 0;

    for(int row = 0; row < ROW_COUNT; row++)
    {
        if(boardStrings[row][0] != '8')
        {
            int columnOffset = 0;
            int index = 0;
            char currentChar;
            while((currentChar = boardStrings[row][index++]) != '\0' && columnOffset < 8)
            {
                //ASCII values for digits 0-9 (48 to 57 in decimal)
                if(currentChar >= '0'  && currentChar <= '9')
                {
                    columnOffset+= currentChar - '0';
                }
                else
                {
                    int square = columnOffset + 8*row;
                    if(currentChar == 'K')
                    {
                        board->pieces[WHITE_KING]|=singleBitMask(square);
                        board->kingSquare[WHITE] = square;
                        board->pieceArr[square] = KING|WHITE;
                    }
                    else if(currentChar == 'k')
                    {
                        board->pieces[BLACK_KING]|=singleBitMask(square);
                        board->kingSquare[BLACK] = square;
                        board->pieceArr[square] = KING|BLACK;
                    }
                    else if(currentChar == 'Q') 
                    {
                        board->pieces[WHITE_QUEEN]|=singleBitMask(square);
                        board->pieceArr[square] = QUEEN|WHITE;
                    }
                    else if(currentChar == 'q') 
                    {
                        board->pieces[BLACK_QUEEN]|=singleBitMask(square);
                        board->pieceArr[square] = QUEEN|BLACK;
                    }
                    else if(currentChar == 'R') 
                    {
                        board->pieces[WHITE_ROOK]|=singleBitMask(square);
                        board->pieceArr[square] = ROOK|WHITE;
                    }
                    else if(currentChar == 'r') 
                    {
                        board->pieces[BLACK_ROOK]|=singleBitMask(square);
                        board->pieceArr[square] = ROOK|BLACK;
                    }
                    else if(currentChar == 'N') 
                    {
                        board->pieces[WHITE_KNIGHT]|=singleBitMask(square);
                        board->pieceArr[square] = KNIGHT|WHITE;
                    }
                    else if(currentChar == 'n') 
                    {
                        board->pieces[BLACK_KNIGHT]|=singleBitMask(square);
                        board->pieceArr[square] = KNIGHT|BLACK;
                    }
                    else if(currentChar == 'B') 
                    {
                        board->pieces[WHITE_BISHOP]|=singleBitMask(square);
                        board->pieceArr[square] = BISHOP|WHITE;
                    }
                    else if(currentChar == 'b') 
                    {
                        board->pieces[BLACK_BISHOP]|=singleBitMask(square);
                        board->pieceArr[square] = BISHOP|BLACK;
                    }
                    else if(currentChar == 'P') 
                    {
                        board->pieces[WHITE_PAWN]|=singleBitMask(square);
                        board->pieceArr[square] = PAWN|WHITE;
                    }
                    else if(currentChar == 'p') 
                    {
                        board->pieces[BLACK_PAWN]|=singleBitMask(square);
                        board->pieceArr[square] = PAWN|BLACK;
                    }
                    columnOffset++;
                }
            }
        }
    }
    for(int pc = 0; pc < PIECE_COUNT; pc+=2) board->pieces_side[WHITE] |= board->pieces[pc];
    for(int pc = 1; pc < PIECE_COUNT; pc+=2) board->pieces_side[BLACK] |= board->pieces[pc];
    board->pieces_all = board->pieces_side[BLACK]|board->pieces_side[WHITE];

    board->flags = 64;
    //Castling Rights
    if(castlingAvailability[0] != '-')
    {
        for(int i = 0; i < 4; i++)
        {
            if(castlingAvailability[i] == 'K') board->flags|=1;
            else if(castlingAvailability[i] == 'Q') board->flags|=2;
            else if(castlingAvailability[i] == 'k') board->flags|=4;
            else if(castlingAvailability[i] == 'q') board->flags|=8;
            else break;
        }
    }

    //Check
    if(isThreatened(board, board->kingSquare[WHITE], WHITE)) board->flags|=16;
    else if(isThreatened(board, board->kingSquare[BLACK], BLACK)) board->flags|=32;

    //Turn
    if(activeColor == 'w') board->turn = WHITE;
    else board->turn = BLACK;

    //50 move rule
    board->movesSinceLastChange = halfMoveClock;
    board->halfMoveCount = fullMoveCount * 2;

    
    if(enPassantTargetSquare[0] != '-') board->enPassantSquare = getSquareNumber(enPassantTargetSquare);
    else board->enPassantSquare = NO_EP_SQUARE;

    board->hashCode = getHashCode(board);

    memset(board->history, 0, MAX_PLY * sizeof(move_d));
    board->historyIndex = 0;

    memset(board->repetitionHashCodes, 0, 4096 * sizeof(uint64_t));
    board->repetitionIndex = 0;
    board->lastChangeIndex = 0;
    board->repetitionHashCodes[board->repetitionIndex++] = board->hashCode;
}

//Resets the board to an opening position
bitboard* create_board(const char* fenString)
{
    if(!fenString) fenString = STARTPOS_FEN;
    bitboard* board = calloc(1, sizeof(bitboard));
    load_fen_string_to_board(board, fenString);
    return board;
}

void piece_print(char boardArray[8][9], uint64_t piece, char printChar)
{
    while(piece) {
        int square = __builtin_ctzll(piece);
        boardArray[getRow(square)][getColumn(square)] = printChar;
        piece &= (piece - 1);
    }
}

void board_print(bitboard* board, int printValues)
{
    if(!board)
    {
        DEBUG_ERROR("No board.");
        return;
    }
    
    if(printValues) values_print(board);

    int lastMoveStartSquare = -1;
    int lastMoveEndSquare = -1;
    if(board->historyIndex > 0)
    {
        move_c compactMove;
        compactMove.raw = board->history[board->historyIndex - 1].compactMove;
        lastMoveEndSquare = compactMove.endSquare;
        lastMoveStartSquare = compactMove.startSquare;
    }


    //8 rows x (8 columns of spaces + null terminator)
    char boardArray[8][9] = {"        \0", "        \0", "        \0", "        \0", "        \0", "        \0", "        \0", "        \0"};

    piece_print(boardArray, board->pieces[WHITE_PAWN], 'p');
    piece_print(boardArray, board->pieces[BLACK_PAWN], 'P');

    piece_print(boardArray, board->pieces[WHITE_ROOK], 'r');
    piece_print(boardArray, board->pieces[BLACK_ROOK], 'R');

    piece_print(boardArray, board->pieces[WHITE_KNIGHT], 'n');
    piece_print(boardArray, board->pieces[BLACK_KNIGHT], 'N');

    piece_print(boardArray, board->pieces[WHITE_BISHOP], 'b');
    piece_print(boardArray, board->pieces[BLACK_BISHOP], 'B');

    piece_print(boardArray, board->pieces[WHITE_QUEEN], 'q');
    piece_print(boardArray, board->pieces[BLACK_QUEEN], 'Q');

    piece_print(boardArray, board->pieces[WHITE_KING], 'k');
    piece_print(boardArray, board->pieces[BLACK_KING], 'K');

    printf("\n");
    printf(TEXT_BOLD "\t\t%% - - - - - - - - - - %%\n\t\t|                     |\n" TEXT_NONE);
    for(int row = 7; row >= 0; row--)
    {
        printf(TEXT_BOLD "\t\t%d   ", row+1);
        for(int column = 0; column <= 7; column++)
        {
            if(boardArray[row][column] < 91 && boardArray[row][column] > 64)
            {
                if(8 * row + column == lastMoveStartSquare) printf(TEXT_COLOR_SOURCE_BLACK_PIECE);
                else if(8 * row + column == lastMoveEndSquare) printf(TEXT_COLOR_DESTINATION_BLACK_PIECE);
                else
                {
                    if((column + row)&1) printf(TEXT_COLOR_LIGHT_SQUARE_BLACK_PIECE);
                    else printf(TEXT_COLOR_DARK_SQUARE_BLACK_PIECE);
                }
                
                
                boardArray[row][column] = boardArray[row][column]+32;
                printf("%c ", boardArray[row][column]);
            }
            else
            {
                if(8 * row + column == lastMoveStartSquare) printf(TEXT_COLOR_SOURCE_WHITE_PIECE);
                else if(8 * row + column == lastMoveEndSquare) printf(TEXT_COLOR_DESTINATION_WHITE_PIECE);
                else
                {
                    if((column + row)&1) printf(TEXT_COLOR_LIGHT_SQUARE_WHITE_PIECE);
                    else printf(TEXT_COLOR_DARK_SQUARE_WHITE_PIECE);
                }

                printf("%c ", boardArray[row][column]);
            }
        }
        printf(TEXT_NONE TEXT_BOLD"  |" TEXT_NONE "\n ");
    }
    printf(TEXT_BOLD "\t\t|                     |\n \t\t%% - a b c d e f g h - %%\n" TEXT_NONE);
}


void values_print(bitboard* board)
{
    assert(board);

    printf("ALL: %016" PRIx64 "\n", board->pieces_all);
    printf("WHITE/BLACK: %016" PRIx64 " | %016" PRIx64 "\n", board->pieces_side[WHITE], board->pieces_side[BLACK]);
    if(board->pieces[WHITE_PAWN] || board->pieces[BLACK_PAWN]) printf("PAWN: %016" PRIx64 " | %016" PRIx64 "\n", board->pieces[WHITE_PAWN], board->pieces[BLACK_PAWN]);
    if(board->pieces[WHITE_KNIGHT] || board->pieces[BLACK_KNIGHT]) printf("KNIGHT: %016" PRIx64 " | %016" PRIx64 "\n", board->pieces[WHITE_KNIGHT], board->pieces[BLACK_KNIGHT]);
    if(board->pieces[WHITE_BISHOP] || board->pieces[BLACK_BISHOP]) printf("BISHOP: %016" PRIx64 " | %016" PRIx64 "\n", board->pieces[WHITE_BISHOP], board->pieces[BLACK_BISHOP]);
    if(board->pieces[WHITE_ROOK] || board->pieces[BLACK_ROOK]) printf("ROOK: %016" PRIx64 " | %016" PRIx64 "\n", board->pieces[WHITE_ROOK], board->pieces[BLACK_ROOK]);
    if(board->pieces[WHITE_QUEEN] || board->pieces[BLACK_QUEEN]) printf("QUEEN: %016" PRIx64 " | %016" PRIx64 "\n", board->pieces[WHITE_QUEEN], board->pieces[BLACK_QUEEN]);
    printf("KING: %016" PRIx64 " | %016" PRIx64 "\n\t(%d) (%d)\n", board->pieces[WHITE_KING], board->pieces[BLACK_KING], board->kingSquare[WHITE], board->kingSquare[BLACK]);
    

    if(board->flags&0xF)
    {
        printf("Castling Rights: ");
        if(KINGSIDE_CASTLE_WHITE(board->flags)) printf("K");
        if(QUEENSIDE_CASTLE_WHITE(board->flags)) printf("Q");
        if(KINGSIDE_CASTLE_BLACK(board->flags)) printf("k");
        if(QUEENSIDE_CASTLE_BLACK(board->flags)) printf("q");
        printf("\n");
    }

    if(IS_IN_CHECK_ANY(board->flags))
    {
        printf("Check: ");
        if(IS_IN_CHECK_W(board->flags)) printf("W\n");
        else printf("B\n");
        printf("\n");
    }

    printf("HALF-MOVES-SINCE-LAST-CHANGE: %d\n", board->movesSinceLastChange);
    if(board->enPassantSquare != NO_EP_SQUARE) printf("En passant square: %d\n", board->enPassantSquare);
    printf("Hashcode: 0x%016" PRIx64 "\n", board->hashCode);
    char FEN[128];
    export_fen_from_board(board, FEN);  
    printf("FEN: %s\n", FEN);
}


void bitmask_print(uint64_t mask, char fill)
{
    char boardArray[8][9] = {"        \0", "        \0", "        \0", "        \0", "        \0", "        \0", "        \0", "        \0"};
    piece_print(boardArray, mask, fill);
    printf("\nMask: 0x%016" PRIx64 "\n", mask);
    printf(TEXT_BOLD "\t\t%% - - - - - - - - - - %%\n\t\t|                     |\n" TEXT_NONE);
    for(int row = 7; row >= 0; row--)
    {
        printf(TEXT_BOLD "\t\t%d   ", row+1);
        for(int column = 0; column <= 7; column++)
        {
            if(boardArray[row][column] < 91 && boardArray[row][column] > 64)
            {
                if((column + row)&1) printf(TEXT_COLOR_LIGHT_SQUARE_BLACK_PIECE);
                else printf(TEXT_COLOR_DARK_SQUARE_BLACK_PIECE);
                
                boardArray[row][column] = boardArray[row][column]+32;
                printf("%c ", boardArray[row][column]);
            }
            else
            {
                if((column + row)&1) printf(TEXT_COLOR_LIGHT_SQUARE_WHITE_PIECE);
                else printf(TEXT_COLOR_DARK_SQUARE_WHITE_PIECE);

                printf("%c ", boardArray[row][column]);
            }
        }
        printf(TEXT_NONE TEXT_BOLD"  |" TEXT_NONE "\n ");
    }
    printf(TEXT_BOLD "\t\t|                     |\n \t\t%% - a b c d e f g h - %%\n" TEXT_NONE);
}


int moves_push(bitboard* board, move_d m)
{
    assert(board);

    if(board->historyIndex >= MAX_PLY)
    {
        DEBUG_ERROR("Attempted to push a move past limit.");
    }
    else board->history[board->historyIndex++] = m;

    return board->historyIndex;
}

move_d moves_pop(bitboard* board)
{
    assert(board);

    if(board->historyIndex == 0) return (move_d){0};

    return board->history[--board->historyIndex];
}


int containsRepetition(bitboard* board)
{
    int index = board->repetitionIndex - 1;
    int count = 1;
    uint64_t checkedVal = board->repetitionHashCodes[index];
    for(index = index - 4; index >= board->lastChangeIndex; index -= 2)
    {
        if(checkedVal == board->repetitionHashCodes[index])
        {
            count++;
            if(count >= 3) return 1;
        }
    }
    return 0;
}