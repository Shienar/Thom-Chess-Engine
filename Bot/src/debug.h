#ifndef DEBUGGER
#define DEBUGGER

#include "types.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

//#define NDEBUG
#include <assert.h>

extern int printDebugMessages;

void enableDebugMessages();
void disableDebugMessages();

void dbg_msg(const char* fileName, int lineNumber, const char* str, ...);

#define DEBUG(x, ...) dbg_msg(__FILE__, __LINE__, x, ##__VA_ARGS__)

#endif