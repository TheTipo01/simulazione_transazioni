CC = gcc
CFLAGS = -std=c89 -pedantic -O2

all:
	$(CC) src/master.c -o master $(CFLAGS)

clean:
	rm master

run:
	./master
