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
    int *nodePIDs = malloc(cfg.SO_NODES_NUM * sizeof(int));
    int *usersPIDs = malloc(cfg.SO_USERS_NUM * sizeof(int));
    unsigned int i, *readerCounter;
    int currentPid, ledgerShID, readCounterShID, status, semID;
    sigset_t wset;

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

    /* Avviamo i processi node */
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                exit(EXIT_FAILURE);
            case 0:
                startNode(&wset, cfg, ledgerShID, nodePIDs, usersPIDs, semID, readCounterShID);
                return 0;
            default:
                nodePIDs[i] = currentPid;
        }
    }

    /* E i processi user */
    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                exit(EXIT_FAILURE);
            case 0:
                startUser(&wset, cfg, ledgerShID, nodePIDs, usersPIDs, semID, readCounterShID);
                exit(0);
            default:
                usersPIDs[i] = currentPid;
        }
    }

    /* Notifichiamo i processi che gli array dei pid sono stati popolati */
    kill(0, SIGUSR1);

    /* Aspettiamo che i processi utente finiscano (DA FIXARE)*/
    waitpid(0, &status, 0);

    /* TODO: check per i segnali*/

    return 0;
}