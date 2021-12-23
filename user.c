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
 * Dovrà anche gestire un segnale per generare una transazione (segnale da scegliere) --> usare un handler
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
#include "lib/libsem.c"
#include "enum.c"
#include "utilities.c"

#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/shm.h>

int failedTransaction = 0;

long calcBilancio(long balance, int semID, Transazione **lm, unsigned int *readCounter) {
    int i, j;

    /* Dobbiamo eseguire una lettura del libro mastro, quindi impostiamo il semaforo di lettura */
    read_start(semID, readCounter);

    /* Cicliamo all'interno del libro mastro per controllare ogni transazione eseguita */
    for (i = 0; i < SO_REGISTRY_SIZE; i++) {

        /* Se in posizione i c'è un blocco, controlliamo il contenuto */
        if (lm[i] != NULL) {
            for (j = 0; j < SO_BLOCK_SIZE; j++) {

                /* Se nella transazione il ricevente è l'utente stesso, allora
                 * aggiungiamo al bilancio la quantità della transazione */
                if (lm[i][j].receiver == getpid()) {
                    balance += lm[i][j].quantity;
                }

                /* Se nella transazione il mittente è l'utente stesso, allora
                 * sottraiamo al bilancio la quantità della transazione */
                if (lm[i][j].sender == getpid()) {
                    balance -= lm[i][j].quantity;
                }
            }
        }
    }

    /* Rilasciamo il semaforo di lettura a fine operazione */
    read_end(semID, readCounter);

    return balance;
}

void startUser(sigset_t *wset, Config cfg, int ledgerShID, int *nodePIDs, int *usersPIDs, int semID, int readCounterShID) {
    Transazione *t;
    struct timespec *my_time;
    int sig, uID, nID;
    long bilancio = cfg.SO_BUDGET_INIT, msgType;
    Transazione **libroMastro;
    unsigned int *readerCounter;

    /* Collegamento del libro mastro */
    libroMastro = shmat(ledgerShID, NULL, 0);

    readerCounter = shmat(readCounterShID, NULL, 0);

    /* Buffer dell'output sul terminale impostato ad asincrono in modo da ricevere comunicazioni dai child */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Child aspettano un segnale dal parent: possono iniziare la loro funzione solo dopo che vengono generati
     * tutti gli altri child */
    sigwait(wset, &sig);

    /* Creo la coda con chiave PID dell'utente */
    uID = msgget(getpid(), IPC_CREAT);

    while (failedTransaction < cfg.SO_RETRY) {
        t = malloc(sizeof(Transazione));

        /* Calcolo del bilancio dell'utente */
        bilancio += calcBilancio(bilancio, semID, libroMastro, readerCounter);

        if (bilancio >= 2) {
            pid_t receiverPID = usersPIDs[rand() % cfg.SO_USERS_NUM];
            pid_t targetNodePID = nodePIDs[rand() % cfg.SO_NODES_NUM];

            /* Creazione coda di messaggi del nodo scelto come target */
            nID = msgget(targetNodePID, IPC_CREAT);

            t->sender = getpid();
            t->receiver = receiverPID;
            clock_gettime(CLOCK_REALTIME, my_time);
            t->timestamp = *my_time;
            t->quantity = rand() % (cfg.SO_BUDGET_INIT + 1 - 2) + 2;
            if (cfg.SO_REWARD < 1) t->reward = 1;
            else t->reward = cfg.SO_REWARD;

            /* Invio al nodo la transazione */
            msgsnd(nID, &t, sizeof(Transazione), 0);

            t = NULL;

            /* Aspetto il messaggio di successo/fallimento */
            msgrcv(uID, &t, sizeof(Transazione), msgType, 0);
            if (t == NULL) failedTransaction++;

            my_time->tv_nsec =
                    rand() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) + cfg.SO_MIN_TRANS_GEN_NSEC;
            nanosleep(my_time, NULL);
        }
        /* no more moners :( */
        /* TODO: aspettare finché bilancio positivo */

        my_time->tv_nsec =
                rand() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) + cfg.SO_MIN_TRANS_GEN_NSEC;
        nanosleep(my_time, NULL);
    }
}
