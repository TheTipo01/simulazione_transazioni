#ifndef UTILITIES_C
#define UTILITIES_C

#include "utilities.h"
#include "master.h"
#include "enum.h"
#include "shared_memory.h"
#include "rwlock.h"

#include <stdio.h>
#include <sys/shm.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <signal.h>

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
            kill(status, 0);
            if (errno) return "term.";
            else return "running";
    }
}

int get_node_balance(int pos) {
    int i, temp_bal = 0;

    /* Dobbiamo eseguire una lettura del libro mastro, quindi impostiamo il semaforo di lettura */
    read_start(ids.sem, sh.ledger_read, LEDGER_READ, LEDGER_WRITE);

    /* Cicliamo all'interno del libro mastro per controllare l'ultima transazione di ogni blocco */
    for (i = 0; i < SO_REGISTRY_SIZE; i++) {
        if (sh.ledger[i].transazioni[SO_BLOCK_SIZE - 1].receiver == get_node(pos).pid) {

            /* Aggiungiamo al bilancio solo le quantity dei blocchi il cui receiver è il pid del nodo in questione */
            temp_bal += (int) sh.ledger[i].transazioni[SO_BLOCK_SIZE - 1].quantity;
        }
    }

    /* Rilasciamo il semaforo di lettura a fine operazione */
    read_end(ids.sem, sh.ledger_read, LEDGER_READ, LEDGER_WRITE);

    return temp_bal;
}

void print_more_status() {
    int i, active_nodes = 0, active_users = 0, tempbal = 0;
    struct PrintNode t3_nodes[3], b3_nodes[3], temp_node;
    struct ProcessoUser t3_users[3], b3_users[3], temp_user;

    clrscr();

    setvbuf(stdout, NULL, _IONBF, 0);

    for (i = 0; i < 3; i++) {
        t3_users[i].balance = 0;
        t3_users[i].status = 0;
        t3_users[i].pid = 0;

        t3_nodes[i].balance = 0;
        t3_nodes[i].pid = 0;

        b3_users[i].balance = cfg.SO_BUDGET_INIT + 1;
        b3_users[i].status = 0;
        b3_users[i].pid = 0;

        b3_nodes[i].balance = 2147483647;
        b3_nodes[i].pid = 0;
    }

    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (sh.users_pid[i].status != PROCESS_FINISHED && sh.users_pid[i].status != PROCESS_FINISHED_PREMATURELY) {
            active_users++;
        }
    }

    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        kill(get_node(i).pid, 0);
        if (errno == 0) {
            active_nodes++;
        }
    }

    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (sh.users_pid[i].balance > t3_users[0].balance) {
            temp_user = t3_users[1];
            t3_users[1] = t3_users[0];
            t3_users[2] = temp_user;
            t3_users[0] = sh.users_pid[i];
        } else if (sh.users_pid[i].balance > t3_users[1].balance) {
            temp_user = t3_users[1];
            t3_users[1] = sh.users_pid[i];
            t3_users[2] = temp_user;
        } else if (sh.users_pid[i].balance > t3_users[2].balance) {
            t3_users[2] = sh.users_pid[i];
        }

        if (sh.users_pid[i].balance < b3_users[0].balance) {
            temp_user = b3_users[1];
            b3_users[1] = b3_users[0];
            b3_users[2] = temp_user;
            b3_users[0] = sh.users_pid[i];
        } else if (sh.users_pid[i].balance < b3_users[1].balance) {
            temp_user = b3_users[1];
            b3_users[1] = sh.users_pid[i];
            b3_users[2] = temp_user;
        } else if (sh.users_pid[i].balance < b3_users[2].balance) {
            b3_users[2] = sh.users_pid[i];
        }
    }

    check_for_update();
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        tempbal = get_node_balance(i);
        if (tempbal > t3_nodes[0].balance) {
            temp_node = t3_nodes[1];
            t3_nodes[1] = t3_nodes[0];
            t3_nodes[2] = temp_node;
            t3_nodes[0].pid = nodes_pid[i].pid;
            t3_nodes[0].balance = tempbal;
        } else if (tempbal > t3_nodes[1].balance) {
            temp_node = t3_nodes[1];
            t3_nodes[1].pid = nodes_pid[i].pid;
            t3_nodes[1].balance = tempbal;
            t3_nodes[2] = temp_node;
        } else if (tempbal > t3_nodes[2].balance) {
            t3_nodes[2].pid = nodes_pid[i].pid;
        }

        if (tempbal < b3_nodes[0].balance) {
            temp_node = b3_nodes[1];
            b3_nodes[1] = b3_nodes[0];
            b3_nodes[2] = temp_node;
            b3_nodes[0].pid = nodes_pid[i].pid;
            b3_nodes[0].balance = tempbal;
        } else if (tempbal < b3_nodes[1].balance) {
            temp_node = b3_nodes[1];
            b3_nodes[1].pid = nodes_pid[i].pid;
            b3_nodes[1].balance = tempbal;
            b3_nodes[2] = temp_node;
        } else if (tempbal < b3_nodes[2].balance) {
            b3_nodes[2].pid = nodes_pid[i].pid;
            b3_nodes[2].balance = tempbal;
        }
    }

#define format "#%-5d %-6d %s       #%-5d %-6d %s\n"

    fprintf(stdout, "------------------------------------------------------------\n");
    fprintf(stdout, "Active Users: %d                Active Nodes: %d\n", active_users, active_nodes);
    fprintf(stdout, "\nTop/Bottom 3 Users:           Top/Bottom 3 Nodes:\n");
    fprintf(stdout, format,
            t3_users[0].pid, t3_users[0].balance, get_status(t3_users[0].status),
            t3_nodes[0].pid, t3_nodes[0].balance, get_status(t3_nodes[0].pid));
    fprintf(stdout, format,
            t3_users[1].pid, t3_users[1].balance, get_status(t3_users[1].status),
            t3_nodes[1].pid, t3_nodes[1].balance, get_status(t3_nodes[1].pid));
    fprintf(stdout, format,
            t3_users[2].pid, t3_users[2].balance, get_status(t3_users[2].status),
            t3_nodes[2].pid, t3_nodes[2].balance, get_status(t3_nodes[2].pid));
    fprintf(stdout, "...                                 ...\n");
    fprintf(stdout, format,
            b3_users[2].pid, b3_users[2].balance, get_status(b3_users[2].status),
            b3_nodes[2].pid, b3_nodes[2].balance, get_status(b3_nodes[2].pid));
    fprintf(stdout, format,
            b3_users[1].pid, b3_users[1].balance, get_status(b3_users[1].status),
            b3_nodes[1].pid, b3_nodes[1].balance, get_status(b3_nodes[1].pid));
    fprintf(stdout, format,
            b3_users[0].pid, b3_users[0].balance, get_status(b3_users[0].status),
            b3_nodes[0].pid, b3_nodes[0].balance, get_status(b3_nodes[0].pid));
    fprintf(stdout, "------------------------------------------------------------\n\n");
}


void shmget_error_checking(int id) {
    if (id == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

int sleeping(long waiting_time) {
    struct timespec requested_time;
    requested_time.tv_sec = waiting_time / 1000000000;
    requested_time.tv_nsec = waiting_time % 1000000000;
    return nanosleep(&requested_time, NULL);
}

long random_between_two(long min, long max) {
    return random() % (max - min + 1) + min;
}

void expand_node() {
    cfg.SO_NODES_NUM += 1;

    /* Allarghiamo l'array dei nodi */
    nodes_pid = realloc(nodes_pid, sizeof(int) * cfg.SO_NODES_NUM);
}

void check_for_update() {
    struct Messaggio_node msg;

    while (msgrcv(ids.msg_new_node, &msg, sizeof(struct Messaggio_node) - sizeof(long), getpid(), IPC_NOWAIT) != -1) {
        expand_node();

        /* E aggiungiamo il nodo */
        nodes_pid[cfg.SO_NODES_NUM - 1] = msg.nodo;
    }

    errno = 0;
}

#endif
