#define _GNU_SOURCE

#include "config.h"
#include "user.c"
#include "node.c"
#include "structure.h"
#include <unistd.h>
#include <stdlib.h>

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
    Transazione blockchain[SO_REGISTRY_SIZE][SO_BLOCK_SIZE];
    unsigned int i;

    for (i = 0; i < SO_NODES_NUM; i++) {
        switch (fork()) {
            case -1:
                exit(EXIT_FAILURE);
            case 0:
                startNode();
                break;
            default:
                break;
                /* TODO: immagazzinare pid */
        }
    }

    for (i = 0; i < SO_USERS_NUM; i++) {
        switch (fork()) {
            case -1:
                exit(EXIT_FAILURE);
            case 0:
                startUser();
                break;
            default:
                break;
                /* TODO: immagazzinare pid */
        }
    }

    /* TODO: check per i segnali*/
}