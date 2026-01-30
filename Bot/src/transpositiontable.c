#include "../include/transpositiontable.h"
#include "../include/debug.h"
#include <string.h>

hashtable_tt* transpositionTable = NULL;

hashtable_tt* create_hashTable_tt()
{
    hashtable_tt* newTable = CALLOC(1, sizeof(hashtable_tt));
    if(!newTable) return NULL;

    newTable->size = 0;
    newTable->capacity = MAX_TABLE_SIZE;

    newTable->array = CALLOC(newTable->capacity, sizeof(table_entry_tt));
    if(!newTable->array)
    {
        FREE(newTable);
        return NULL;
    }

    return newTable;
}

void destroy_hashTable_tt(hashtable_tt* ht)
{
    if(!ht) FREE(ht);
    if(ht->array) FREE(ht->array);
    
}

table_entry_tt* transposition_table_get(bitboard* board, hashtable_tt* tt)
{
    if(!board || !tt) return NULL;

    char key[HASHKEY_STRING_LENGTH];
    hashKey(board, key);
    size_t index = getHashCode(key)%tt->capacity;
    size_t startIndex = index;

    while(tt->array[index].age != 0)
    {
        if(strcmp(tt->array[index].key, key) == 0)
        {
            return &tt->array[index];
        }

        index++;
        if(index >= tt->capacity) index=0;
        if(index == startIndex) break;
    }

    return NULL;
}


void transposition_table_set(hashtable_tt* tt, table_entry_tt entry)
{
     if(!tt)
     {
        DEBUG("Transposition table is nil.")
        return;
     }

    size_t index = getHashCode(entry.key)%tt->capacity;
    size_t startIndex = index;

    while(tt->array[index].age != 0)
    {
        if(strcmp(tt->array[index].key, entry.key) == 0)
        {
            //Update
            tt->array[index] = entry;
            return;
        }

        index++;
        if(index >= tt->capacity) index=0;
        if(index == startIndex)
        {
            //Hashtable is full.
            index = -1;
            break;
        }
    }

    //Insert
    if(index == -1)
    {
        //Find a replacement. (Oldest)
        clock_t oldestTime = LONG_MAX;
        for(size_t i = 0; i < tt->capacity; i++)
        {
            if(tt->array[i].age != 0 && tt->array[i].age < oldestTime) 
            {
                index = i;
                oldestTime = tt->array[i].age;
            }
        }
        tt->array[index] = entry;
    }
    else
    {
        tt->size++;
        tt->array[index] = entry;
    }
}