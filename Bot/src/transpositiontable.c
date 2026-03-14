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
    if(!ht) return;
    if(ht->array) FREE(ht->array);
    FREE(ht);
}

uint8_t generateChecksum(table_entry_tt* entry)
{
    const uint8_t* ptr = (const uint8_t*)entry;
    uint8_t checksum = 0;
    for(int i = 0; i < sizeof(entry) - 1; i++)  checksum^=ptr[i];
    return checksum;
}

table_entry_tt* transposition_table_get(bitboard* board, hashtable_tt* tt)
{
    if(!board || !tt) return NULL;

    uint64_t hashCode = getHashCode(board);
    size_t index = hashCode%tt->capacity;
    size_t startIndex = index;

    while(tt->array[index].hashCode != 0)
    {
        if(tt->array[index].hashCode == hashCode)
        {
            uint8_t checkSum = generateChecksum(&tt->array[index]);
            if(tt->array[index].checkSum == checkSum) return &tt->array[index];
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
        DEBUG("Transposition table is nil.");
        return;
     }

    size_t index = entry.hashCode%tt->capacity;
    size_t startIndex = index;

    (&entry)->checkSum = generateChecksum(&entry); 

    while(tt->array[index].hashCode != 0)
    {
        if(tt->array[index].hashCode == entry.hashCode && tt->array[index].checkSum == entry.checkSum)
        {
            //Update
            if(entry.evaluationDepth > tt->array[index].evaluationDepth || (entry.nodeType == NODE_TYPE_PV && entry.nodeType != tt->array[index].nodeType)) tt->array[index] = entry;
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
        //Find a replacement. (Always replace)
        //I don't want multiple threads fighting over the oldest spot.
        tt->array[startIndex] = entry;
    }
    else
    {
        tt->size++;
        tt->array[index] = entry;
    }
}