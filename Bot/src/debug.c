#include "debug.h"

int printDebugMessages = 0;
FILE* dbg_file = NULL;


void enableDebugMessages() 
{
    printDebugMessages = 1;
    dbg_file = fopen("debug.log", "a");
}
void disableDebugMessages() 
{
    printDebugMessages = 0;
    if(dbg_file)
    {
        fclose(dbg_file);
        dbg_file = NULL;
    }
}

void dbg_msg(const char* fileName, int lineNumber, const char* type, const char* str, ...)
{
    if(!printDebugMessages) return;

    va_list args;
    va_start(args, str);
    fprintf(dbg_file, "%s: %s: %d -- ", type, fileName, lineNumber);
    vfprintf(dbg_file, str, args);
    va_end(args);
    fprintf(dbg_file, "\n");
    fflush(dbg_file);
}