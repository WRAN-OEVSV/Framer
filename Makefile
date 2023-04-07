.PHONY: all clean

CC = g++
DEFS = -DDEBUG
CFLAGS = -std=c++17 -pedantic -Wall -g -pthread
LIBS = -lz

all: framer

framer: framer.cpp framer.h common.h framer_transmit.o framer_receive.o functions.o
	$(CC) $(CFLAGS) -o $@ $< functions.o framer_transmit.o framer_receive.o $(DEFS) $(LIBS)

framer_transmit.o: framer_transmit.cpp framer_transmit.h common.h
	$(CC) $(CFLAGS) -c -o $@ $< $(DEFS) $(LIBS)

framer_receive.o: framer_receive.cpp framer_receive.h common.h
	$(CC) $(CFLAGS) -c -o $@ $< $(DEFS) $(LIBS)

functions.o: functions.cpp functions.h common.h
	$(CC) $(CFLAGS) -c -o $@ $< $(DEFS) $(LIBS)

clean:
	rm -rf framer *.o
