#ifndef UTILITIES_C
#define UTILITIES_C

#include <sys/shm.h>
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

/* Stampa lo stato attuale dei processi utente e nodo */
void printStatus(Processo *nodePIDs, Processo *usersPIDs, Config *cfg) {
    int i, activeNodes, activeUsers, maxBudget = 0, minBudget = 0;

    setvbuf(stdout, NULL, _IONBF, 0);

    for (i = 0; i < cfg->SO_USERS_NUM; i++) {
        if (usersPIDs[i].status != PROCESS_FINISHED) activeUsers++;
    }
    fprintf(stdout, "\nNumero di processi utente attivi: %d\n", activeUsers);

    for (i = 0; i < cfg->SO_NODES_NUM; i++) {
        if (nodePIDs[i].status != PROCESS_FINISHED) activeNodes++;
    }
    fprintf(stdout, "\nNumero di processi nodo attivi: %d\n", activeNodes);
}


void shmdt_error_checking(const void *adrr) {
    if (shmdt(adrr) == -1) {
        fprintf(stderr, "%s: %d. Errore in shmdt #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

#endif