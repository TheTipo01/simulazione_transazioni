CC = gcc
CFLAGS = -std=c89 -pedantic


all: master.c
	gcc master.c -o master $(CFLAGS)

clean:
	rm -rf *.o

run:
	./master
