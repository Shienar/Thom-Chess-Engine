CC = gcc
CFLAGS = -Wall -g -std=c99 -march=native
TARGET = ChessBot

OBJFILES = src/*.c
OBJFILES += src/analyze/*.c
OBJFILES += src/board/*.c
OBJFILES += src/hashtables/*.c
OBJFILES += src/pyrrhic/tbprobe.c

# No debug flag for assert() removals.
# CFLAGS += -NDEBUG

# Thread count
CFLAGS+= -DHELPER_THREAD_COUNT=8

# Track nodes visited.
# CFLAGS+= -DCOUNT_NODES_VISITED=1

# Extra CFLAGS for gprof analysis.
# Need to run the executable first to generate gmon.out file.
# gprof ./ChessBot.exe -b > output.txt
# CFLAGS += -pg -no-pie

all: clean $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES)

clean:
	rm -f $(TARGET) *~ leaks.txt