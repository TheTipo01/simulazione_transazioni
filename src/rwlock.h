#ifndef RWLOCK_H
#define RWLOCK_H

/*
 * Semaforo per la lettura. Ha priorità rispetto al semaforo di scrittura: quando il node ha bisogno di scrivere
 * sul libro mastro, deve aspettare che gli altri processi finiscano di eseguire la lettura, in modo da evitare
 * inconsistenze sui dati letti.
 */
void read_start(int sem_id, unsigned int *read_counter, int read, int write);

void read_end(int sem_id, unsigned int *read_counter, int read, int write);

int get_stop_value();

struct ProcessoNode get_node(unsigned int i);

#endif
