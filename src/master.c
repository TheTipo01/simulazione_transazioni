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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

/*
 * master: crea SO_USERS_NUM processi utente, gestisce simulazione
 * user: fa transaction
 * node: elabora transaction e riceve reward
 */

Config cfg;
struct SharedMemory sh;
struct SharedMemoryID ids;

int main(int argc, char *argv[]) {
    unsigned int execTime = 0;
    int i, cont, currentPid, status;
    sigset_t wset;

    /* Disattiviamo il buffering di stdout per stampare subito lo stato della simulazione */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Lettura del file di configurazione a partire dalle variabili d'ambiente */
    cfg = newConfig();

    /* Ottenimento degli id per la shared memory */
    get_shared_ids();
    /* E successivo attach */
    attach_shared_memory();

    /* In questo punto del codice solo il processo master può leggere/scrivere su stop */
    *sh.stop = -1;

    /* I semafori usati li usiamo come mutex: inizializziamo i loro valori ad 1 */
    for (i = 0; i < NUM_SEMAFORI; i++) {
        semctl(ids.sem, i, SETVAL, 1);
    }

    /* Creiamo un set in cui mettiamo il segnale che usiamo per far aspettare i processi */
    sigemptyset(&wset);
    sigaddset(&wset, SIGUSR1);
    sigaddset(&wset, SIGUSR2);

    /* Mascheriamo i segnali che usiamo */
    sigprocmask(SIG_BLOCK, &wset, NULL);

    /* Avviamo i processi node */
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                fprintf(stderr, "Error forking\n");
                exit(EXIT_FAILURE);
            case 0:
                startNode(i);
                exit(EXIT_SUCCESS);
            default:
                sh.nodes_pid[i].pid = currentPid;
                sh.nodes_pid[i].balance = 0;
                sh.nodes_pid[i].status = PROCESS_WAITING;
                sh.nodes_pid[i].transactions = 0;
                sh.nodes_pid[i].msg_id = msgget(IPC_PRIVATE, GET_FLAGS);
        }
    }

    /* Avviamo i processi user */
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                fprintf(stderr, "Error forking\n");
                exit(EXIT_FAILURE);
            case 0:
                startUser(i);
                exit(EXIT_SUCCESS);
            default:
                sh.users_pid[i].pid = currentPid;
                sh.users_pid[i].balance = cfg.SO_BUDGET_INIT;
                sh.users_pid[i].status = PROCESS_WAITING;
        }
    }

    /* Notifichiamo i processi che gli array dei pid sono stati popolati */
    kill(0, SIGUSR1);

    /*
     * Tempo di esecuzione della simulazione = SO_SIM_SEC.
     * Usiamo un altra fork per generare un processo che tenga conto del tempo di esecuzione e stampi a schermo i dati.
     * Se finisce il tempo/tutti i processi utente finiscono/il libro mastro è pieno, terminare la simulazione.
     */
    if (!fork()) {
        sigprocmask(SIG_BLOCK, &wset, NULL);
        do {
            print_more_status(sh.nodes_pid, sh.users_pid);
            execTime++;
        } while (execTime < cfg.SO_SIM_SEC && !sleeping(1000000000) && get_stop_value(sh.stop, sh.stop_read) == -1);

        if (execTime == cfg.SO_SIM_SEC) {
            sem_reserve(ids.sem, STOP_WRITE);
            *sh.stop = TIMEDOUT;
            sem_release(ids.sem, STOP_WRITE);
        }

        /* Segnale di stop per i processi child */
        kill(0, SIGUSR1);

        /* Per forzare i nodi a uscire, eliminiamo la loro message queue */
        delete_message_queue();

        exit(EXIT_SUCCESS);
    }

    /* Aspettiamo che i processi finiscano */
    for (i = 0; i < cfg.SO_NODES_NUM + cfg.SO_USERS_NUM; i++) {
        waitpid(-1, &status, 0);
    }

    /*
     * Dopo la terminazione dei processi nodo e utente, cominciamo la terminazione della simulazione con la
     * stampa del riepilogo
     */

    /* Stampa della causa di terminazione della simulazione */
    switch (get_stop_value(sh.stop, sh.stop_read)) {
        case LEDGERFULL:
            fprintf(stdout, "--- TERMINE SIMULAZIONE ---\nCausa terminazione: il libro mastro è pieno.\n\n");
            break;
        case TIMEDOUT:
            fprintf(stdout, "--- TERMINE SIMULAZIONE ---\nCausa terminazione: sono passati %d secondi.\n\n",
                    cfg.SO_SIM_SEC);
            break;
        default:
            fprintf(stdout,
                    "--- TERMINE SIMULAZIONE ---\nCausa terminazione: tutti i processi utente sono terminati.\n\n");
    }

    /* Stampa dello stato e del bilancio di ogni processo utente */
    fprintf(stdout, "PROCESSI UTENTE:\n");
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        fprintf(stdout, "       #%d, balance = %.2f\n", sh.users_pid[i].pid, sh.users_pid[i].balance);
    }
    fprintf(stdout, "\n");

    /* Stampa del bilancio di ogni processo nodo */
    fprintf(stdout, "PROCESSI NODO:\n");
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        fprintf(stdout, "       #%d, balance = %.2f\n", sh.nodes_pid[i].pid, sh.nodes_pid[i].balance);
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
        fprintf(stdout, "       #%d, transactions = %d\n", sh.nodes_pid[i].pid, sh.nodes_pid[i].transactions);
    }

    /* Se sono state fatte transazioni tramite CLI, vengono riportate qui. */
    if (sh.mmts[0].timestamp != 0) {
        fprintf(stdout, "\nMan-Made Transactions:\n");
        for (i = 0; i < 10 && sh.mmts[i].timestamp != 0; i++) {
            fprintf(stdout, "       #%d -> #%d, %.2f$, date: %s\n\n",
                    sh.mmts[i].sender, sh.mmts[i].receiver, sh.mmts[i].quantity, format_time(sh.mmts[i].timestamp));
        }
    }

    /* Cleanup finale */
    detach_and_delete();

    return 0;
}
