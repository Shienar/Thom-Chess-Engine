CC = gcc
CFLAGS = -Wall -g -std=c99
OBJFILES = *.c
TARGET = ChessBot

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES)

clean:
	rm -f $(TARGET) *~