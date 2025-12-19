#ifndef DEBUG
#include <stdio.h>

extern int printDebugMessages;

#define DEBUG(x, args...) if(printDebugMessages) \
{ \
    printf("ERROR: %s:%d -- ", __FILE__, __LINE__);  \
    printf(x, ##args); \
    printf("\n"); \
}
void enableDebugMessages();
void disableDebugMessages();

#endif