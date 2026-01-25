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
            newTable->array[i].key = CALLOC(HASHKEY_STRING_LENGTH, sizeof(char));
            strncpy(newTable->array[i].key, src->array[i].key, HASHKEY_STRING_LENGTH);
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

double transposition_table_evaluate(bitboard* board, hashtable_tt* tt)
{
    if(!board) return 0;

    if(!tt) 
    {
        tt = create_hashTable_tt();
    }

    char key[HASHKEY_STRING_LENGTH];
    hashKey(board, key);
    uint64_t hashCode = getHashCode(key);
    size_t index = hashCode%tt->capacity;

    while(tt->array[index].key != NULL)
    {
        if(strcmp(tt->array[index].key, key) == 0)
        {
            return tt->array[index].evaluation;
        }

        index++;
        if(index >= tt->capacity) index=0;
    }

    tt->array[index].evaluation = evaluate(board);
    return tt->array[index].evaluation;
}