#include "debug.h"

int printDebugMessages = 0;
char debugLogFile[1024];
FILE* dbg_file = NULL;

int this_pid;


void enableDebugMessages() 
{
    printDebugMessages = 1;
    this_pid = (int) GETPID();
    if(debugLogFile[0] != '\0')
        dbg_file = fopen(debugLogFile, "a");
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

//Returns an input buffer to make "%s" printing simpler on memory management.
char* commaDelimitNumber(char buffer[32], uint64_t value)
{
    char temp[32] = {'\0'};
    snprintf(temp, 32, "%" PRIu64, value);
    uint8_t digitCount = strlen(temp);
    uint8_t commaCount = (digitCount - 1) / 3;

    uint8_t writeIndex = digitCount + commaCount;
    buffer[writeIndex--] = '\0';

    uint8_t digitIndex = 0;
    for(int i = digitCount - 1; i >= 0; i--)
    {
        buffer[writeIndex--] = temp[i];
        if(++digitIndex == 3 && i > 0)
        {
            buffer[writeIndex--] = ',';
            digitIndex = 0;
        }
    }

    return buffer;
}

void dbg_msg(const char* fileName, int lineNumber, const char* type, const char* str, ...)
{
    if(!printDebugMessages) return;


    va_list args;
    if(dbg_file)
    {
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
    }

    va_start(args, str);
    printf("info string ");
    vprintf(str, args);
    printf("\n");
    va_end(args);
}