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

void startNode(sigset_t *wset, Config cfg, int ledgerShID, int nodePIDsID, int usersPIDsID, int semID,
               int readCounterShID, unsigned int nodeIndex) {
    Transazione **trnsToProcess = malloc(SO_BLOCK_SIZE * sizeof(Transazione));
    Transazione **transactionPool = malloc(cfg.SO_TP_SIZE * sizeof(Transazione));
    int sig, i = 0, j, k, last = 0, qID, sID;
    long msgType;
    unsigned int sum, *readerCounter;
    struct timespec *cur_time, wait_time;
    Transazione tmp;
    Transazione **libroMastro;
    Processo *nodePIDs, *usersPIDs;

    /* Buffer dell'output sul terminale impostato ad asincrono in modo da ricevere comunicazioni dai child */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Collegamento all'array dello stato dei processi utente */
    usersPIDs = shmat(usersPIDsID, NULL, 0);

    /* Collegamento all'array dello stato dei processi nodo */
    nodePIDs = shmat(nodePIDsID, NULL, 0);

    /* Creo la coda con chiave PID del nodo */
    qID = msgget(getpid(), IPC_CREAT);

    /* Ci attacchiamo al libro mastro */
    libroMastro = shmat(ledgerShID, NULL, 0);

    readerCounter = shmat(readCounterShID, NULL, 0);

    /*
     * Child aspettano un segnale dal parent: possono iniziare la loro funzione solo dopo che vengono generati
     * tutti gli altri child
     */
    sigwait(wset, &sig);

    while (1) {
        while (transactionPool[i + SO_BLOCK_SIZE - 2] == NULL && last != cfg.SO_TP_SIZE) {
            msgrcv(qID, &tmp, sizeof(Transazione), msgType, 0);
            transactionPool[last]->quantity = tmp.quantity * ((100 - tmp.reward) / 100);
            transactionPool[last]->reward = tmp.reward;
            transactionPool[last]->sender = tmp.sender;
            transactionPool[last]->receiver = tmp.receiver;
            transactionPool[last]->timestamp = tmp.timestamp;
            nodePIDs[nodeIndex].balance += tmp.quantity * (tmp.reward / 100);
            last++;
        }

        sID = msgget(tmp.sender, IPC_CREAT);

        /*
         * Passo successivo: creazione del blocco avente SO_BLOCK_SIZE-1 transazioni presenti nella TP.
         * Se però la TP è piena, la transazione viene scartata: si avvisa il sender
         */
        if (last != cfg.SO_TP_SIZE) {
            /* Transazione accettata: inviamo la transazione indietro */
            msgsnd(sID, &tmp, sizeof(Transazione), IPC_NOWAIT);

            /* Creazione blocco */
            j = 0;
            for (; i < cfg.SO_TP_SIZE && j < SO_BLOCK_SIZE - 1; i++) {
                trnsToProcess[i] = transactionPool[i];
                j++;
            }
            i += SO_BLOCK_SIZE;

            /* Popolazione blocco */
            clock_gettime(CLOCK_REALTIME, cur_time);
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
            sem_reserve(semID, LEDGER_WRITE);
            for (k = 0; k < SO_REGISTRY_SIZE; k++) {
                if (libroMastro[i] == NULL) {
                    libroMastro[i] = *trnsToProcess;
                }
            }
            sem_release(semID, LEDGER_WRITE);

            /* Allochiamo nuova memoria per le transazioni da processare */
            trnsToProcess = malloc(SO_BLOCK_SIZE * sizeof(Transazione));
        } else {
            /* Notifichiamo l'utente, rinviandogli la transazione rifiutata */
            msgsnd(sID, NULL, sizeof(Transazione), IPC_NOWAIT);
        }
    }

    /* Cleanup prima di uscire */

    /* Detach di tutte le shared memory */
    shmdt_error_checking(nodePIDs);
    shmdt_error_checking(usersPIDs);
    shmdt_error_checking(libroMastro);

    /* Chiusura delle code di messaggi */
    msgctl(qID, IPC_RMID, NULL);

    /* Impostazione dello stato del nostro processo */
    nodePIDs[nodeIndex].status = PROCESS_FINISHED;
}