# Compiler and flags
CC = gcc
CFLAGS = -Wall -std=c99 -march=native

#release is incompatible with trainer & debug, check here.
ifneq ($(origin RELEASE),undefined)
  ifneq ($(origin TRAIN),undefined)
    $(error Trainer is incompatible with release's read-only weights)
  endif
  ifneq ($(origin DEBUG),undefined)
    $(error Release version and debug version incompatible)
  endif
endif

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
		   $(wildcard src/pyrrhic/tbprobe.c)
ASMFILES = 
ifdef RELEASE
	CFLAGS += -DRELEASE
	ASMFILES += src/incbin.S
else
# 	define PROJECT_CWD as a string literal evaluating to the current path for file opening.
	CFLAGS += -DPROJECT_CWD="\"$(CURDIR)\""
endif

ifdef TRAIN
	ifdef HIP_PATH
	HIP_ROOT = $(subst \,/,$(HIP_PATH))
	CFLAGS += -I"$(HIP_ROOT)/include"
	LIBS = -L"$(HIP_ROOT)/lib" -lamdhip64

# 		Use "make NVIDIA=1" to compile on NVIDIA gpus. (untested)
		ifdef NVIDIA
			CFLAGS +=-D__HIP_PLATFORM_NVIDIA__
			KC = nvcc
			KFLAGS = -O3 --ptx -D__HIP_PLATFORM_NVIDIA__
			KTARGET = $(OBJ_DIR)/train/kernels.nvptx
		else
			CFLAGS +=-D__HIP_PLATFORM_AMD__
			KC = hipcc
			KFLAGS = -O3 --genco --offload-arch=native -D__HIP_PLATFORM_AMD__
			KTARGET = $(OBJ_DIR)/train/kernels.hsaco
		endif
	endif

	SRCFILES += $(wildcard src/train/*.c)
 	SRCFILES += $(wildcard src/binpack/*c)
	CFLAGS += -DTRAIN

	ifdef KPERFT
		CFLAGS += -DPERFT_KERNELS
	endif
endif

OBJFILES := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCFILES))
ASMOBJFILES := $(patsubst $(SRC_DIR)/%.S, $(OBJ_DIR)/%.o, $(ASMFILES))
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

.PHONY: all clean directories

ifdef TRAIN
all: directories $(TARGET) $(KTARGET)
else
all: directories $(TARGET)
endif

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S | directories
	$(CC) $(CFLAGS) -c -o $@ $<

directories:
	@mkdir -p $(OBJ_DIRS)

$(KTARGET): src/train/kernels.hip
	$(KC) $(KFLAGS) src/train/kernels.hip -o $(KTARGET)

clean:
	rm -rf $(TGT_DIR)

-include $(DEPS)