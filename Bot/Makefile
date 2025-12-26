CC = gcc
CFLAGS = -Wall -g -std=c99
OBJFILES = src/*.c
TARGET = ChessBot

all: clean $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES)

clean:
	rm -f $(TARGET) *~ leaks.txt