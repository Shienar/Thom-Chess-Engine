# Compiler and flags
CC = gcc
CFLAGS = -Wall -std=c99 -mavx2 -fopenmp -MMD -MP

# Add debug or optimization flags to compiler flags
ifdef DEBUG
	CFLAGS += -g
else

ifdef VERIFY
$(error VERIFY cannot be compiled without DEBUG)	
endif

	CFLAGS += -O3
	CFLAGS += -DNDEBUG
endif

# NNUE validation.
ifdef VERIFY
	CFLAGS += -DVERIFY
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
DEPS     := $(OBJFILES:.o=.d)
OBJFILES += $(ASMOBJFILES)

OBJ_DIRS := $(sort $(dir $(OBJFILES)))

# Linux / MacOS specific libaries
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LIBS += -pthread -lm
endif
ifeq ($(UNAME_S),Darwin)
	LIBS += -pthread
endif

.PHONY: all clean directories

all: directories $(TARGET) 

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s import/weights.bin import/komodo.bin | directories
	$(CC) $(CFLAGS) -c -o $@ $<

directories:
	@mkdir -p $(OBJ_DIRS)

clean:
	rm -f $(OBJFILES) $(DEPS) $(TARGET)

-include $(DEPS)