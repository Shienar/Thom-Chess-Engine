#include "../include/book.h"
#include "../include/debug.h"
#include "../include/bitboard.h"
#include "../include/hashtable.h"
#include <stdint.h>

uint64_t entryCount = 0;
polyglot_book_entry *entries = NULL;

void loadBook()
{
    printf("Loading opening book...\n");
    FILE* input = fopen("import/komodo.bin", "rb");

    if(!input)
    {
        DEBUG("Failed to read book file.")
        return;
    }

    //Skip to end of file and count number of entries.
    fseek(input, 0, SEEK_END);
    entryCount = ftell(input);

    if(!entryCount)
    {
        DEBUG("Opening book is empty.")
        return;
    }

    entryCount/=sizeof(polyglot_book_entry);
    entries = CALLOC(entryCount, sizeof(polyglot_book_entry));

    
    rewind(input);

    size_t readItems = fread(entries, sizeof(polyglot_book_entry), entryCount, input);

    printf("%lld/%lld entries imported.", entryCount, readItems);
}

void unloadBook()
{
    printf("Unloading book...\n");
    if(entries)
    {
        FREE(entries);
        entries = NULL;
    }
}

typedef struct weight_node {
    uint16_t weight;
    struct weight_node* next;
} weight_node;

move* getBookMove(bitboard* board)
{
    uint64_t polyglotKey = getHashCode(board);
    
    uint32_t totalWeight = 0;
    move* moveHead = NULL;
    weight_node* weightHead  = NULL;

    for(polyglot_book_entry* entry = entries; entry < &entries[entryCount + 1]; entry++)
    {
        if(polyglotKey == _byteswap_uint64(entry->hashKey))
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
            uint16_t moveBits = _byteswap_ushort(entry->move);
            
            int endSquare = (moveBits&0x7) + 8*((moveBits&0x38)>>3);
            int startSquare = ((moveBits&0x1C0)>>6) + 8*((moveBits&0xE00)>>9);
            int promoteTo = ((moveBits&0x7000)>>12) + 1;
            
            move* tempMove = CALLOC(1, sizeof(move));
            weight_node* tempWeight = CALLOC(1, sizeof(weight_node));
            tempWeight->weight = _byteswap_ushort(entry->weight);
            totalWeight+= tempWeight->weight;

            tempMove->promoteTo = promoteTo;
            tempMove->startSquare = startSquare;
            tempMove->endSquare = endSquare;
            tempMove->flags = board->flags;
            tempMove->previousMovesSinceLastChange = board->movesSinceLastChange;
            tempMove->piece = findPieceOnSquare(board, startSquare);
            tempMove->nextMove = NULL;

            //Overwrite the polyglot castling syntax with our own.
            if(ISKING(tempMove->piece))
            {
                if(tempMove->startSquare - tempMove->endSquare == 4) tempMove->endSquare+=2;
                if(tempMove->startSquare - tempMove->endSquare == -3) tempMove->endSquare-=1;
            }

            if((tempMove->capturedPiece = findPieceOnSquare(board, endSquare))) tempMove->capturedPieceSquare = endSquare;

            tempMove->nextMove = moveHead;
            moveHead = tempMove;
            tempWeight->next = weightHead;
            weightHead = tempWeight;
        }
    }

    if(!moveHead) return NULL;

    uint32_t randomValue = ((rand()<<16)|rand())%totalWeight;
    move* returnedMove = NULL;
    move* tempMove;
    weight_node* tempWeight;
    for(tempMove = moveHead, tempWeight = weightHead; tempMove != NULL; tempMove=tempMove->nextMove, tempWeight=tempWeight->next)
    {
        if(randomValue < tempWeight->weight || !tempMove->nextMove)
        {
            returnedMove = tempMove;
            break;
        }
        else randomValue-= tempWeight->weight;
    }

    move* prevMove = moveHead;
    weight_node* prevWeight = weightHead;
    tempMove = moveHead->nextMove;
    tempWeight = weightHead->next;

    while(prevMove && prevWeight)
    {
        if(prevMove != returnedMove) FREE(prevMove);
        FREE(prevWeight);

        prevMove = tempMove;
        if(tempMove) tempMove = tempMove->nextMove;

        prevWeight = tempWeight;
        if(tempWeight) tempWeight = tempWeight->next;
    }

    returnedMove->nextMove = NULL;
    return returnedMove;
    
}