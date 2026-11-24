#pragma once

#include <stdint.h>

extern int print_debug_messages;

extern char debug_log_file[1024];

void enable_debug_messages(void);
void disable_debug_messages(void);

char *comma_delimit_number(char buffer[32], uint64_t value);

void dbg_msg(const char *file_name, int line_number, const char *type, const char *str, ...);

#define DEBUG_ERROR(x, ...) dbg_msg(__FILE__, __LINE__, "ERROR", x, ##__VA_ARGS__)
#define DEBUG_INFO(x, ...) dbg_msg(__FILE__, __LINE__, "INFO", x, ##__VA_ARGS__)
