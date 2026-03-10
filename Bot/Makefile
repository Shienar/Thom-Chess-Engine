CC = gcc
CFLAGS = -Wall -g -std=c99 -march=native -mavx512vl -mavx512bw
OBJFILES = src/*.c
TARGET = ChessBot

all: clean $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) $(SIMFLAGS) -o $(TARGET) $(OBJFILES)

clean:
	rm -f $(TARGET) *~ leaks.txt