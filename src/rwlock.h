#ifndef RWLOCK_H
#define RWLOCK_H

/* Funzione per iniziare una lettura */
void read_start(int sem_id, unsigned int *read_counter, int read, int write);

/* Funzione per terminare una lettura */
void read_end(int sem_id, unsigned int *read_counter, int read, int write);

/* Funzione wrapper per bloccare dietro a un semaforo l'accesso alla variabile sh.stop */
int get_stop_value();

/* Funzione wrapper per bloccare dietro a un semaforo la lettura sull'array sh.nodes_pid */
struct ProcessoNode get_node(unsigned int i);

#endif
