#define _GNU_SOURCE

#include "config.h"
#include "user.h"
#include "node.h"
#include "structure.h"
#include "../vendor/libsem.h"
#include "utilities.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#define PERMS 0666

/*
 * master: crea SO_USERS_NUM processi utente, gestisce simulazione
 * user: fa transaction
 * node: elabora transaction e riceve reward
 */

int main(int argc, char *argv[]) {
    Config cfg = newConfig();
    unsigned int execTime = 0;
    int i, j = 0, currentPid, status, *stop;
    sigset_t wset;
    struct SharedMemory sh;
    struct SharedMemoryID ids;

    setvbuf(stdout, NULL, _IONBF, 0);

    /* Allocazione memoria per il libro mastro */
    ids.ledger = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE * sizeof(Blocco), PERMS);
    shmget_error_checking(ids.ledger);

    /* Allocazione del semaforo di lettura come variabile in memoria condivisa */
    ids.readCounter = shmget(IPC_PRIVATE, sizeof(unsigned int), PERMS);
    shmget_error_checking(ids.readCounter);

    /* Allocazione dell'array dello stato dei nodi */
    ids.nodePIDs = shmget(IPC_PRIVATE, cfg.SO_NODES_NUM * sizeof(ProcessoNode), PERMS);
    shmget_error_checking(ids.nodePIDs);

    /* Allocazione dell'array dello stato dei nodi */
    ids.usersPIDs = shmget(IPC_PRIVATE, cfg.SO_USERS_NUM * sizeof(ProcessoUser), PERMS);
    shmget_error_checking(ids.usersPIDs);

    /* Allocazione del flag per far terminare correttamente i processi */
    ids.stop = shmget(IPC_PRIVATE, sizeof(int), PERMS);
    shmget_error_checking(ids.stop);

    /* Creiamo un set in cui mettiamo il segnale che usiamo per far aspettare i processi */
    sigemptyset(&wset);
    sigaddset(&wset, SIGUSR1);

    /* Mascheriamo i segnali che usiamo */
    sigprocmask(SIG_BLOCK, &wset, NULL);

    /* Inizializziamo i semafori che usiamo */
    ids.sem = semget(IPC_PRIVATE, FINE_SEMAFORI + cfg.SO_USERS_NUM + 1, IPC_CREAT | PERMS);
    TEST_ERROR;

    /* Inizializziamo il semaforo di lettura a 0 */
    sh.readCounter = shmat(ids.readCounter, NULL, 0);
    *sh.readCounter = 0;
    shmdt_error_checking(sh.readCounter);

    /* Inizializziamo il flag di terminazione a -1 (verrà abbassato quando tutti i processi devono terminare) */
    stop = shmat(ids.stop, NULL, 0);
    *stop = -1;

    /* Collegamento del libro mastro (ci serve per controllo dati) */
    sh.libroMastro = shmat(ids.ledger, NULL, 0);
    sh.libroMastro->freeBlock = 0;

    /* Collegamento all'array dello stato dei processi */
    sh.nodePIDs = shmat(ids.nodePIDs, NULL, 0);

    /* Avviamo i processi node */
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                fprintf(stderr, "Error forking\n");
                exit(EXIT_FAILURE);
            case 0:
                startNode(cfg, ids, i);
                exit(0);
            default:
                sh.nodePIDs[i].pid = currentPid;
                sh.nodePIDs[i].balance = 0;
                sh.nodePIDs[i].status = PROCESS_WAITING;
                sh.nodePIDs[i].transactions = 0;
                sh.nodePIDs[i].msgID = msgget(IPC_PRIVATE, IPC_CREAT | PERMS);
                fprintf(stdout, "%d\n", sh.nodePIDs[i].msgID);
        }
    }

    /* Collegamento all'array dello stato dei processi utente */
    sh.usersPIDs = shmat(ids.usersPIDs, NULL, 0);

    /* Avviamo i processi user */
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                fprintf(stderr, "Error forking\n");
                exit(EXIT_FAILURE);
            case 0:
                startUser(cfg, ids, i);
                exit(0);
            default:
                sh.usersPIDs[i].pid = currentPid;
                sh.usersPIDs[i].balance = cfg.SO_BUDGET_INIT;
                sh.usersPIDs[i].status = PROCESS_WAITING;
        }
    }

    /* Notifichiamo i processi che gli array dei pid sono stati popolati */
    kill(0, SIGUSR1);

    /*
     * Tempo di esecuzione della simulazione = SO_SIM_SEC.
     * Se finisce il tempo/tutti i processi utente finiscono/il libro mastro è pieno, terminare la simulazione.
     */
    if (!fork()) {
        stop = shmat(ids.stop, NULL, 0);
        while (execTime < cfg.SO_SIM_SEC && *stop == -1) {
            sleeping(1000000000);
            printStatus(sh.nodePIDs, sh.usersPIDs, &cfg);
            execTime++;
        }

        if (execTime == cfg.SO_SIM_SEC) {
            *stop = TIMEDOUT;
        }

        shmdt_error_checking(stop);
        return 0;
    }

    /* Aspettiamo che i processi finiscano */
    waitpid(0, &status, 0);

    /*
     * Dopo la terminazione dei processi nodo e utente, cominciamo la terminazione della simulazione con la
     * stampa del riepilogo
     */

    /* Stampa della causa di terminazione della simulazione */
    switch (*stop) {
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
        fprintf(stdout, "       #%d, balance = %d\n", sh.usersPIDs[i].pid, sh.usersPIDs[i].balance);
    }
    fprintf(stdout, "\n");

    /* Stampa del bilancio di ogni processo nodo */
    fprintf(stdout, "PROCESSI NODO:\n");
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        fprintf(stdout, "       #%d, balance = %d\n", sh.nodePIDs[i].pid, sh.nodePIDs[i].balance);
    }
    fprintf(stdout, "\n");

    /* Stampa degli utenti terminati a causa delle molteplici transazioni fallite */
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (sh.usersPIDs[i].status == PROCESS_FINISHED_PREMATURELY) j++;
    }
    fprintf(stdout, "Numero di processi utente terminati prematuramente : %d\n\n", j);

    /* Stampa del numero di blocchi nel libro mastro */
    fprintf(stdout, "Numero di blocchi nel libro mastro: %d\n\n", sh.libroMastro->freeBlock);

    /* Stampa del numero di transazioni nella TP di ogni nodo */
    fprintf(stdout, "Numero di transazioni presenti nella TP di ogni processo nodo:\n");
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        fprintf(stdout, "       #%d, transactions = %d\n", sh.nodePIDs[i].pid, sh.nodePIDs[i].transactions);
    }

    sleeping(1000000000);

    /* Cleanup prima di uscire: detach di tutte le shared memory, e impostazione dello stato del nostro processo */
    shmdt_error_checking(sh.nodePIDs);
    shmdt_error_checking(sh.usersPIDs);
    shmdt_error_checking(sh.libroMastro);
    semctl(ids.sem, 0, IPC_RMID);

    sleeping(1000000000);

    return 0;
}
