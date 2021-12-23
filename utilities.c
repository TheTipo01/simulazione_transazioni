#ifndef UTILITIES_C
#define UTILITIES_C

#include "lib/libsem.c"

void read_start(int sem_id, unsigned int *readCounter) {
    sem_reserve(sem_id, LEDGER_READ);
    readCounter++;

    if (*readCounter == 1) {
        sem_reserve(sem_id, LEDGER_WRITE);
    }
    sem_release(sem_id, LEDGER_READ);
}

void read_end(int sem_id, unsigned int *readCounter) {
    sem_reserve(sem_id, LEDGER_READ);
    readCounter--;

    if (*readCounter == 0) {
        sem_release(sem_id, LEDGER_WRITE);
    }
    sem_release(sem_id, LEDGER_READ);
}

#endif