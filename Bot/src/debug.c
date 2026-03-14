#include "../include/debug.h"

int printDebugMessages = 0;
int trackLeaks = 0;
memoryBlock *allocatedList = NULL;
HANDLE leakListLock = NULL;


void enableDebugMessages() {printDebugMessages = 1;}
void disableDebugMessages() {printDebugMessages = 0;}
void enableLeakTracking() {
    trackLeaks = 1;
    leakListLock = CreateMutex(NULL, 0, NULL);
    if(!leakListLock) DEBUG("Failed to created leak list lock.");
}
void disableLeakTracking() {
    trackLeaks = 0;
    CloseHandle(leakListLock);
    leakListLock = NULL;
}

void dbg_msg(const char* fileName, int lineNumber, const char* str, ...)
{
    if(!printDebugMessages) return;

    va_list args;
    va_start(args, str);
    printf("ERROR: %s: %d -- ", fileName, lineNumber);
    vprintf(str, args);
    va_end(args);
    printf("\n");
}

void* allocate_debug(size_t count, size_t size, const char* file, int line)
{
    void* returnAddr = calloc(count, size);
    
    if(trackLeaks && returnAddr != NULL)
    {
        WaitForSingleObject(leakListLock, INFINITE);
        memoryBlock* newNodeHead = calloc(1, sizeof(memoryBlock));
        if(!newNodeHead)
        {
            free(returnAddr);
            ReleaseMutex(leakListLock);
            return NULL;
        }

        newNodeHead->size = size;
        newNodeHead->addr = returnAddr;
        newNodeHead->filename = file;
        newNodeHead->line = line;
        newNodeHead->next = allocatedList;
        allocatedList = newNodeHead;
        ReleaseMutex(leakListLock);
    }

    return returnAddr;
}


void* realloc_debug(void* addr, size_t size, const char* file, int line)
{
    void* returnAddr = realloc(addr, size);

    if(trackLeaks && returnAddr != NULL)
    {
        WaitForSingleObject(leakListLock, INFINITE);
        for(memoryBlock* curNode = allocatedList; curNode != NULL; curNode = curNode->next)
        {
            if(curNode->addr == addr)
            {
                curNode->addr = returnAddr;
                curNode->size = size;
                curNode->filename = file;
                curNode->line = line;
                break;
            }
        }
        ReleaseMutex(leakListLock);
    }

    return returnAddr;
}

void free_debug(void* addr)
{
    if(!addr) return;

    if(trackLeaks)
    {
        WaitForSingleObject(leakListLock, INFINITE);
        memoryBlock* prevNode = NULL;
        for(memoryBlock* curNode = allocatedList; curNode != NULL; curNode = curNode->next)
        {
            if(curNode->addr == addr)
            {
                if(prevNode) prevNode->next = curNode->next;
                else allocatedList = curNode->next;
                
                free(curNode);
                break;
            }
            prevNode = curNode;
        }
        ReleaseMutex(leakListLock);
    }

    free(addr);
}

void dump_allocations()
{
    if(trackLeaks)
    {   
        FILE* output = fopen("leaks.txt", "w");
        if(!output) return;
        if(allocatedList)
        {
            for(memoryBlock* curNode = allocatedList; curNode != NULL; curNode = curNode->next)
            {
                fprintf(output, "%s:%d - Allocated address <%p> of size %lld\n", curNode->filename, curNode->line, curNode->addr, curNode->size);
            }
        }
        else
        {
            fprintf(output, "No leaks detected!");
        }
        
        fclose(output);
    }
}