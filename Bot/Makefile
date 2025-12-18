CC = gcc
CFLAGS = -Wall -g -std=c99
OBJFILES = main.c moves.c bitboard.c engine.c
TARGET = TomBot

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES)

clean:
	rm -f $(TARGET) *~