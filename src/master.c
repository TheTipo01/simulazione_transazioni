#define _GNU_SOURCE

#include "config.c"
#include "user.h"
#include "node.h"
#include "structure.h"
#include "../vendor/libsem.c"
#include "utilities.c"

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

/*
 * master: crea SO_USERS_NUM processi utente, gestisce simulazione
 * user: fa transaction
 * node: elabora transaction e riceve reward
 *
 * TODO: fare in modo che si possano inviare segnali al master per fare roba™️
 */

int main(int argc, char *argv[]) {
    Config cfg = newConfig();
    unsigned int *readerCounter, execTime = 0;
    int i, j = 0, currentPid, status, *stop;
    sigset_t wset;
    struct timespec *delay;
    struct Transazione **libroMastro;
    struct SharedMemoryID ids;
    Processo *nodePIDs, *usersPIDs;

    /* Disattiviamo il buffering */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Seeding di rand con il tempo attuale */
    srand(time(NULL));

    /* Allocazione memoria per il libro mastro */
    ids.ledger = shmget(IPC_PRIVATE,
                        SO_REGISTRY_SIZE * (SO_BLOCK_SIZE * sizeof(struct Transazione)),
                        S_IRUSR | S_IWUSR);
    if (ids.ledger == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Allocazione del semaforo di lettura come variabile in memoria condivisa */
    ids.readCounter = shmget(IPC_PRIVATE, sizeof(unsigned int), S_IRUSR | S_IWUSR);
    if (ids.readCounter == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Allocazione dell'array dello stato dei nodi */
    ids.nodePIDs = shmget(IPC_PRIVATE, cfg.SO_NODES_NUM * sizeof(int), S_IRUSR | S_IWUSR);
    if (ids.nodePIDs == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Allocazione dell'array dello stato dei nodi */
    ids.usersPIDs = shmget(IPC_PRIVATE, cfg.SO_USERS_NUM * sizeof(int), S_IRUSR | S_IWUSR);
    if (ids.usersPIDs == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Allocazione del flag per far terminare correttamente i processi */
    ids.stop = shmget(IPC_PRIVATE, sizeof(int), S_IRUSR | S_IWUSR);
    if (ids.stop == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Creiamo un set in cui mettiamo il segnale che usiamo per far aspettare i processi */
    sigemptyset(&wset);
    sigaddset(&wset, SIGUSR1);

    /* Mascheriamo i segnali che usiamo */
    sigprocmask(SIG_BLOCK, &wset, NULL);

    /* Inizializziamo i semafori che usiamo */
    ids.sem = semget(IPC_PRIVATE, FINE_SEMAFORI, IPC_CREAT);

    /* Inizializziamo il semaforo di lettura a 0 */
    readerCounter = shmat(ids.readCounter, NULL, 0);
    *readerCounter = 0;
    shmdt_error_checking(&ids.readCounter);

    /* Inizializziamo il flag di terminazione a 1 (verrà abbassato quando tutti i processi devono terminare) */
    stop = shmat(ids.stop, NULL, 0);
    *stop = -1;

    /* Collegamento del libro mastro (ci serve per controllo dati) */
    libroMastro = shmat(ids.ledger, NULL, 0);

    /* Collegamento all'array dello stato dei processi */
    nodePIDs = shmat(ids.nodePIDs, NULL, 0);



    /* Avviamo i processi node */
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                exit(EXIT_FAILURE);
            case 0:
                startNode(cfg, ids, i);
                exit(0);
            default:
                nodePIDs[i].pid = currentPid;
                nodePIDs[i].balance = 0;
                nodePIDs[i].status = PROCESS_WAITING;
                nodePIDs[i].transactions = 0;
        }
    }

    /* Collegamento all'array dello stato dei processi utente */
    usersPIDs = shmat(ids.usersPIDs, NULL, 0);

    /* Avviamo i processi user */
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                exit(EXIT_FAILURE);
            case 0:
                startUser(cfg, ids, i);
                exit(0);
            default:
                usersPIDs[i].pid = currentPid;
                usersPIDs[i].balance = cfg.SO_BUDGET_INIT;
                usersPIDs[i].status = PROCESS_WAITING;
                usersPIDs[i].transactions = -1;
        }
    }

    /* Notifichiamo i processi che gli array dei pid sono stati popolati */
    kill(0, SIGUSR1);

    /*
     * Tempo di esecuzione della simulazione = SO_SIM_SEC.
     * Se finisce il tempo/tutti i processi utente finiscono/il libro mastro è pieno, terminare la simulazione.
     */
    delay->tv_nsec = 1000000000;
    if (!fork()) {
        stop = shmat(ids.stop, NULL, 0);
        while (execTime < cfg.SO_SIM_SEC && *stop < -1) {
            printStatus(nodePIDs, usersPIDs, &cfg);
            nanosleep(delay, NULL);
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
        fprintf(stdout, "       #%d, balance = %d\n", usersPIDs[i].pid, usersPIDs[i].balance);
    }
    fprintf(stdout, "\n");

    /* Stampa del bilancio di ogni processo nodo */
    fprintf(stdout, "PROCESSI NODO:\n");
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        fprintf(stdout, "       #%d, balance = %d\n", nodePIDs[i].pid, nodePIDs[i].balance);
    }
    fprintf(stdout, "\n");

    /* Stampa degli utenti terminati a causa delle molteplici transazioni fallite */
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        if (usersPIDs[i].status == PROCESS_FINISHED_PREMATURELY) j++;
    }
    fprintf(stdout, "Numero di processi utente terminati prematuramente : %d\n\n", j);

    /* Stampa del numero di blocchi nel libro mastro */
    j = 0;
    for (i = 0; i < SO_REGISTRY_SIZE; i++) {
        if (libroMastro[i] != NULL) j++;
    }
    fprintf(stdout, "Numero di blocchi nel libro mastro: %d\n\n", j);

    /* Stampa del numero di transazioni nella TP di ogni nodo */
    fprintf(stdout, "Numero di transazioni presenti nella TP di ogni processo nodo:\n");
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        fprintf(stdout, "       #%d, transactions = %d\n", nodePIDs[i].pid, nodePIDs[i].transactions);
    }

    /* Cleanup prima di uscire: detach di tutte le shared memory, e impostazione dello stato del nostro processo */
    shmdt_error_checking(nodePIDs);
    shmdt_error_checking(usersPIDs);
    shmdt_error_checking(libroMastro);

    return 0;
}
