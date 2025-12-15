#ifndef DEBUG

#include <stdio.h>

#define TOGGLEDEBUG 1
#define DEBUG(x, args...) if(TOGGLEDEBUG) \
{ \
    printf("ERROR: %s:%d -- ", __FILE__, __LINE__);  \
    printf(x, ##args); \
    printf("\n"); \
}

#endif