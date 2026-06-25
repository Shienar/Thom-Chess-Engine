#ifndef DEBUGGER
#define DEBUGGER

#include "types.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <assert.h>

extern int printDebugMessages;

void enableDebugMessages();
void disableDebugMessages();

void dbg_msg(const char* fileName, int lineNumber, const char* type, const char* str, ...);

#define DEBUG_ERROR(x, ...) dbg_msg(__FILE__, __LINE__, "ERROR", x, ##__VA_ARGS__)
#define DEBUG_INFO(x, ...) dbg_msg(__FILE__, __LINE__, "INFO", x, ##__VA_ARGS__)

#endif