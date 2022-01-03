CC = gcc
CFLAGS = -std=c89 -pedantic


all: src/master.c
	$(CC) master.c -o master $(CFLAGS)

clean:
	rm -rf *.o

run:
	./master
