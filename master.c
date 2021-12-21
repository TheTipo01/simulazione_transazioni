#define _GNU_SOURCE

#include "config.c"
#include "user.c"
/*#include "node.c"*/
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
 * TODO: libro mastro come linked list, per sincronizzare usiamo shared memory shit
 * TODO: ogni processo node avrà una message queue a cui gli user inviano messaggi
 * TODO: fare in modo che si possano inviare segnali al master per fare roba™️
 */

int main(int argc, char *argv[]) {
    Config cfg = newConfig();
    int *nodePIDs = malloc(cfg.SO_NODES_NUM * sizeof(int));
    int *usersPIDs = malloc(cfg.SO_USERS_NUM * sizeof(int));
    unsigned int i;
    int currentPid;
    int shID;
    int status;
    sigset_t wset;

    setvbuf(stdout, NULL, _IONBF, 0);

    /* Allocazione memoria per il libro mastro */
    shID = shmget(IPC_PRIVATE, (SO_REGISTRY_SIZE * sizeof(Transazione)) * (SO_BLOCK_SIZE * sizeof(Transazione)),
                  S_IRUSR | S_IWUSR);
    if (shID == -1) {
        fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Creiamo un set in cui mettiamo il segnale che usiamo per far aspettare i processi */
    sigemptyset(&wset);
    sigaddset(&wset, SIGUSR1);

    /* Mascheriamo i segnali che usiamo */
    sigprocmask(SIG_BLOCK, &wset, NULL);

    /*
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                exit(EXIT_FAILURE);
            case 0:
                startNode(&wset, cfg, shID, nodePIDs, usersPIDs);
                return 0;
            default:
                nodePIDs[i]=currentPid;
        }
    }
    */

    for (i = 0; i < cfg.SO_USERS_NUM; i++) {
        switch (currentPid = fork()) {
            case -1:
                exit(EXIT_FAILURE);
            case 0:
                startUser(&wset, cfg, shID, nodePIDs, usersPIDs);
                exit(0);
            default:
                usersPIDs[i] = currentPid;
        }
    }

    fprintf(stdout, "\nFinito!");
    kill(0, SIGUSR1);
    /* Aspettiamo che i processi finiscano */
    waitpid(0, &status, 0);

    /* TODO: check per i segnali*/

    return 0;
}