#include "compatibility.h"
#include "analyze/book.h"
#include "debug.h"
#include "board/bitboard.h"
#include "hashtables/hashtable.h"
#include <stdint.h>

int initBook = 0;
int useBook = 0;
uint64_t entryCount = 0;
polyglot_book_entry *entries = NULL;

void loadBook()
{
    if(initBook) 
        return;
    initBook = 1;

    entries = (polyglot_book_entry*) book_bin_start;
    entryCount = (uint64_t) (book_bin_end - book_bin_start);
    entryCount /= sizeof(polyglot_book_entry);
}

move_c getBookMove(bitboard* board)
{
    if(!IS_IN_BOOK_OPENING(board->flags) || !useBook || board->repetitionIndex > MAX_BOOK_PLY) return (move_c) {0};
    uint64_t polyglotKey = board->hashCode;
    
    uint32_t totalWeight = 0;
    move_c potentialMoves[256] = {0};
    uint16_t moveWeights[256] = {0};
    int moveCount = 0;

    for(polyglot_book_entry* entry = entries; entry < &entries[entryCount]; entry++)
    {
        if(polyglotKey == bigEndian64(entry->hashKey))
        {
            /** Bits:
             *  0,1,2               to file
             *  3,4,5               to row
             *  6,7,8               from file
             *  9,10,11             from row
             *  12,13,14            promotion piece
             * 
             * Promotion: 1 less than my enums.
             *  - 1 = Knight
             *  - 2 = Bishop
             *  - 3 = Rook
             *  - 4 = Queen
             */
            uint16_t moveBits = bigEndian16(entry->move);
            
            int endSquare = (moveBits&0x7) + 8*((moveBits&0x38)>>3);
            int startSquare = ((moveBits&0x1C0)>>6) + 8*((moveBits&0xE00)>>9);
            int promoteTo = ((moveBits&0x7000)>>12) + 1;
            
            move_c* tempMove = &potentialMoves[moveCount];
            moveWeights[moveCount] = bigEndian16(entry->weight);
            totalWeight+= moveWeights[moveCount];

            tempMove->promoteTo = promoteTo;
            tempMove->startSquare = startSquare;
            tempMove->endSquare = endSquare;
            int piece = findPieceOnSquare(board, startSquare);

            //Overwrite the polyglot castling syntax with our own.
            if(ISKING(piece))
            {
                if(tempMove->startSquare - tempMove->endSquare == 4) tempMove->endSquare+=2;
                if(tempMove->startSquare - tempMove->endSquare == -3) tempMove->endSquare-=1;
            }

            moveCount++;
            if(moveCount >= 512) break;
        }
    }

    if(moveCount == 0) 
    {
        LEAVE_BOOK_OPENING(board->flags);
        return (move_c){0};
    }

    uint32_t randomValue = (((uint32_t)rand()) & 0xFFFF) | ((((uint32_t)rand()) & 0xFFFF) << 16);
    randomValue = randomValue%totalWeight;
    int selectedIndex = 0;

    for(int i = 0; i < moveCount; i++) 
    {
        if(randomValue < moveWeights[i]) 
        {
            selectedIndex = i;
            break;
        }
        randomValue -= moveWeights[i];
    }

    return potentialMoves[selectedIndex];
}