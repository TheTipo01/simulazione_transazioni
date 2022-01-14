#ifndef RWLOCK_H
#define RWLOCK_H

#include "rwlock.c"

/*
 * Semaforo per la lettura. Ha priorità rispetto al semaforo di scrittura: quando il node ha bisogno di scrivere
 * sul libro mastro, deve aspettare che gli altri processi finiscano di eseguire la lettura, in modo da evitare
 * inconsistenze sui dati letti.
 */
void read_start(int sem_id, unsigned int *readCounter, int read, int write);

void read_end(int sem_id, unsigned int *readCounter, int read, int write);

int get_stop_value(int *stop, unsigned int *readCounter);

#endif
