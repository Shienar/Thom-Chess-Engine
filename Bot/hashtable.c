#include "hashtable.h"
#include "bitboard.h"
#include "stdio.h"
#include <string.h>
#include <stdlib.h>

hashtable* create_hashTable()
{
    hashtable* newTable = calloc(1, sizeof(hashtable));
    if(!newTable) return NULL;

    newTable->size = 0;
    newTable->capacity = STARTING_CAPACITY;

    newTable->array = calloc(STARTING_CAPACITY, sizeof(table_entry));
    if(!newTable->array)
    {
        free(newTable);
        return NULL;
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
            if(ht->array[i].key != NULL) free(ht->array[i].key);
        }
    }
    free(ht->array);
    free(ht);
}

uint64_t getHashCode(char* key)
{
    uint64_t hashCode = FNV_OFFSET_BASIS;
    for(char* c = key; *c != '\0'; c++)
    {
        hashCode *= FNV_PRIME;
        hashCode ^= (uint64_t)(unsigned char)(*c);
    }
    return hashCode;
}

char* hashKey(bitboard* board)
{
    if(!board) return NULL;
    char* key = calloc(65, sizeof(char));
    if(!key) return NULL;

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

    return key;
}

int increment_table_value(hashtable* ht, char* key, int amount)
{
    if(!ht) return INT32_MIN;

    uint64_t hashCode = getHashCode(key);
    int index = hashCode%ht->capacity;

    while(ht->array[index].key != NULL)
    {
        if(strcmp(ht->array[index].key, key) == 0)
        {
            ht->array[index].count+= amount;
            free(key);
            return ht->array[index].count;
        }

        index++;
        if(index >= ht->capacity) index=0;
    }

    ht->array[index].key = key;
    ht->array[index].count = amount;
    ht->size++;

    if(ht->size >= ht->capacity/2)
    {
        //Expand
        ht->capacity*=2;
        table_entry* oldTableArray = ht->array;
        ht->array = calloc(ht->capacity, sizeof(table_entry));
        if(!ht->array) 
        {
            ht->array = oldTableArray;
            ht->capacity/=2;
            return INT32_MIN;
        }

        for(int i = 0; i < ht->capacity/2; i++)
        {
            if(oldTableArray[i].key) increment_table_value(ht, oldTableArray[i].key, oldTableArray[i].count);
        }

        free(oldTableArray);
    }

    return ht->array[index].count;
}

int decrement_table_value(hashtable* ht, char* key)
{
    if(!ht) 
    {
        free(key);
        return INT32_MIN;
    }
    uint64_t hashCode = getHashCode(key);
    int index = hashCode%ht->capacity;

    while(ht->array[index].key != NULL)
    {
        if(strcmp(ht->array[index].key, key) == 0) 
        {
            ht->array[index].count--;
            if(ht->array[index].count <= 0)
            {
                free(ht->array[index].key);
                free(key);
                ht->array[index].key = NULL;
                ht->array[index].count = 0;
                ht->size--;

                if(ht->size <= ht->capacity/4)
                {
                    //Shrink
                    ht->capacity/=2;
                    table_entry* oldTableArray = ht->array;
                    ht->array = calloc(ht->capacity, sizeof(table_entry));
                    if(!ht->array) 
                    {
                        ht->capacity*=2;
                        ht->array = oldTableArray;
                        return INT32_MIN;
                    }

                    for(int i = 0; i < ht->capacity*2; i++)
                    {
                        if(oldTableArray[i].key) increment_table_value(ht, oldTableArray[i].key, oldTableArray[i].count);
                    }

                    free(oldTableArray);
                }
            }
            return ht->array[index].count;
        }

        index++;
        if(index >= ht->capacity) index=0;
    }
    free(key);
    return INT32_MIN;
}