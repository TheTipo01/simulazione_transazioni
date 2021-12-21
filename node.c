/*
 * Memorizza privatamente la lista di transazioni sulla transaction pool
 * di grandezza SO_TP_SIZE (> SO_BLOCK_SIZE)
 *
 * Se TP piena, viene scartata, e user avvisa master di ciò
 *
 * SO_BLOCK_SIZE-1 transazioni processate a blocchi di grandezza SO_BLOCK_SIZE
 *
 * Creazione di un blocco:
 *      - estrazione dalla TP SO_BLOCK_SIZE_-1 trans. non ancora presenti nel libro mastro;
 *      - aggiungere reward con seguenti caratteristiche:
 *           timestamp: clock_gettime()
 *           sender: -1 (con macro)
 *           receiver: id nodo corrente
 *           quantità: somma di tutti i reward delle transazioni nel blocco
 *           reward: 0
 *
 * Elaborazione del blocco attraverso attesa non attiva di un intervallo temporale
 * espresso in nanosec. [SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC]
 *
 * Quando trans. elaborata, scrive il nuovo blocco nel libro mastro e pulisce TP
 */

#include "config.c"
#include "structure.h"
#include "lib/libsem.h"

#include <signal.h>

#define BLOCK_SENDER -1

void startNode(sigset_t *wset, Config cfg, int shID, int *nodePIDs, int *usersPIDs) {
    Transazione transactionPool[cfg.SO_TP_SIZE];
    int sig;

    /* Buffer dell'output sul terminale impostato ad asincrono in modo da ricevere comunicazioni dai child */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Child aspettano un segnale dal parent: possono iniziare la loro funzione solo dopo che vengono generati
     * tutti gli altri child */
    sigwait(wset, &sig);


}