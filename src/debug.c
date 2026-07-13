#include "debug.h"

int printDebugMessages = 0;
FILE* dbg_file = NULL;

int this_pid;


void enableDebugMessages() 
{
    printDebugMessages = 1;
    this_pid = (int) GETPID();
    #ifndef RELEASE
    dbg_file = fopen(PROJECT_CWD "/debug.log", "a");
    #endif
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
    #ifndef RELEASE
    time_t t = time(NULL);
    struct tm *local_time = localtime(&t);
    
    va_start(args, str);
    fprintf(dbg_file, "%06d | %02d-%02d-%04d %02d:%02d:%02d | %s: %s: %d -- ", 
           this_pid,
           local_time->tm_mon + 1,
           local_time->tm_mday,
           local_time->tm_year + 1900,
           local_time->tm_hour,
           local_time->tm_min,
           local_time->tm_sec,
           type, 
           fileName, 
           lineNumber);
    vfprintf(dbg_file, str, args);
    fprintf(dbg_file, "\n");
    fflush(dbg_file);
    va_end(args);
    #endif

    va_start(args, str);
    printf("info string ");
    vprintf(str, args);
    printf("\n");
    va_end(args);
}