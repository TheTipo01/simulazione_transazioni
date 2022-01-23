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

void print_status() {
    int i, active_nodes = 0, active_users = 0;
    pid_t max_pid, min_pid;
    double max_bal, min_bal;
    char *max_stat, *min_stat;

    setvbuf(stdout, NULL, _IONBF, 0);

    clrscr();

    max_bal = 0;
    min_bal = cfg.SO_BUDGET_INIT;
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (sh.users_pid[i].status != PROCESS_FINISHED) {
            active_users++;
            if (sh.users_pid[i].balance > max_bal) {
                max_bal = sh.users_pid[i].balance;
                max_pid = sh.users_pid[i].pid;
                max_stat = get_status(sh.users_pid[i].status);
            }
            if (sh.users_pid[i].balance < min_bal) {
                min_bal = sh.users_pid[i].balance;
                min_pid = sh.users_pid[i].pid;
                min_stat = get_status(sh.users_pid[i].status);
            }
        }
    }
    fprintf(stdout, "\nNumero di processi utente attivi: %d\n", active_users);
    fprintf(stdout, "Processo utente con bilancio più alto: #%d, status = %s, balance = %.2f\n", max_pid, max_stat,
            max_bal);
    fprintf(stdout, "Processo utente con bilancio più basso: #%d, status = %s, balance = %.2f\n", min_pid, min_stat,
            min_bal);

    max_bal = 0;
    min_bal = cfg.SO_BUDGET_INIT;
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        if (sh.nodes_pid[i].status != PROCESS_FINISHED) {
            active_nodes++;
            if (sh.nodes_pid[i].balance > max_bal) {
                max_bal = sh.nodes_pid[i].balance;
                max_pid = sh.nodes_pid[i].pid;
                max_stat = get_status(sh.nodes_pid[i].status);
            }
            if (sh.nodes_pid[i].balance < min_bal) {
                min_bal = sh.nodes_pid[i].balance;
                min_pid = sh.nodes_pid[i].pid;
                min_stat = get_status(sh.nodes_pid[i].status);
            }
        }
    }
    fprintf(stdout, "Numero di processi nodo attivi: %d\n", active_nodes);
    fprintf(stdout, "Processo nodo con bilancio più alto: #%d, status = %s, balance = %.2f\n", max_pid, max_stat,
            max_bal);
    fprintf(stdout, "Processo nodo con bilancio più basso: #%d, status = %s, balance = %.2f\n\n", min_pid, min_stat,
            min_bal);
}

void print_more_status() {
    int i, active_nodes = 0, active_users = 0;
    struct ProcessoNode t3_nodes[3], b3_nodes[3], temp_node;
    struct ProcessoUser t3_users[3], b3_users[3], temp_user;

    clrscr();

    setvbuf(stdout, NULL, _IONBF, 0);

    for (i = 0; i < 3; i++) {
        t3_users[i].balance = 0;
        t3_users[i].status = 0;
        t3_users[i].pid = 0;

        t3_nodes[i].balance = 0;
        t3_nodes[i].status = 0;
        t3_nodes[i].pid = 0;

        b3_users[i].balance = cfg.SO_BUDGET_INIT + 1;
        b3_users[i].status = 0;
        b3_users[i].pid = 0;

        b3_nodes[i].balance = cfg.SO_BUDGET_INIT + 1;
        b3_nodes[i].status = 0;
        b3_nodes[i].pid = 0;
    }

    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (sh.users_pid[i].status != PROCESS_FINISHED && sh.users_pid[i].status != PROCESS_FINISHED_PREMATURELY) {
            active_users++;
        }
    }

    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        if (sh.nodes_pid[i].status != PROCESS_FINISHED) {
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

    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        if (sh.nodes_pid[i].balance > t3_nodes[0].balance) {
            temp_node = t3_nodes[1];
            t3_nodes[1] = t3_nodes[0];
            t3_nodes[2] = temp_node;
            t3_nodes[0] = sh.nodes_pid[i];
        } else if (sh.nodes_pid[i].balance > t3_nodes[1].balance) {
            temp_node = t3_nodes[1];
            t3_nodes[1] = sh.nodes_pid[i];
            t3_nodes[2] = temp_node;
        } else if (sh.nodes_pid[i].balance > t3_nodes[2].balance) {
            t3_nodes[2] = sh.nodes_pid[i];
        }

        if (sh.nodes_pid[i].balance < b3_nodes[0].balance) {
            temp_node = b3_nodes[1];
            b3_nodes[1] = b3_nodes[0];
            b3_nodes[2] = temp_node;
            b3_nodes[0] = sh.nodes_pid[i];
        } else if (sh.nodes_pid[i].balance < b3_nodes[1].balance) {
            temp_node = b3_nodes[1];
            b3_nodes[1] = sh.nodes_pid[i];
            b3_nodes[2] = temp_node;
        } else if (sh.nodes_pid[i].balance < b3_nodes[2].balance) {
            b3_nodes[2] = sh.nodes_pid[i];
        }
    }

#define format "#%-5d %-6.2f %s       #%-5d %-6.2f %s\n"

    fprintf(stdout, "------------------------------------------------------------\n");
    fprintf(stdout, "Active Users: %d                Active Nodes: %d\n", active_users, active_nodes);
    fprintf(stdout, "\nTop/Bottom 3 Users:           Top/Bottom 3 Nodes:\n");
    fprintf(stdout, format,
            t3_users[0].pid, t3_users[0].balance, get_status(t3_users[0].status),
            t3_nodes[0].pid, t3_nodes[0].balance, get_status(t3_nodes[0].status));
    fprintf(stdout, format,
            t3_users[1].pid, t3_users[1].balance, get_status(t3_users[1].status),
            t3_nodes[1].pid, t3_nodes[1].balance, get_status(t3_nodes[1].status));
    fprintf(stdout, format,
            t3_users[2].pid, t3_users[2].balance, get_status(t3_users[2].status),
            t3_nodes[2].pid, t3_nodes[2].balance, get_status(t3_nodes[2].status));
    fprintf(stdout, "...                                 ...\n");
    fprintf(stdout, format,
            b3_users[2].pid, b3_users[2].balance, get_status(b3_users[2].status),
            b3_nodes[2].pid, b3_nodes[2].balance, get_status(b3_nodes[2].status));
    fprintf(stdout, format,
            b3_users[1].pid, b3_users[1].balance, get_status(b3_users[1].status),
            b3_nodes[1].pid, b3_nodes[1].balance, get_status(b3_nodes[1].status));
    fprintf(stdout, format,
            b3_users[0].pid, b3_users[0].balance, get_status(b3_users[0].status),
            b3_nodes[0].pid, b3_nodes[0].balance, get_status(b3_nodes[0].status));
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

#endif
