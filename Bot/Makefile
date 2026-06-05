CC = gcc
CFLAGS = -Wall -g -std=c99 -march=native -fopenmp
TARGET = ChessBot.exe
LIBS = -lOpenCL

SRCFILES = src/*.c
SRCFILES += src/analyze/*.c
SRCFILES += src/board/*.c
SRCFILES += src/hashtables/*.c
SRCFILES += src/pyrrhic/tbprobe.c
SRCFILES += src/gpu/*.c

# Speedup, harder debugging.
CFLAGS += -O3 -DNDEBUG

# Kernel profiling for trainer, no ping-pong buffers.
# CFLAGS += -DPERFT_KERNELS

# Extra CFLAGS for gprof analysis.
# Need to run the executable first to generate gmon.out file.
# gprof ./ChessBot.exe -P_mcount_private -P__fentry__ -b > output.txt
# CFLAGS += -pg -no-pie

# Linux / MacOS specific compilation flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LIBS += -pthread -lm
endif
ifeq ($(UNAME_S),Darwin)
    LIBS += -pthread
endif

all: clean $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCFILES) $(LIBS)

clean:
	rm -f $(TARGET)