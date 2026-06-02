CC = gcc
CFLAGS = -Wall -g -std=c99 -march=native -fopenmp 
TARGET = ChessBot.exe
LIBS = -lOpenCL

OBJFILES = src/*.c
OBJFILES += src/analyze/*.c
OBJFILES += src/board/*.c
OBJFILES += src/hashtables/*.c
OBJFILES += src/pyrrhic/tbprobe.c
OBJFILES += src/gpu/*.c

# No debug flag for assert() removals.
# CFLAGS += -DNDEBUG

# Speedup, harder debugging.
# CFLAGS += -O3

# Kernel profiling, no ping-pong buffers.
# CFLAGS += -DPERFT_KERNELS

# Extra CFLAGS for gprof analysis.
# Need to run the executable first to generate gmon.out file.
# gprof ./ChessBot.exe -P_mcount_private -P__fentry__ -b > output.txt
# CFLAGS += -pg -no-pie

# Add -pthread flag if on linux or macOS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LIBS += -pthread -lm
endif
ifeq ($(UNAME_S),Darwin)
    LIBS += -pthread
endif

all: clean $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) $(LIBS)

clean:
	rm -f $(TARGET)