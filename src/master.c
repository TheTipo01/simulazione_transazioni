#define _GNU_SOURCE

#include "config.h"
#include "user.h"
#include "node.h"
#include "structure.h"
#include "../vendor/libsem.h"
#include "utilities.h"
#include "shared_memory.h"
#include "master.h"

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

/*
 * master: crea SO_USERS_NUM processi utente, gestisce simulazione
 * user: fa transaction
 * node: elabora transaction e riceve reward
 */

int main(int argc, char *argv[]) {
    unsigned int execTime = 0;
    int i, j = 0, currentPid, status;
    sigset_t wset;

    setvbuf(stdout, NULL, _IONBF, 0);

    cfg = newConfig();

    get_shared_ids();
    attach_shared_memory();

    *sh.stop = -1;
    *sh.freeBlock = 0;
    *sh.readCounter = 0;

    /* Creiamo un set in cui mettiamo il segnale che usiamo per far aspettare i processi */
    sigemptyset(&wset);
    sigaddset(&wset, SIGUSR1);

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
                sh.nodePIDs[i].pid = currentPid;
                sh.nodePIDs[i].balance = 0;
                sh.nodePIDs[i].status = PROCESS_WAITING;
                sh.nodePIDs[i].transactions = 0;
                sh.nodePIDs[i].msgID = msgget(IPC_PRIVATE, IPC_CREAT | PERMS);
                fprintf(stdout, "%d\n", sh.nodePIDs[i].msgID);
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
        sh.stop = shmat(ids.stop, NULL, 0);
        while (execTime < cfg.SO_SIM_SEC && *sh.stop == -1) {
            sleeping(1000000000);
            printStatus(sh.nodePIDs, sh.usersPIDs, &cfg);
            execTime++;
        }

        if (execTime == cfg.SO_SIM_SEC) {
            *sh.stop = TIMEDOUT;
        }

        shmdt_error_checking(sh.stop);
        return 0;
    }

/*
    Aspettiamo che i processi finiscano
    for(i = 0; i < cfg.SO_USERS_NUM; i++) {
        waitpid(sh.usersPIDs[i].pid, &status, 0);
    }
 */

    waitpid(-1, &status, 0);

    /*
     * Dopo la terminazione dei processi nodo e utente, cominciamo la terminazione della simulazione con la
     * stampa del riepilogo
     */

    /* Stampa della causa di terminazione della simulazione */
    switch (*sh.stop) {
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
    fprintf(stdout, "Numero di blocchi nel libro mastro: %d\n\n", *sh.freeBlock);

    /* Stampa del numero di transazioni nella TP di ogni nodo */
    fprintf(stdout, "Numero di transazioni presenti nella TP di ogni processo nodo:\n");
    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        fprintf(stdout, "       #%d, transactions = %d\n", sh.nodePIDs[i].pid, sh.nodePIDs[i].transactions);
    }

    sleeping(1000000000);

    fprintf(stdout, "cleanup starting \n");
    /* Cleanup prima di uscire: detach di tutte le shared memory, e impostazione dello stato del nostro processo */
    shmdt_error_checking(sh.nodePIDs);
    shmdt_error_checking(sh.usersPIDs);
    shmdt_error_checking(sh.ledger);
    semctl(ids.sem, 0, IPC_RMID);
    shmctl(ids.nodePIDs, IPC_RMID, NULL);
    shmctl(ids.usersPIDs, IPC_RMID, NULL);
    shmctl(ids.readCounter, IPC_RMID, NULL);
    shmctl(ids.stop, IPC_RMID, NULL);

    sleeping(1000000000);

    return 0;
}
