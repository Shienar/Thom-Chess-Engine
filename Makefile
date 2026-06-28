CC = gcc
CFLAGS = -Wall -std=c99 -march=native -fopenmp -MMD -MP

ifdef DEBUG
	CFLAGS += -g
else
	CFLAGS += -O3
	CFLAGS += -DNDEBUG
endif

# Profiling
# gprof .\target\Thom.exe -P_mcount_private -P__fentry__ -b > output.txt
ifdef PROF
	CFLAGS += -pg -no-pie
endif

SRC_DIR = src
TGT_DIR = target
OBJ_DIR = $(TGT_DIR)/obj

CFLAGS += -I$(SRC_DIR)

# Use "make NEW=1" to create a copy. Used for testing elo differences between versions.
ifdef NEW
	TARGET = $(TGT_DIR)/Thom_new.exe
else
	TARGET = $(TGT_DIR)/Thom.exe
endif

# define PROJECT_CWD as a string literal evaluating to the current path. Used for fopen-ing.
CFLAGS += -DPROJECT_CWD="\"$(CURDIR)\""

SRCFILES = $(wildcard src/*.c) \
		   $(wildcard src/analyze/*.c) \
		   $(wildcard src/board/*.c) \
		   $(wildcard src/hashtables/*.c) \
		   $(wildcard src/pyrrhic/tbprobe.c)

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
	CFLAGS += -DTRAIN

	ifdef KPERFT
		CFLAGS += -DPERFT_KERNELS
	endif
endif

OBJFILES := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCFILES))
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

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c Makefile | directories
	$(CC) $(CFLAGS) -c -o $@ $<

directories:
	@mkdir -p $(OBJ_DIRS)

$(KTARGET): src/train/kernels.hip
	$(KC) $(KFLAGS) src/train/kernels.hip -o $(KTARGET)

clean:
	rm -rf $(TGT_DIR)

-include $(DEPS)