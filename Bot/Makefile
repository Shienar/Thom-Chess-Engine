CC = gcc
CFLAGS = -Wall -g -std=c99
OBJFILES = main.c moves.c bitboard.c engine.c hashtable.c debug.c
TARGET = ChessBot

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES)

clean:
	rm -f $(TARGET) *~