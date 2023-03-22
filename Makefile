.PHONY: all clean

CC = gcc
DEFS = -DDEBUG
CFLAGS = -std=c99 -pedantic -Wall
LIBS = -lz	#zlib required for crc32

all: sender receiver

sender: sender.c sender.h functions.o common.h
	$(CC) $(CFLAGS) -g -o $@ $< functions.o $(DEFS) $(LIBS)

receiver: receiver.c receiver.h functions.o common.h
	$(CC) $(CFLAGS) -g -o $@ $< functions.o $(DEFS) $(LIBS)

functions.o: functions.c functions.h common.h
	$(CC) $(CFLAGS) -g -c -o $@ $< $(DEFS) $(LIBS)

clean:
	rm -rf sender receiver *.o
