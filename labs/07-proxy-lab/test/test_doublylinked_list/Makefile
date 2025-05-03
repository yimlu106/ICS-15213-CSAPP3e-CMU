# Makefile for Doubly Linked List test

CC = gcc
CFLAGS = -g -Wall

all: test_main

doublylinkedlist.o: ../../doublylinkedlist.c ../../doublylinkedlist.h
	$(CC) $(CFLAGS) -c ../../doublylinkedlist.c

test_main.o: test_main.c ../../doublylinkedlist.h
	$(CC) $(CFLAGS) -c test_main.c

test_main: test_main.o doublylinkedlist.o
	$(CC) $(CFLAGS) test_main.o doublylinkedlist.o -o test_main

clean:
	rm -f *~ *.o test_main core *.tar *.zip *.gzip *.bzip *.gz
