#ifndef UTILITIES_C
#define UTILITIES_C

#include "utilities.h"
#include "master.h"
#include "enum.h"

#include <stdio.h>
#include <sys/shm.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

char *format_time(time_t rawtime) {
    struct tm *timeinfo;
    timeinfo = localtime(&rawtime);
    return asctime(timeinfo);
}

char *get_status(int status) {
    switch (status) {
        case 0:
            return "running";
        case 1:
            return "waiting";
        case 2:
            return "term.";
        case 3:
            return "p-term.";
        default:
            return "";
    }
}

void printStatus() {
    int i, activeNodes = 0, activeUsers = 0;
    pid_t maxPid, minPid;
    double maxBal, minBal;
    char *maxStat, *minStat;

    setvbuf(stdout, NULL, _IONBF, 0);

    clrscr();

    maxBal = 0;
    minBal = cfg.SO_BUDGET_INIT;
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (sh.users_pid[i].status != PROCESS_FINISHED) {
            activeUsers++;
            if (sh.users_pid[i].balance > maxBal) {
                maxBal = sh.users_pid[i].balance;
                maxPid = sh.users_pid[i].pid;
                maxStat = get_status(sh.users_pid[i].status);
            }
            if (sh.users_pid[i].balance < minBal) {
                minBal = sh.users_pid[i].balance;
                minPid = sh.users_pid[i].pid;
                minStat = get_status(sh.users_pid[i].status);
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
        if (sh.nodes_pid[i].status != PROCESS_FINISHED) {
            activeNodes++;
            if (sh.nodes_pid[i].balance > maxBal) {
                maxBal = sh.nodes_pid[i].balance;
                maxPid = sh.nodes_pid[i].pid;
                maxStat = get_status(sh.nodes_pid[i].status);
            }
            if (sh.nodes_pid[i].balance < minBal) {
                minBal = sh.nodes_pid[i].balance;
                minPid = sh.nodes_pid[i].pid;
                minStat = get_status(sh.nodes_pid[i].status);
            }
        }
    }
    fprintf(stdout, "Numero di processi nodo attivi: %d\n", activeNodes);
    fprintf(stdout, "Processo nodo con bilancio più alto: #%d, status = %s, balance = %.2f\n", maxPid, maxStat, maxBal);
    fprintf(stdout, "Processo nodo con bilancio più basso: #%d, status = %s, balance = %.2f\n\n", minPid, minStat,
            minBal);
}

void print_more_status() {
    int i, activeNodes = 0, activeUsers = 0;
    struct ProcessoNode Top3Nodes[3], Bottom3Nodes[3], tempNode;
    struct ProcessoUser Top3Users[3], Bottom3Users[3], tempUser;

    clrscr();

    setvbuf(stdout, NULL, _IONBF, 0);

    for (i = 0; i < 3; i++) {
        Top3Users[i].balance = 0;
        Top3Users[i].status = 0;
        Top3Users[i].pid = 0;

        Top3Nodes[i].balance = 0;
        Top3Nodes[i].status = 0;
        Top3Nodes[i].pid = 0;

        Bottom3Users[i].balance = cfg.SO_BUDGET_INIT + 1;
        Bottom3Users[i].status = 0;
        Bottom3Users[i].pid = 0;

        Bottom3Nodes[i].balance = cfg.SO_BUDGET_INIT + 1;
        Bottom3Nodes[i].status = 0;
        Bottom3Nodes[i].pid = 0;
    }

    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (sh.users_pid[i].status != PROCESS_FINISHED && sh.users_pid[i].status != PROCESS_FINISHED_PREMATURELY) {
            activeUsers++;
        }
    }

    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        if (sh.nodes_pid[i].status != PROCESS_FINISHED) {
            activeNodes++;
        }
    }

    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (sh.users_pid[i].balance > Top3Users[0].balance) {
            tempUser = Top3Users[1];
            Top3Users[1] = Top3Users[0];
            Top3Users[2] = tempUser;
            Top3Users[0] = sh.users_pid[i];
        } else if (sh.users_pid[i].balance > Top3Users[1].balance) {
            tempUser = Top3Users[1];
            Top3Users[1] = sh.users_pid[i];
            Top3Users[2] = tempUser;
        } else if (sh.users_pid[i].balance > Top3Users[2].balance) {
            Top3Users[2] = sh.users_pid[i];
        }

        if (sh.users_pid[i].balance < Bottom3Users[0].balance) {
            tempUser = Bottom3Users[1];
            Bottom3Users[1] = Bottom3Users[0];
            Bottom3Users[2] = tempUser;
            Bottom3Users[0] = sh.users_pid[i];
        } else if (sh.users_pid[i].balance < Bottom3Users[1].balance) {
            tempUser = Bottom3Users[1];
            Bottom3Users[1] = sh.users_pid[i];
            Bottom3Users[2] = tempUser;
        } else if (sh.users_pid[i].balance < Bottom3Users[2].balance) {
            Bottom3Users[2] = sh.users_pid[i];
        }
    }

    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        if (sh.nodes_pid[i].balance > Top3Nodes[0].balance) {
            tempNode = Top3Nodes[1];
            Top3Nodes[1] = Top3Nodes[0];
            Top3Nodes[2] = tempNode;
            Top3Nodes[0] = sh.nodes_pid[i];
        } else if (sh.nodes_pid[i].balance > Top3Nodes[1].balance) {
            tempNode = Top3Nodes[1];
            Top3Nodes[1] = sh.nodes_pid[i];
            Top3Nodes[2] = tempNode;
        } else if (sh.nodes_pid[i].balance > Top3Nodes[2].balance) {
            Top3Nodes[2] = sh.nodes_pid[i];
        }

        if (sh.nodes_pid[i].balance < Bottom3Nodes[0].balance) {
            tempNode = Bottom3Nodes[1];
            Bottom3Nodes[1] = Bottom3Nodes[0];
            Bottom3Nodes[2] = tempNode;
            Bottom3Nodes[0] = sh.nodes_pid[i];
        } else if (sh.nodes_pid[i].balance < Bottom3Nodes[1].balance) {
            tempNode = Bottom3Nodes[1];
            Bottom3Nodes[1] = sh.nodes_pid[i];
            Bottom3Nodes[2] = tempNode;
        } else if (sh.nodes_pid[i].balance < Bottom3Nodes[2].balance) {
            Bottom3Nodes[2] = sh.nodes_pid[i];
        }
    }

#define format "#%-5d %-6.2f %s       #%-5d %-6.2f %s\n"

    fprintf(stdout, "------------------------------------------------------------\n");
    fprintf(stdout, "Active Users: %d                Active Nodes: %d\n", activeUsers, activeNodes);
    fprintf(stdout, "\nTop/Bottom 3 Users:           Top/Bottom 3 Nodes:\n");
    fprintf(stdout, format,
            Top3Users[0].pid, Top3Users[0].balance, get_status(Top3Users[0].status),
            Top3Nodes[0].pid, Top3Nodes[0].balance, get_status(Top3Nodes[0].status));
    fprintf(stdout, format,
            Top3Users[1].pid, Top3Users[1].balance, get_status(Top3Users[1].status),
            Top3Nodes[1].pid, Top3Nodes[1].balance, get_status(Top3Nodes[1].status));
    fprintf(stdout, format,
            Top3Users[2].pid, Top3Users[2].balance, get_status(Top3Users[2].status),
            Top3Nodes[2].pid, Top3Nodes[2].balance, get_status(Top3Nodes[2].status));
    fprintf(stdout, "...                                 ...\n");
    fprintf(stdout, format,
            Bottom3Users[2].pid, Bottom3Users[2].balance, get_status(Bottom3Users[2].status),
            Bottom3Nodes[2].pid, Bottom3Nodes[2].balance, get_status(Bottom3Nodes[2].status));
    fprintf(stdout, format,
            Bottom3Users[1].pid, Bottom3Users[1].balance, get_status(Bottom3Users[1].status),
            Bottom3Nodes[1].pid, Bottom3Nodes[1].balance, get_status(Bottom3Nodes[1].status));
    fprintf(stdout, format,
            Bottom3Users[0].pid, Bottom3Users[0].balance, get_status(Bottom3Users[0].status),
            Bottom3Nodes[0].pid, Bottom3Nodes[0].balance, get_status(Bottom3Nodes[0].status));
    fprintf(stdout, "------------------------------------------------------------\n\n");
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
