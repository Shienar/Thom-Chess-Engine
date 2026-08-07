# Compiler and flags
CC = gcc
CFLAGS = -Wall -std=c99 -march=native -fopenmp -MMD -MP

# Add debug or optimization flags to compiler flags
ifdef DEBUG
	CFLAGS += -g
else
	CFLAGS += -O3
	CFLAGS += -DNDEBUG
endif

# Define SPSA to compile with extra uci options.
ifdef SPSA
	CFLAGS += -DSPSA
endif

# Declare & include source/target.
SRC_DIR = src
TGT_DIR = target
OBJ_DIR = $(TGT_DIR)/obj

CFLAGS += -I$(SRC_DIR)

# Simple method to compile a copy for SPRT testing.
ifdef NEW
	TARGET = $(TGT_DIR)/Thom_new.exe
else
	TARGET = $(TGT_DIR)/Thom.exe
endif

SRCFILES = $(wildcard src/*.c) \
		   $(wildcard src/analyze/*.c) \
		   $(wildcard src/board/*.c) \
		   $(wildcard src/hashtables/*.c) \
		   $(wildcard src/pyrrhic/tbprobe.c) \
 		   $(wildcard src/binpack/*c) \
		   $(wildcard src/analyze/hce/*.c) \
		   $(wildcard src/analyze/nnue/*.c)

ASMFILES = $(wildcard src/incbin/*.s)

OBJFILES := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCFILES))
ASMOBJFILES := $(patsubst $(SRC_DIR)/%.s, $(OBJ_DIR)/%.o, $(ASMFILES))
OBJFILES += $(ASMOBJFILES)
DEPS     := $(OBJFILES:.o=.d)

OBJ_DIRS := $(sort $(dir $(OBJFILES)))

# Linux / MacOS specific libaries
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LIBS += -pthread -lm
endif
ifeq ($(UNAME_S),Darwin)
	LIBS += -pthread
endif

.PHONY: all clean directories weights

all: directories weights $(TARGET) 

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s | directories
	$(CC) $(CFLAGS) -c -o $@ $<

directories:
	@mkdir -p $(OBJ_DIRS)

# If we don't have any weights, make incbin include an empty file to avoid errors.
# NNUE eval will be disabled in the binary
weights:
ifeq ($(wildcard .\import\weights.nnue),)
	touch .\import\weights.nnue
endif

clean:
	rm -rf $(TGT_DIR)

-include $(DEPS)