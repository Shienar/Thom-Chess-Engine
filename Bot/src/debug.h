#ifndef DEBUGGER
#define DEBUGGER

#include "structs.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

extern int printDebugMessages;
extern int trackLeaks;
extern HANDLE leakListLock;

void enableDebugMessages();
void disableDebugMessages();

void enableLeakTracking();
void disableLeakTracking();

void dbg_msg(const char* fileName, int lineNumber, const char* str, ...);

#define DEBUG(x, ...) dbg_msg(__FILE__, __LINE__, x, ##__VA_ARGS__)

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