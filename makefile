CC = gcc
CFLAGS = -std=c89 -pedantic -O2

all:
	$(CC) -o master src/config.c src/rwlock.c src/shared_memory.c src/utilities.c vendor/libsem.c src/node.c src/user.c src/master.c $(CFLAGS)

clean:
	rm master

run:
	./master
