CC = gcc
CFLAGS = -Wall -g -std=c99 -march=native
OBJFILES = src/*.c
TARGET = ChessBot

#Alternative CFLAGS for gprof analysis.
# gprof ./ChessBot.exe -b > output.txt
# CFLAGS = -Wall -g -std=c99 -march=native -pg -no-pie


all: clean $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) $(SIMFLAGS) -o $(TARGET) $(OBJFILES)

clean:
	rm -f $(TARGET) *~ leaks.txt