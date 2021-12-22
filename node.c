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
 *      - aggiungere transazione finale di reward con seguenti caratteristiche:
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
#include "lib/libsem.c"
#include "enum.c"

#include <signal.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define BLOCK_SENDER -1

void startNode(sigset_t *wset, Config cfg, int shID, int *nodePIDs, int *usersPIDs, int semID) {
    Transazione **trnsToProcess = malloc(SO_BLOCK_SIZE * sizeof(Transazione));
    Transazione **transactionPool = malloc(cfg.SO_TP_SIZE * sizeof(Transazione));
    int sig, i, j, last = 0, qID;
    long msgType;
    unsigned int sum;
    struct timespec *cur_time, wait_time;
    Transazione tmp;
    Transazione **libroMastro;

    /* Buffer dell'output sul terminale impostato ad asincrono in modo da ricevere comunicazioni dai child */
    setvbuf(stdout, NULL, _IONBF, 0);

    /*
     * Child aspettano un segnale dal parent: possono iniziare la loro funzione solo dopo che vengono generati
     * tutti gli altri child
     */
    sigwait(wset, &sig);

    /* Creo la coda con chiave PID del nodo */
    qID = msgget(getpid(), IPC_CREAT | IPC_EXCL);

    /* Ci attacchiamo al libro mastro */
    libroMastro = shmat(shID, NULL, 0);

    /* Puntatore che serve per cominciare a leggere dalle transazioni nuove, in modo da
     * non inserire transazioni vecchie nel blocco da creare */
    i = 0;

    /* ---------loop--------- */
    while (transactionPool[i + SO_BLOCK_SIZE - 2] == NULL && last != cfg.SO_TP_SIZE) {
        msgrcv(qID, &tmp, sizeof(Transazione), msgType, 0);
        transactionPool[last]->quantity = tmp.quantity;
        transactionPool[last]->reward = tmp.reward;
        transactionPool[last]->sender = tmp.sender;
        transactionPool[last]->receiver = tmp.receiver;
        transactionPool[last]->timestamp = tmp.timestamp;
        last++;
    }

    /* Passo successivo: creazione del blocco avente SO_BLOCK_SIZE-1 transazioni presenti nella TP.
     * Se però la TP è piena, si avvisa il sender */
    if (last != cfg.SO_TP_SIZE) {

        /* Creazione blocco */
        j = 0;
        for (; i < cfg.SO_TP_SIZE && j < SO_BLOCK_SIZE - 1; i++) {
            trnsToProcess[i] = transactionPool[i];
            j++;
        }
        i += SO_BLOCK_SIZE;

        /* Popolazione blocco */
        clock_gettime(CLOCK_REALTIME, time);
        trnsToProcess[SO_BLOCK_SIZE - 1]->timestamp = *cur_time;
        trnsToProcess[SO_BLOCK_SIZE - 1]->sender = BLOCK_SENDER;
        trnsToProcess[SO_BLOCK_SIZE - 1]->receiver = getpid();
        trnsToProcess[SO_BLOCK_SIZE - 1]->reward = 0;
        for (i = 0; i < SO_BLOCK_SIZE - 1; i++) {
            sum += trnsToProcess[i]->quantity;
        }
        trnsToProcess[SO_BLOCK_SIZE - 1]->quantity = sum;

        /* Simulazione elaborazione del blocco */
        wait_time.tv_nsec =
                rand() % (cfg.SO_MAX_TRANS_PROC_NSEC + 1 - cfg.SO_MIN_TRANS_PROC_NSEC) + cfg.SO_MIN_TRANS_PROC_NSEC;
        nanosleep(&wait_time, NULL);

        /* Scrittura in libro mastro con sincronizzazione (semaforo) */
        sem_reserve(semID, LEDGER);

        sem_release(semID, LEDGER);
    } else {

    }
    /* ---------loop--------- */
}