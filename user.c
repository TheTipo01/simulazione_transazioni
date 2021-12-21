/*
 * I processi utente si occupano di creare e inviare transazioni ai processi nodo.
 *
 * Ogni processo utente inizia con SO_BUDGET_INIT, e dovrà:
 *     - Calcolare il bilancio, facendo la somma algebrica delle entrate e delle uscite che abbiamo fatto, ancor prima di
 *       essere registrate nel libro mastro
 *           - Se questo bilancio è >= 2, il processo farà le seguenti operazioni:
 *                - Sceglie un processo utente a cui inviare il denaro
 *                - Sceglie un nodo a cui inviare la transazione da processare
 *                - Sceglie un valore intero compreso tra 2 e il suo budget, suddiviso in questa maniera
 *                     * Il reward, una percentuale di SO_REWARD, arrotondato ad un minimo di 1
 *                     * L'importo della transazione, quindi il valore scelto prima - reward
 *           - Se il bilancio è minore di 2, allora il processo non invia alcuna transazione
 *     - Invia al nodo scelto la transazione, e attende un intervallo di tempo tra SO_MIN_TRANS_GEN_NSEC e SO_MAX_TRANS_GEN_NSEC
 *
 * Dovrà anche gestire un segnale per generare una transazione (segnale da scegliere)
 *
 * Funzione transaction inviata da utente a nodo, deve avere:
 *      timestamp
 *      sender
 *      receiver
 *      quantità
 *      reward
 *  e deve essere memorizzata con questi parametri sul libro mastro.
 */

#include "config.c"
#include "structure.h"
#include "lib/libsem.h"

#include <time.h>
#include <unistd.h>
#include <signal.h>

long calcBilancio(budget) {
    long bilancio;
    int i, j;

    bilancio = 0;

    for (i = 0; i < SO_REGISTRY_SIZE; i++) {
        for (j = 0; j < SO_BLOCK_SIZE; j++) {
            /* bilancio += blockchain[i][j].quantity; */
        }
    }
    return bilancio;
}

void startUser(sigset_t *wset, Config cfg, int shID, int *nodePIDs, int *usersPIDs) {
    struct timespec my_time;
    int sig;
    long bilancio;

    /*  */
    setvbuf(stdout, NULL, _IONBF, 0);

    sigwait(wset, &sig);

    bilancio = calcBilancio(cfg.SO_BUDGET_INIT);
    if (bilancio >= 2) {
        pid_t receiverPID = usersPIDs[rand() % cfg.SO_USERS_NUM];

        my_time.tv_nsec =
                rand() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) + cfg.SO_MIN_TRANS_GEN_NSEC;
        nanosleep(&my_time, NULL);
    } else {
        my_time.tv_nsec =
                rand() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) + cfg.SO_MIN_TRANS_GEN_NSEC;
        nanosleep(&my_time, NULL);
    }
}
