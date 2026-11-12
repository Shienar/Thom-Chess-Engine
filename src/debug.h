#pragma once

#include <stdint.h>

extern int printDebugMessages;

extern char debugLogFile[1024];

void enableDebugMessages();
void disableDebugMessages();

char* commaDelimitNumber(char buffer[32], uint64_t value);

void dbg_msg(const char* fileName, int lineNumber, const char* type, const char* str, ...);

#define DEBUG_ERROR(x, ...) dbg_msg(__FILE__, __LINE__, "ERROR", x, ##__VA_ARGS__)
#define DEBUG_INFO(x, ...) dbg_msg(__FILE__, __LINE__, "INFO", x, ##__VA_ARGS__)