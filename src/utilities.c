#ifndef UTILITIES_C
#define UTILITIES_C

#include <sys/shm.h>
#include <errno.h>
#include <time.h>
#include "utilities.h"
#include "master.h"
#include "enum.h"

#ifdef _WIN32
#include <conio.h>
#else

#include <stdio.h>

#define clrscr() printf("\e[1;1H\e[2J")
#endif

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

    clrscr();

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
    fprintf(stdout, "Numero di processi nodo attivi: %d\n", activeNodes);
    fprintf(stdout, "Processo nodo con bilancio più alto: #%d, status = %s, balance = %.2f\n", maxPid, maxStat, maxBal);
    fprintf(stdout, "Processo nodo con bilancio più basso: #%d, status = %s, balance = %.2f\n\n", minPid, minStat,
            minBal);
}

void printMoreStatus(struct ProcessoNode *nodePIDs, struct ProcessoUser *usersPIDs) {
    int i, activeNodes = 0, activeUsers = 0;
    struct ProcessoNode Top3Nodes[3], Bottom3Nodes[3], tempNode;
    struct ProcessoUser Top3Users[3], Bottom3Users[3], tempUser;

    for (i = 0; i < 3; i++) Bottom3Nodes[i].balance = 1001;
    for (i = 0; i < 3; i++) Bottom3Users[i].balance = 1001;

    setvbuf(stdout, NULL, _IONBF, 0);

    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (usersPIDs[i].status != PROCESS_FINISHED && usersPIDs[i].status != PROCESS_FINISHED_PREMATURELY) {
            activeUsers++;
        }
    }

    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        if (nodePIDs[i].status != PROCESS_FINISHED) {
            activeNodes++;
        }
    }

    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (usersPIDs[i].balance > Top3Users[0].balance) {
            tempUser = Top3Users[1];
            Top3Users[1] = Top3Users[0];
            Top3Users[2] = tempUser;
            Top3Users[0] = usersPIDs[i];
        } else if (usersPIDs[i].balance > Top3Users[1].balance) {
            tempUser = Top3Users[1];
            Top3Users[1] = usersPIDs[i];
            Top3Users[2] = tempUser;
        } else if (usersPIDs[i].balance > Top3Users[2].balance) {
            Top3Users[2] = usersPIDs[i];
        }

        if (usersPIDs[i].balance < Bottom3Users[0].balance) {
            tempUser = Bottom3Users[1];
            Bottom3Users[1] = Bottom3Users[0];
            Bottom3Users[2] = tempUser;
            Bottom3Users[0] = usersPIDs[i];
        } else if (usersPIDs[i].balance < Bottom3Users[1].balance) {
            tempUser = Bottom3Users[1];
            Bottom3Users[1] = usersPIDs[i];
            Bottom3Users[2] = tempUser;
        } else if (usersPIDs[i].balance < Bottom3Users[2].balance) {
            Bottom3Users[2] = usersPIDs[i];
        }
    }

    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        if (nodePIDs[i].balance > Top3Nodes[0].balance) {
            tempNode = Top3Nodes[1];
            Top3Nodes[1] = Top3Nodes[0];
            Top3Nodes[2] = tempNode;
            Top3Nodes[0] = nodePIDs[i];
        } else if (nodePIDs[i].balance > Top3Nodes[1].balance) {
            tempNode = Top3Nodes[1];
            Top3Nodes[1] = nodePIDs[i];
            Top3Nodes[2] = tempNode;
        } else if (nodePIDs[i].balance > Top3Nodes[2].balance) {
            Top3Nodes[2] = nodePIDs[i];
        }

        if (nodePIDs[i].balance < Bottom3Nodes[0].balance) {
            tempNode = Bottom3Nodes[1];
            Bottom3Nodes[1] = Bottom3Nodes[0];
            Bottom3Nodes[2] = tempNode;
            Bottom3Nodes[0] = nodePIDs[i];
        } else if (nodePIDs[i].balance < Bottom3Nodes[1].balance) {
            tempNode = Bottom3Nodes[1];
            Bottom3Nodes[1] = nodePIDs[i];
            Bottom3Nodes[2] = tempNode;
        } else if (nodePIDs[i].balance < Bottom3Nodes[2].balance) {
            Bottom3Nodes[2] = nodePIDs[i];
        }
    }

    fprintf(stdout, "------------------------------------------------------------\n");
    fprintf(stdout, "Active Users: %d                          Active Nodes: %d\n", activeUsers, activeNodes);
    fprintf(stdout, "\nTop/Bottom 3 Users:                      Top/Bottom 3 Nodes:\n");
    fprintf(stdout, "#%d, %.2fÐ, %d                      #%d, %.2fÐ, %d\n",
            Top3Users[0].pid, Top3Users[0].balance, Top3Users[0].status,
            Top3Nodes[0].pid, Top3Nodes[0].balance, Top3Nodes[0].status);
    fprintf(stdout, "#%d, %.2fÐ, %d                      #%d, %.2fÐ, %d\n",
            Top3Users[1].pid, Top3Users[1].balance, Top3Users[1].status,
            Top3Nodes[1].pid, Top3Nodes[1].balance, Top3Nodes[1].status);
    fprintf(stdout, "#%d, %.2fÐ, %d                      #%d, %.2fÐ, %d\n",
            Top3Users[2].pid, Top3Users[2].balance, Top3Users[2].status,
            Top3Nodes[2].pid, Top3Nodes[2].balance, Top3Nodes[2].status);
    fprintf(stdout, "...                                      ...\n");
    fprintf(stdout, "#%d, %.2fÐ, %d                         #%d, %.2fÐ, %d\n",
            Bottom3Users[2].pid, Bottom3Users[2].balance, Bottom3Users[2].status,
            Bottom3Nodes[2].pid, Bottom3Nodes[2].balance, Bottom3Nodes[2].status);
    fprintf(stdout, "#%d, %.2fÐ, %d                         #%d, %.2fÐ, %d\n",
            Bottom3Users[1].pid, Bottom3Users[1].balance, Bottom3Users[1].status,
            Bottom3Nodes[1].pid, Bottom3Nodes[1].balance, Bottom3Nodes[1].status);
    fprintf(stdout, "#%d, %.2fÐ, %d                         #%d, %.2fÐ, %d\n",
            Bottom3Users[0].pid, Bottom3Users[0].balance, Bottom3Users[0].status,
            Bottom3Nodes[0].pid, Bottom3Nodes[0].balance, Bottom3Nodes[0].status);
    fprintf(stdout, "------------------------------------------------------------\n\n");

    clrscr();
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
