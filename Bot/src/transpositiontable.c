#include "../include/transpositiontable.h"
#include "../include/debug.h"
#include <string.h>

/**
 * Internal resizing functions
 */
void ht_resize_tt(hashtable_tt* ht, int shouldShrink)
{
    size_t newCapacity;
    if(shouldShrink) newCapacity = ht->capacity/2;
    else newCapacity = ht->capacity*2;
     
    table_entry_tt* newArray = CALLOC(newCapacity, sizeof(table_entry_tt));
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
            memcpy(&newArray[index], &ht->array[i], sizeof(table_entry_tt));
        }
    }
    FREE(ht->array);
    ht->array = newArray;
    ht->capacity = newCapacity;
}

hashtable_tt* create_hashTable_tt()
{
    hashtable_tt* newTable = CALLOC(1, sizeof(hashtable_tt));
    if(!newTable) return NULL;

    newTable->size = 0;
    newTable->capacity = STARTING_CAPACITY;

    newTable->array = CALLOC(newTable->capacity, sizeof(table_entry_tt));
    if(!newTable->array)
    {
        FREE(newTable);
        return NULL;
    }

    return newTable;
}

hashtable_tt* copy_hashTable_tt(hashtable_tt* src)
{
    hashtable_tt* newTable = CALLOC(1, sizeof(hashtable_tt));
    if(!newTable) return NULL;

    newTable->size = src->size;
    newTable->capacity = src->capacity;

    newTable->array = CALLOC(newTable->capacity, sizeof(table_entry_tt));
    if(!newTable->array)
    {
        FREE(newTable);
        return NULL;
    }

    for(size_t i = 0; i < newTable->capacity; i++)
    {
        if(src->array[i].key != NULL)
        {
            memcpy(&newTable->array[i], &src->array[i], sizeof(table_entry_tt));

            //Create different address pointer
            newTable->array[i].key = CALLOC(65, sizeof(char));
            strncpy(newTable->array[i].key, src->array[i].key, 65);
        }
    }

    return newTable;
}

void destroy_hashTable_tt(hashtable_tt* ht)
{
    if(!ht) return;
    if(ht->array)
    {
        for(size_t i = 0; i < ht->capacity; i++)
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

double transposition_table_evaluate(bitboard* board, engine* engine)
{
    if(!engine || !board) return 0;

    if(engine->transpositionTable == NULL) 
    {
        engine->transpositionTable = create_hashTable_tt();
    }

    char key[65];
    hashKey(board, key);
    uint64_t hashCode = getHashCode(key);
    size_t index = hashCode%engine->transpositionTable->capacity;

    while(engine->transpositionTable->array[index].key != NULL)
    {
        if(strcmp(engine->transpositionTable->array[index].key, key) == 0)
        {
            return engine->transpositionTable->array[index].evaluation;
        }

        index++;
        if(index >= engine->transpositionTable->capacity) index=0;
    }

    engine->transpositionTable->array[index].evaluation = evaluate(board, engine);
    return engine->transpositionTable->array[index].evaluation;
}