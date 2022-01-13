#ifndef UTILITIES_C
#define UTILITIES_C

#include <sys/shm.h>
#include <errno.h>
#include <time.h>
#include "utilities.h"
#include "master.h"
#include "enum.h"

#define msg_size() sizeof(struct Messaggio) - sizeof(long)

char *formatTime(time_t rawtime) {
    struct tm *timeinfo;
    timeinfo = localtime(&rawtime);
    return asctime(timeinfo);
}

void printStatus(struct ProcessoNode *nodePIDs, struct ProcessoUser *usersPIDs) {
    int i, activeNodes = 0, activeUsers = 0;
    pid_t maxPid, minPid;
    double maxBal, minBal;
    char *maxStat, *minStat;

    setvbuf(stdout, NULL, _IONBF, 0);

    fprintf(stdout, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    maxBal = 0;
    minBal = cfg.SO_BUDGET_INIT;
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
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
            }
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
            }
        }
    }
    fprintf(stdout, "\nNumero di processi utente attivi: %d\n", activeUsers);
    fprintf(stdout, "Processo utente con bilancio più alto: #%d, status = %s, balance = %.2f\n", maxPid, maxStat,
            maxBal);
    fprintf(stdout, "Processo utente con bilancio più basso: #%d, status = %s, balance = %.2f\n", minPid, minStat,
            minBal);

    maxBal = 0;
    minBal = cfg.SO_BUDGET_INIT;
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
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
            }
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
            }
        }
    }
    fprintf(stdout, "\nNumero di processi nodo attivi: %d\n", activeNodes);
    fprintf(stdout, "Processo nodo con bilancio più alto: #%d, status = %s, balance = %.2f\n", maxPid, maxStat, maxBal);
    fprintf(stdout, "Processo nodo con bilancio più basso: #%d, status = %s, balance = %.2f\n\n", minPid, minStat,
            minBal);

    /*
    fprintf(stdout, "Transazioni effettuate dai nodi:\n");
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        fprintf(stdout, "#%d   transactions = %d\n", nodePIDs[i].pid, nodePIDs[i].transactions);
    }
    */

    fprintf(stdout, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}

void shmget_error_checking(int id) {
    if (id == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int sleeping(long waitingTime) {
    struct timespec requested_time;
    requested_time.tv_sec = waitingTime / 1000000000;
    requested_time.tv_nsec = waitingTime % 1000000000;
    return nanosleep(&requested_time, NULL);
}

#endif
