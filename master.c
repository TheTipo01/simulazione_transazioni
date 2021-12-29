#define _GNU_SOURCE

#include "config.c"
#include "user.c"
#include "node.c"
#include "structure.h"
#include "lib/libsem.c"

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
    unsigned int i, j, *readerCounter, execTime = 0;
    int currentPid, ledgerShID, readCounterShID, status, semID, nodePIDsID, usersPIDsID;
    sigset_t wset;
    struct timespec *delay;
    Transazione **libroMastro;
    Processo *nodePIDs, *usersPIDs;

    /* Disattiviamo il buffering */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Seeding di rand con il tempo attuale */
    srand(time(NULL));

    /* Allocazione memoria per il libro mastro */
    ledgerShID = shmget(IPC_PRIVATE, (SO_REGISTRY_SIZE * sizeof(Transazione)) * (SO_BLOCK_SIZE * sizeof(Transazione)),
                        S_IRUSR | S_IWUSR);
    if (ledgerShID == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Allocazione del semaforo di lettura come variabile in memoria condivisa */
    readCounterShID = shmget(IPC_PRIVATE, sizeof(unsigned int), S_IRUSR | S_IWUSR);
    if (readCounterShID == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Allocazione dell'array dello stato dei nodi */
    nodePIDsID = shmget(IPC_PRIVATE, cfg.SO_NODES_NUM * sizeof(int), S_IRUSR | S_IWUSR);
    if (nodePIDsID == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Allocazione dell'array dello stato dei nodi */
    usersPIDsID = shmget(IPC_PRIVATE, cfg.SO_USERS_NUM * sizeof(int), S_IRUSR | S_IWUSR);
    if (usersPIDsID == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Creiamo un set in cui mettiamo il segnale che usiamo per far aspettare i processi */
    sigemptyset(&wset);
    sigaddset(&wset, SIGUSR1);

    /* Mascheriamo i segnali che usiamo */
    sigprocmask(SIG_BLOCK, &wset, NULL);

    /* Inizializziamo i semafori che usiamo */
    semID = semget(IPC_PRIVATE, FINE_SEMAFORI, IPC_CREAT);

    /* Inizializziamo il semaforo di lettura a 0 */
    readerCounter = shmat(readCounterShID, NULL, 0);
    readerCounter = 0;
    shmdt(&readCounterShID);

    /* Collegamento del libro mastro (ci serve per controllo dati) */
    libroMastro = shmat(ledgerShID, NULL, 0);

    /* Collegamento all'array dello stato dei processi */
    nodePIDs = shmat(nodePIDsID, NULL, 0);

    /* Avviamo i processi node */
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                exit(EXIT_FAILURE);
            case 0:
                startNode(&wset, cfg, ledgerShID, nodePIDsID, usersPIDsID, semID, readCounterShID, i);
                return 0;
            default:
                nodePIDs[i].pid = currentPid;
                nodePIDs[i].balance = 0;
                nodePIDs[i].status = PROCESS_WAITING;
        }
    }

    /* Collegamento all'array dello stato dei processi utente */
    usersPIDs = shmat(usersPIDsID, NULL, 0);

    /* Avviamo i processi user */
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                exit(EXIT_FAILURE);
            case 0:
                startUser(&wset, cfg, ledgerShID, nodePIDsID, usersPIDsID, semID, readCounterShID, i);
                exit(0);
            default:
                usersPIDs[i].pid = currentPid;
                usersPIDs[i].balance = cfg.SO_BUDGET_INIT;
                usersPIDs[i].status = PROCESS_WAITING;
        }
    }

    /* Notifichiamo i processi che gli array dei pid sono stati popolati */
    kill(0, SIGUSR1);

    /*
     * Tempo di esecuzione della simulazione = SO_SIM_SEC.
     * Se finisce il tempo/tutti i processi utente finiscono/il libro mastro è pieno, terminare la simulazione.
     */
    delay->tv_nsec = 1000000000;
    if (fork()) {
        /* TODO: inserire check per fine tempo esecuzione */
        while (j != -1) {
            j = 0;
            if (libroMastro[SO_REGISTRY_SIZE] != NULL) j = -1;
            for (i = 0; i < cfg.SO_USERS_NUM && j != -1; i++) {
                if (usersPIDs[i].status == PROCESS_RUNNING || usersPIDs[i].status == PROCESS_WAITING) j++;
            }
            /* TODO: fare in modo che i nodes finiscono quando tutti gli users finiscono */
            if (j == 0) j = -1;
        }
    } else {
        while (execTime < cfg.SO_SIM_SEC) {
            printStatus(nodePIDs, usersPIDs, &cfg);
            nanosleep(delay, NULL);
            execTime++;
        }
        /* TODO: inviare msg di fine exec al master */
    }

    /* Aspettiamo che i processi utente finiscano (DA FIXARE)*/
    waitpid(0, &status, 0);

    /* TODO: check per i segnali*/

    /* Cleanup prima di uscire: detach di tutte le shared memory, e impostazione dello stato del nostro processo */
    shmdt_error_checking(nodePIDs);
    shmdt_error_checking(usersPIDs);
    shmdt_error_checking(libroMastro);

    return 0;
}