#include "../include/repetitiontable.h"
#include "../include/debug.h"
#include <float.h>
#include <string.h>

/**
 * Internal resizing functions
 */
void ht_resize_pos(hashtable_pos* ht, int shouldShrink)
{
    size_t newCapacity;
    if(shouldShrink) newCapacity = ht->capacity/2;
    else newCapacity = ht->capacity*2;
     
    table_entry_pos* newArray = CALLOC(newCapacity, sizeof(table_entry_pos));
    if(!newArray) return;

    for(size_t i = 0; i < ht->capacity; i++)
    {
        if(ht->array[i].count != 0) 
        {
            uint64_t hashCode = getHashCode(ht->array[i].key);
            size_t index = hashCode%newCapacity;

            //Linear insertion
            while(newArray[index].count != 0)
            {
                index++;
                if(index >= newCapacity) index=0;
            }
            memcpy(&newArray[index], &ht->array[i], sizeof(table_entry_pos));
        }
    }
    FREE(ht->array);
    ht->array = newArray;
    ht->capacity = newCapacity;
}

hashtable_pos* create_hashTable_pos()
{
    hashtable_pos* newTable = CALLOC(1, sizeof(hashtable_pos));
    if(!newTable) return NULL;

    newTable->size = 0;
    newTable->capacity = STARTING_CAPACITY;

    newTable->array = CALLOC(newTable->capacity, sizeof(table_entry_pos));
    if(!newTable->array)
    {
        FREE(newTable);
        return NULL;
    }

    return newTable;
}

hashtable_pos* copy_hashTable_pos(hashtable_pos* src)
{
    hashtable_pos* newTable = CALLOC(1, sizeof(hashtable_pos));
    if(!newTable) return NULL;

    newTable->size = src->size;
    newTable->capacity = src->capacity;

    newTable->array = CALLOC(newTable->capacity, sizeof(table_entry_pos));
    if(!newTable->array)
    {
        FREE(newTable);
        return NULL;
    }

    for(size_t i = 0; i < newTable->capacity; i++)
    {
        if(src->array[i].count != 0)
        {
            newTable->array[i].count = src->array[i].count;
            strncpy(newTable->array[i].key, src->array[i].key, HASHKEY_STRING_LENGTH);
        }
    }

    return newTable;
}

void destroy_hashTable_pos(hashtable_pos* ht)
{
    if(!ht) FREE(ht);
    if(ht->array) FREE(ht->array);
}

double increment_table_value(hashtable_pos* ht, const char* key, double amount)
{
    if(!ht) return DBL_MIN;

    uint64_t hashCode = getHashCode(key);
    size_t index = hashCode%ht->capacity;

    while(ht->array[index].count != 0 && ht->array[index].count != DBL_MIN)
    {
        if(strcmp(ht->array[index].key, key) == 0)
        {
            ht->array[index].count += amount;
            return ht->array[index].count;
        }

        index++;
        if(index >= ht->capacity) index=0;
    }

    strncpy(ht->array[index].key, key, HASHKEY_STRING_LENGTH);
    ht->array[index].count = amount;
    ht->size++;

    if(ht->size >= ht->capacity/2)
    {
        //Expand
        ht_resize_pos(ht, 0);
    }

    return ht->array[index].count;
}

double decrement_table_value(hashtable_pos* ht, const char* key)
{
    if(!ht) return DBL_MIN;
    uint64_t hashCode = getHashCode(key);
    size_t index = hashCode%ht->capacity;

    while(ht->array[index].count != 0 && ht->array[index].count != DBL_MIN)
    {
        if(strcmp(ht->array[index].key, key) == 0) 
        {
            ht->array[index].count--;
            double returnedCount = ht->array[index].count;
            if(ht->array[index].count <= 0)
            {
                ht->array[index].count = DBL_MIN; //Tombstone
                ht->size--;
            }

            if(ht->size < ht->capacity/4)
            {
                //Shrink
                ht_resize_pos(ht, 1);
            }

            return returnedCount;
        }

        index++;
        if(index >= ht->capacity) index=0;
    }
    return DBL_MIN;
}
