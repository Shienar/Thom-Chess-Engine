#include "debug.h"
#include "compatibility.h"
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>

int print_debug_messages = 0;
char debug_log_file[1024];
FILE* dbg_file = NULL;

int this_pid;

void enable_debug_messages(void) {
	print_debug_messages = 1;
	this_pid = (int) GETPID();
	if (debug_log_file[0] != '\0')
		dbg_file = fopen(debug_log_file, "a");
}

void disable_debug_messages(void) {
	print_debug_messages = 0;
	if (dbg_file) {
		fclose(dbg_file);
		dbg_file = NULL;
	}
}

//Returns an input buffer to make "%s" printing simpler on memory management.
char* comma_delimit_number(char buffer[32], uint64_t value) {
	char temp[32] = {'\0'};
	snprintf(temp, 32, "%" PRIu64, value);
	uint8_t digit_count = strlen(temp);
	uint8_t comma_count = (digit_count - 1) / 3;

	uint8_t write_index = digit_count + comma_count;
	buffer[write_index--] = '\0';

	uint8_t digit_index = 0;
	for (int i = digit_count - 1; i >= 0; i--) {
		buffer[write_index--] = temp[i];
		if (++digit_index == 3 && i > 0) {
			buffer[write_index--] = ',';
			digit_index = 0;
		}
	}

	return buffer;
}

void dbg_msg(const char* file_name, int line_number, const char* type, const char* str, ...) {
	if (!print_debug_messages) return;


	va_list args;
	if (dbg_file) {
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
			file_name,
			line_number);
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