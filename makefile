CC = gcc
CFLAGS = -std=c89 -pedantic

all:
	$(CC) src/master.c -o master $(CFLAGS)

clean:
	rm master

run:
	./master
