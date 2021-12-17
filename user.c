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

#define _GNU_SOURCE

#include "config.h"
#include <time.h>

long calcBilancio(budget) {
    long bilancio;
    int i, j;
    for (i = 0; i < SO_REGISTRY_SIZE; i++){
        for(j = 0; j < SO_BLOCK_SIZE; j++){
            bilancio += blockchain[i][j].quantity;
        }
    }
    return bilancio;
}

void startUser() {
    struct timespec my_time;
    long bilancio = calcBilancio(SO_BUDGET_INIT);
    if (bilancio >= 2) {
        /*TODO: fare roba del figlio dopo aver fatto la mem condivisa*/
        my_time.tv_nsec = rand() % (SO_MAX_TRANS_GEN_NSEC + 1 - SO_MIN_TRANS_GEN_NSEC) + SO_MIN_TRANS_GEN_NSEC;
        nanosleep(&my_time, NULL);
    } else {
        my_time.tv_nsec = rand() % (SO_MAX_TRANS_GEN_NSEC + 1 - SO_MIN_TRANS_GEN_NSEC) + SO_MIN_TRANS_GEN_NSEC;
        nanosleep(&my_time, NULL);
    }
}
