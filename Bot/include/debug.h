#ifndef DEBUGGER
#define DEBUGGER

#include "structs.h"
#include <stdio.h>
#include <stdlib.h>

extern int printDebugMessages;
extern int trackLeaks;
extern HANDLE leakListLock;

void enableDebugMessages();
void disableDebugMessages();

void enableLeakTracking();
void disableLeakTracking();

#define DEBUG(x, args...) if(printDebugMessages) \
{ \
    printf("ERROR: %s:%d -- ", __FILE__, __LINE__);  \
    printf(x, ##args); \
    printf("\n"); \
}

/*** Memory leak checker ***/
typedef struct memoryBlock {
    void* addr;
    size_t size;
    const char* filename;
    int line;
    struct memoryBlock* next;
} memoryBlock;

extern memoryBlock *allocatedList;

void* allocate_debug(size_t count, size_t size, const char* file, int line);
void* realloc_debug(void* addr, size_t size, const char* file, int line);
void free_debug(void* addr);
void dump_allocations();

#define CALLOC(count, size) allocate_debug(count, size, __FILE__, __LINE__)
#define REALLOC(addr, size) realloc_debug(addr, size, __FILE__, __LINE__)
#define FREE(addr) free_debug(addr)

/***************************/

#endif