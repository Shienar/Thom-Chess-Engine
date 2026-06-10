#include "analyze/book.h"
#include "debug.h"
#include "board/bitboard.h"
#include "hashtables/hashtable.h"
#include <stdint.h>

uint64_t entryCount = 0;
polyglot_book_entry *entries = NULL;

void loadBook()
{
    if(entries) return;
    FILE* input = fopen(PROJECT_CWD "/import/komodo.bin", "rb");
    if(!input)
    {
        DEBUG_ERROR("Failed to read book file.");
        return;
    }

    //Skip to end of file and count number of entries.
    fseek(input, 0, SEEK_END);
    entryCount = ftell(input);

    if(!entryCount)
    {
        DEBUG_ERROR("Opening book is empty.");
        fclose(input);
        return;
    }

    entryCount/=sizeof(polyglot_book_entry);
    entries = calloc(entryCount, sizeof(polyglot_book_entry));

    
    rewind(input);

    size_t readItems = fread(entries, sizeof(polyglot_book_entry), entryCount, input);

    if(entryCount < readItems) DEBUG_ERROR("%lld/%lld entries imported.", entryCount, readItems);
    fclose(input);
}

void unloadBook()
{
    if(entries)
    {
        free(entries);
        entries = NULL;
    }
}

move getBookMove(bitboard* board)
{
    uint64_t polyglotKey = board->hashCode;
    
    uint32_t totalWeight = 0;
    move potentialMoves[512] = {0};
    uint16_t moveWeights[512] = {0};
    int moveCount = 0;

    for(polyglot_book_entry* entry = entries; entry < &entries[entryCount]; entry++)
    {
        if(polyglotKey == __builtin_bswap64(entry->hashKey))
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
            uint16_t moveBits = __builtin_bswap16(entry->move);
            
            int endSquare = (moveBits&0x7) + 8*((moveBits&0x38)>>3);
            int startSquare = ((moveBits&0x1C0)>>6) + 8*((moveBits&0xE00)>>9);
            int promoteTo = ((moveBits&0x7000)>>12) + 1;
            
            move* tempMove = &potentialMoves[moveCount];
            moveWeights[moveCount] = __builtin_bswap16(entry->weight);
            totalWeight+= moveWeights[moveCount];

            tempMove->promoteTo = promoteTo;
            tempMove->startSquare = startSquare;
            tempMove->endSquare = endSquare;
            tempMove->piece = findPieceOnSquare(board, startSquare);

            //Overwrite the polyglot castling syntax with our own.
            if(ISKING(tempMove->piece))
            {
                if(tempMove->startSquare - tempMove->endSquare == 4) tempMove->endSquare+=2;
                if(tempMove->startSquare - tempMove->endSquare == -3) tempMove->endSquare-=1;
            }

            if((tempMove->capturedPiece = findPieceOnSquare(board, endSquare))) tempMove->capturedPieceSquare = endSquare;

            moveCount++;
            if(moveCount >= 512) break;
        }
    }

    if(moveCount == 0) return (move){0};

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