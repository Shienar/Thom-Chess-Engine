#include "debug.h"

int printDebugMessages = 0;


void enableDebugMessages() {printDebugMessages = 1;}
void disableDebugMessages() {printDebugMessages = 0;}

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