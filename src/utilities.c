#ifndef UTILITIES_C
#define UTILITIES_C

#include <sys/shm.h>
#include "../vendor/libsem.c"

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
    int i, activeNodes, activeUsers, maxPid, minPid;
    unsigned int maxBal, minBal;
    char *maxStat, *minStat;

    setvbuf(stdout, NULL, _IONBF, 0);

    maxBal = 0;
    minBal = cfg->SO_BUDGET_INIT;
    for (i = 0; i < cfg->SO_USERS_NUM; i++) {
        if (usersPIDs[i].status != PROCESS_FINISHED) {
            activeUsers++;
            if (usersPIDs[i].balance > maxBal) {
                maxBal = usersPIDs[i].balance;
                maxPid = usersPIDs[i].pid;
                switch (usersPIDs[i].status) {
                    case PROCESS_WAITING:
                        maxStat = "waiting";
                        break;
                    case PROCESS_RUNNING:
                        maxStat = "running";
                        break;
                }
            };
            if (usersPIDs[i].balance < minBal) {
                minBal = usersPIDs[i].balance;
                minPid = usersPIDs[i].pid;
                switch (usersPIDs[i].status) {
                    case PROCESS_WAITING:
                        minStat = "waiting";
                        break;
                    case PROCESS_RUNNING:
                        minStat = "running";
                        break;

                }
            };
        };
    }
    fprintf(stdout, "\nNumero di processi utente attivi: %d\n", activeUsers);
    fprintf(stdout, "Processo utente con bilancio più alto: #%d, status = %s, balance = %d\n", maxPid, maxStat, maxBal);
    fprintf(stdout, "Processo utente con bilancio più basso: #%d, status = %s, balance = %d\n", minPid, minStat,
            minBal);

    maxBal = 0;
    minBal = cfg->SO_BUDGET_INIT;
    for (i = 0; i < cfg->SO_NODES_NUM; i++) {
        if (nodePIDs[i].status != PROCESS_FINISHED) {
            activeNodes++;
            if (nodePIDs[i].balance > maxBal) {
                maxBal = nodePIDs[i].balance;
                maxPid = nodePIDs[i].pid;
                switch (nodePIDs[i].status) {
                    case PROCESS_WAITING:
                        maxStat = "waiting";
                        break;
                    case PROCESS_RUNNING:
                        maxStat = "running";
                        break;
                }
            };
            if (nodePIDs[i].balance < minBal) {
                minBal = nodePIDs[i].balance;
                minPid = nodePIDs[i].pid;
                switch (nodePIDs[i].status) {
                    case PROCESS_WAITING:
                        minStat = "waiting";
                        break;
                    case PROCESS_RUNNING:
                        minStat = "running";
                        break;
                }
            };
        };
    }
    fprintf(stdout, "\nNumero di processi nodo attivi: %d\n", activeNodes);
    fprintf(stdout, "Processo nodo con bilancio più alto: #%d, status = %s, balance = %d\n", maxPid, maxStat, maxBal);
    fprintf(stdout, "Processo nodo con bilancio più basso: #%d, status = %s, balance = %d\n", minPid, minStat, minBal);
}


void shmdt_error_checking(const void *adrr) {
    if (shmdt(adrr) == -1) {
        fprintf(stderr, "%s: %d. Errore in shmdt #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

#endif
