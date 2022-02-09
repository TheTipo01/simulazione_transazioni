#define _GNU_SOURCE

#include "config.h"
#include "user.h"
#include "node.h"
#include "structure.h"
#include "utilities.h"
#include "shared_memory.h"
#include "master.h"
#include "../vendor/libsem.h"
#include "rwlock.h"
#include "enum.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

#define last_index cfg.SO_NODES_NUM - 1
/*
 * master: crea SO_USERS_NUM processi utente, gestisce simulazione
 * user: fa transaction
 * node: elabora transaction e riceve reward
 */

Config cfg;
struct SharedMemory sh;
struct SharedMemoryID ids;

int main(int argc, char *argv[]) {
    unsigned int exec_time = 0;
    int i, j, cont, current_pid, status, *friend, node_to_send;
    struct Messaggio temp_tran;
    struct Messaggio_int temp_pid;
    sigset_t wset;

    /* Disattiviamo il buffering di stdout per stampare subito lo stato della simulazione */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Lettura del file di configurazione a partire dalle variabili d'ambiente */
    cfg = new_config();

    /* Ottenimento degli id per la shared memory */
    get_shared_ids();
    /* E successivo attach */
    attach_shared_memory();

    /* In questo punto del codice solo il processo master può leggere/scrivere sulle shared memory */
    *sh.stop = -1;
    *sh.new_nodes_pid = ids.nodes_pid;
    *sh.nodes_num = cfg.SO_NODES_NUM;

    /* I semafori usati li usiamo come mutex: inizializziamo i loro valori ad 1 */
    for (i = 0; i < NUM_SEMAFORI; i++) {
        sem_set_val(ids.sem, i, 1);
    }

    /*
     * Inizializziamo la coda di messaggi del processo master, utilizzata per ricevere transazioni dai processi nodo,
     * che poi dovrà mandare ad altri processi nodo creati da lui stesso.
     */
    ids.master_msg_id = msgget(IPC_PRIVATE, GET_FLAGS);

    /* E anche la coda di messaggi usata per inviare i nodi amici nuovi da aggiungere */
    ids.msg_friends = msgget(IPC_PRIVATE, GET_FLAGS);

    /* Creiamo un set in cui mettiamo il segnale che usiamo per far aspettare i processi */
    sigemptyset(&wset);
    sigaddset(&wset, SIGUSR1);
    sigaddset(&wset, SIGUSR2);

    /* Mascheriamo i segnali che usiamo */
    sigprocmask(SIG_SETMASK, &wset, NULL);

    /* Avviamo i processi node */
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        switch (current_pid = fork()) {
            case -1:
                fprintf(stderr, "Error forking\n");
                exit(EXIT_FAILURE);
            case 0:
                start_node(i);
                exit(EXIT_SUCCESS);
            default:
                sh.nodes_pid[i].pid = current_pid;
                sh.nodes_pid[i].balance = 0;
                sh.nodes_pid[i].status = PROCESS_WAITING;
                sh.nodes_pid[i].last = 0;
                sh.nodes_pid[i].msg_id = msgget(IPC_PRIVATE, GET_FLAGS);
                sh.nodes_pid[i].friends = shmget(IPC_PRIVATE, sizeof(int) * cfg.SO_NUM_FRIENDS, GET_FLAGS);
                friend = shmat(sh.nodes_pid[i].friends, NULL, 0);
                for (j = 0; j < cfg.SO_NUM_FRIENDS; j++) {
                    friend[j] = (int) (random() % cfg.SO_NODES_NUM);
                }
                shmdt_error_checking(friend);
        }
    }

    /* Avviamo i processi user */
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        switch (current_pid = fork()) {
            case -1:
                fprintf(stderr, "Error forking\n");
                exit(EXIT_FAILURE);
            case 0:
                start_user(i);
                exit(EXIT_SUCCESS);
            default:
                sh.users_pid[i].pid = current_pid;
                sh.users_pid[i].balance = cfg.SO_BUDGET_INIT;
                sh.users_pid[i].status = PROCESS_WAITING;
        }
    }

    /* Notifichiamo i processi che gli array dei n sono stati popolati */
    kill(0, SIGUSR1);

    if (!fork()) {
        /* Blocchiamo anche nel fork i segnali usati dai processi user/node */
        sigprocmask(SIG_SETMASK, &wset, NULL);

        while (get_stop_value() == -1) {
            /* Ricezione della transazione che ha fatto SO_HOPS salti */
            msgrcv(ids.master_msg_id, &temp_tran, msg_size(), 1, 0);
            if (errno) {
                if (get_stop_value() != -1) {
                    exit(EXIT_SUCCESS);
                } else {
                    fprintf(stderr, "%s:%d: PID=%5d: Error %d (%s)\n", __FILE__, __LINE__, getpid(), errno,
                            strerror(errno));
                }
            }

            sem_reserve(ids.sem, NODES_PID_WRITE);

            /* Espansione dell'array nodes_pid e aumento del numero di nodi presenti */
            expand_node();

            fprintf(stderr, "CALABRIA\n");

            /* Creazione del nuovo nodo */
            switch (current_pid = fork()) {
                case -1:
                    fprintf(stderr, "Error forking\n");
                    exit(EXIT_FAILURE);
                case 0:
                    start_node(last_index);
                    exit(EXIT_SUCCESS);
                default:
                    sh.nodes_pid[last_index].pid = current_pid;
                    sh.nodes_pid[last_index].balance = 0;
                    sh.nodes_pid[last_index].status = PROCESS_WAITING;
                    sh.nodes_pid[last_index].last = 0;
                    sh.nodes_pid[last_index].msg_id = msgget(IPC_PRIVATE, GET_FLAGS);
                    sh.nodes_pid[last_index].friends = shmget(IPC_PRIVATE,
                                                              sizeof(int) * cfg.SO_NUM_FRIENDS, GET_FLAGS);
                    sem_release(ids.sem, NODES_PID_WRITE);

                    friend = shmat(get_node(last_index).friends, NULL, 0);

                    temp_pid.n = (int) last_index;

                    for (j = 0; j < cfg.SO_NUM_FRIENDS; j++) {
                        /* Popolazione della lista amici del nuovo nodo */
                        friend[j] = (int) (random() % cfg.SO_NODES_NUM);

                        node_to_send = (int) (random() % cfg.SO_NODES_NUM);
                        temp_pid.m_type = get_node(node_to_send).pid;

                        /* Selezionamento di SO_NUM_FRIENDS nodi a cui deve essere aggiunto il nuovo nodo nella lista amici */
                        msgsnd(ids.msg_friends, &temp_pid, sizeof(struct Messaggio_int) - sizeof(long), 0);
                        TEST_ERROR;
                    }

                    shmdt_error_checking(friend);
                    msgsnd(get_node(temp_pid.n).msg_id, &temp_tran, msg_size(), 0);
                    kill(current_pid, SIGUSR1);
                    TEST_ERROR;
            }
        }
        exit(EXIT_SUCCESS);
    }

    /*
     * Tempo di esecuzione della simulazione = SO_SIM_SEC.
     * Usiamo un altra fork per generare un processo che tenga conto del tempo di esecuzione e stampi a schermo i dati.
     * Se finisce il tempo/tutti i processi utente finiscono/il libro mastro è pieno, terminare la simulazione.
     */
    if (!fork()) {
        /* Blocchiamo anche nel fork i segnali usati dai processi user/node */
        sigprocmask(SIG_SETMASK, &wset, NULL);

        do {
            check_for_update();
            print_more_status(sh.nodes_pid, sh.users_pid);
            exec_time++;
        } while (exec_time < cfg.SO_SIM_SEC && !sleeping(1000000000) && get_stop_value() == -1);

        if (exec_time == cfg.SO_SIM_SEC) {
            sem_reserve(ids.sem, STOP_WRITE);
            *sh.stop = TIMEDOUT;
            sem_release(ids.sem, STOP_WRITE);
        }

        /* Per forzare i nodi a uscire, eliminiamo la loro message queue */
        delete_message_queue();

        exit(EXIT_SUCCESS);
    }

    /* Aspettiamo che i processi finiscano */
    for (i = 0; i < cfg.SO_NODES_NUM + cfg.SO_USERS_NUM; i++) {
        waitpid(-1, &status, 0);
        check_for_update();
    }

    /*
     * Dopo la terminazione dei processi nodo e utente, cominciamo la terminazione della simulazione con la
     * stampa del riepilogo
     */

    /* Stampa della causa di terminazione della simulazione */
    switch (get_stop_value()) {
        case LEDGERFULL:
            fprintf(stdout, "--- TERMINE SIMULAZIONE ---\nCausa terminazione: il libro mastro è pieno.\n\n");
            break;
        case TIMEDOUT:
            fprintf(stdout, "--- TERMINE SIMULAZIONE ---\nCausa terminazione: sono passati %d secondi.\n\n",
                    cfg.SO_SIM_SEC);
            break;
        case NOBALANCE:
            fprintf(stdout, "--- TERMINE SIMULAZIONE ---\nCausa terminazione: il bilancio dei processi non"
                            "è più sufficiente per eseguire alcuna transazione.\n\n");
            break;
        default:
            fprintf(stdout,
                    "--- TERMINE SIMULAZIONE ---\nCausa terminazione: tutti i processi utente sono terminati.\n\n");
    }

    /* Stampa dello stato e del bilancio di ogni processo utente */
    fprintf(stdout, "PROCESSI UTENTE:\n");
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        fprintf(stdout, "       #%d, balance = %d\n", sh.users_pid[i].pid, sh.users_pid[i].balance);
    }
    fprintf(stdout, "\n");

    /* Stampa del bilancio di ogni processo nodo */
    fprintf(stdout, "PROCESSI NODO:\n");
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        fprintf(stdout, "       #%d, balance = %d\n", sh.nodes_pid[i].pid, sh.nodes_pid[i].balance);
    }
    fprintf(stdout, "\n");

    /* Stampa degli utenti terminati a causa delle molteplici transazioni fallite */
    cont = 0;
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (sh.users_pid[i].status == PROCESS_FINISHED_PREMATURELY) cont++;
    }
    fprintf(stdout, "Numero di processi utente terminati prematuramente : %d\n\n", cont);

    /* Stampa del numero di blocchi nel libro mastro */
    fprintf(stdout, "Numero di blocchi nel libro mastro: %d\n\n", *sh.ledger_free_block);

    /* Stampa del numero di transazioni nella TP di ogni nodo */
    fprintf(stdout, "Numero di transazioni presenti nella TP di ogni processo nodo:\n");
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        fprintf(stdout, "       #%d, transactions = %d\n", sh.nodes_pid[i].pid, sh.nodes_pid[i].last);
    }

    /* Se sono state fatte transazioni tramite CLI, vengono riportate qui. */
    if (sh.mmts[0].timestamp != 0) {
        fprintf(stdout, "\nMan-Made Transactions:\n");
        for (i = 0; i < 10 && sh.mmts[i].timestamp != 0; i++) {
            fprintf(stdout, "       #%d -> #%d, %d$, date: %s\n\n",
                    sh.mmts[i].sender, sh.mmts[i].receiver, sh.mmts[i].quantity, format_time(sh.mmts[i].timestamp));
        }
    }

    /* Cleanup finale */
    delete_message_queue();
    delete_shared_memory();

    return 0;
}
