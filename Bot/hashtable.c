#include "hashtable.h"
#include "bitboard.h"
#include "stdio.h"
#include "debug.h"
#include <string.h>

/**
 * Internal resizing function
 */
void ht_resize(hashtable* ht, int shouldShrink)
{
    size_t newCapacity;
    if(shouldShrink) newCapacity = ht->capacity/2;
    else newCapacity = ht->capacity*2;
     
    table_entry* newArray = CALLOC(newCapacity, sizeof(table_entry));
    if(!newArray) return;

    for(size_t i = 0; i < ht->capacity; i++)
    {
        if(ht->array[i].key != NULL) 
        {
            uint64_t hashCode = getHashCode(ht->array[i].key);
            size_t index = hashCode%newCapacity;

            //Linear insertion
            while(newArray[index].key != NULL)
            {
                index++;
                if(index >= newCapacity) index=0;
            }
            newArray[index].key = ht->array[i].key;
            newArray[index].count = ht->array[i].count;
        }
    }
    FREE(ht->array);
    ht->array = newArray;
    ht->capacity = newCapacity;
}

hashtable* create_hashTable()
{
    hashtable* newTable = CALLOC(1, sizeof(hashtable));
    if(!newTable) return NULL;

    newTable->size = 0;
    newTable->capacity = STARTING_CAPACITY;

    newTable->array = CALLOC(newTable->capacity, sizeof(table_entry));
    if(!newTable->array)
    {
        FREE(newTable);
        return NULL;
    }

    return newTable;
}

hashtable* copy_hashTable(hashtable* src)
{
    hashtable* newTable = CALLOC(1, sizeof(hashtable));
    if(!newTable) return NULL;

    newTable->size = src->size;
    newTable->capacity = src->capacity;

    newTable->array = CALLOC(newTable->capacity, sizeof(table_entry));
    if(!newTable->array)
    {
        FREE(newTable);
        return NULL;
    }

    for(int i = 0; i < newTable->capacity; i++)
    {
        if(src->array[i].key != NULL)
        {
            newTable->array[i].count = src->array[i].count;
            newTable->array[i].key = CALLOC(65, sizeof(char));
            strncpy(newTable->array[i].key, src->array[i].key, 65);
        }
    }

    return newTable;
}

void destroy_hashTable(hashtable* ht)
{
    if(!ht) return;
    if(ht->array)
    {
        for(int i = 0; i < ht->capacity; i++)
        {
            if(ht->array[i].key != NULL) 
            {
                FREE(ht->array[i].key);
                ht->array[i].key = NULL;
            }
        }
    }
    FREE(ht->array);
    FREE(ht);
}

uint64_t getHashCode(const char* key)
{
    if(!key) { printf("X"); return 0; }

    uint64_t hashCode = FNV_OFFSET_BASIS;
    for(int i = 0; i < 64; i++)
    {
        hashCode *= FNV_PRIME;
        hashCode ^= (uint64_t)(unsigned char)(key[i]);
    }
    return hashCode;
}

void hashKey(bitboard* board, char* key)
{
    if(!board) return;

    memset(key, 'e', 64);
    key[64] = '\0';

    uint64_t mask = 1;
    for(int currentSquare = 0; currentSquare < 64; currentSquare++)
    {
        if(board->pawn_w&mask) key[currentSquare] = 'p';
        else if(board->pawn_b&mask) key[currentSquare] = 'P';
        else if(board->rook_w&mask) key[currentSquare] = 'r';
        else if(board->rook_b&mask) key[currentSquare] = 'R';
        else if(board->knight_w&mask) key[currentSquare] = 'n';
        else if(board->knight_b&mask) key[currentSquare] = 'N';
        else if(board->bishop_w&mask) key[currentSquare] = 'b';
        else if(board->bishop_b&mask) key[currentSquare] = 'B';
        else if(board->queen_w&mask) key[currentSquare] = 'q';
        else if(board->queen_b&mask) key[currentSquare] = 'Q';
        else if(board->king_w&mask) key[currentSquare] = 'k';
        else if(board->king_b&mask) key[currentSquare] = 'K';
        mask = mask<<1;
    }
}

int increment_table_value(hashtable* ht, const char* key, int amount)
{
    if(!ht) return INT32_MIN;

    uint64_t hashCode = getHashCode(key);
    int index = hashCode%ht->capacity;

    while(ht->array[index].key != NULL)
    {
        if(strcmp(ht->array[index].key, key) == 0)
        {
            ht->array[index].count += amount;
            return ht->array[index].count;
        }

        index++;
        if(index >= ht->capacity) index=0;
    }

    ht->array[index].key = CALLOC(65, sizeof(char));
    if(ht->array[index].key == NULL) return INT32_MIN;
    strncpy(ht->array[index].key, key, 65);
    ht->array[index].count = amount;
    ht->size++;

    if(ht->size >= ht->capacity/2)
    {
        //Expand
        ht_resize(ht, 0);
    }

    return ht->array[index].count;
}

int decrement_table_value(hashtable* ht, const char* key)
{
    if(!ht) return INT32_MIN;
    uint64_t hashCode = getHashCode(key);
    int index = hashCode%ht->capacity;

    while(ht->array[index].key != NULL)
    {
        if(strcmp(ht->array[index].key, key) == 0) 
        {
            ht->array[index].count--;
            int returnedCount = ht->array[index].count;
            if(ht->array[index].count <= 0)
            {
                FREE(ht->array[index].key);
                ht->array[index].key = NULL;
                ht->array[index].count = 0;
                ht->size--;
            }

            if(ht->size < ht->capacity/4)
            {
                //Shrink
                ht_resize(ht, 1);
            }

            return returnedCount;
        }

        index++;
        if(index >= ht->capacity) index=0;
    }
    return INT32_MIN;
}