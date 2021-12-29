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

int failedTransaction = 0, i = 0;

unsigned int calcEntrate(int semID, struct Transazione **lm, unsigned int *readCounter) {
    int pid = getpid(), j;
    unsigned int tmpBalance = 0;

    /* Dobbiamo eseguire una lettura del libro mastro, quindi impostiamo il semaforo di lettura */
    read_start(semID, readCounter);

    /* Cicliamo all'interno del libro mastro per controllare ogni transazione eseguita */
    for (; i < SO_REGISTRY_SIZE && lm[i] != NULL; i++) {
        /* Se in posizione i c'è un blocco, controlliamo il contenuto */
        for (j = 0; j < SO_BLOCK_SIZE; j++) {
            /* Se nella transazione il ricevente è l'utente stesso, allora
             * aggiungiamo al bilancio la quantità della transazione */
            if (lm[i][j].receiver == pid) {
                tmpBalance += lm[i][j].quantity;
            }
        }
    }

    /* Rilasciamo il semaforo di lettura a fine operazione */
    read_end(semID, readCounter);

    return tmpBalance;
}

void startUser(sigset_t *wset, Config cfg, int ledgerShID, int nodePIDsID, int usersPIDsID, int semID,
               int readCounterShID, unsigned int userIndex) {
    struct timespec *my_time;
    int sig, uID, nID;
    long msgType;
    struct Transazione **libroMastro;
    unsigned int *readerCounter;
    Processo *nodePIDs, *usersPIDs;
    struct Messaggio *msg;
    struct Transazione *msg_feedback;

    /* Buffer dell'output sul terminale impostato ad asincrono in modo da ricevere comunicazioni dai child */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Collegamento del libro mastro */
    libroMastro = shmat(ledgerShID, NULL, 0);

    readerCounter = shmat(readCounterShID, NULL, 0);

    /* Creo la coda con chiave PID dell'utente */
    uID = msgget(getpid(), IPC_CREAT);

    /* Collegamento all'array dello stato dei processi utente */
    usersPIDs = shmat(usersPIDsID, NULL, 0);

    /* Collegamento all'array dello stato dei processi nodo */
    nodePIDs = shmat(nodePIDsID, NULL, 0);

    /* Child aspettano un segnale dal parent: possono iniziare la loro funzione solo dopo che vengono generati
     * tutti gli altri child */
    sigwait(wset, &sig);

    while (failedTransaction < cfg.SO_RETRY) {
        msg = malloc(sizeof(struct Messaggio));

        /* Calcolo del bilancio dell'utente: calcoliamo solo le entrate, in quanto le uscite vengono registrate dopo */
        usersPIDs[userIndex].balance += calcEntrate(semID, libroMastro, readerCounter);

        if (usersPIDs[userIndex].balance >= 2) {
            pid_t receiverPID = usersPIDs[rand() % cfg.SO_USERS_NUM].pid;
            pid_t targetNodePID = nodePIDs[rand() % cfg.SO_NODES_NUM].pid;

            /* Creazione coda di messaggi del nodo scelto come target */
            nID = msgget(targetNodePID, IPC_CREAT);

            msg->transazione.sender = getpid();
            msg->transazione.receiver = receiverPID;
            clock_gettime(CLOCK_REALTIME, my_time);
            msg->transazione.timestamp = *my_time;
            if (cfg.SO_REWARD < 1) msg->transazione.reward = 1;
            else msg->transazione.reward = cfg.SO_REWARD;
            msg->transazione.quantity = rand() % (cfg.SO_BUDGET_INIT + 1 - 2) + 2;

            msg->m_type = 0;

            /* Invio al nodo la transazione */
            msgsnd(nID, &msg, sizeof(struct Transazione), 0);

            /* Aspetto il messaggio di successo/fallimento */
            msgrcv(uID, &msg_feedback, sizeof(struct Transazione), 0, 0);
            if (msg_feedback == NULL) {
                /* Se la transazione fallisce, aumentiamo il contatore delle transazioni fallite di 1 */
                failedTransaction++;
            } else {
                /* Se la transazione ha avuto successo, aggiorniamo il bilancio con l'uscita appena effettuata */
                usersPIDs[userIndex].balance -= msg->transazione.quantity * ((100 - msg->transazione.reward) / 100);
            }

            my_time->tv_nsec =
                    rand() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) + cfg.SO_MIN_TRANS_GEN_NSEC;
            nanosleep(my_time, NULL);

        } else {
            /* no more moners :( */
            /* TODO: fare aspettare lo user finché il suo bilancio non è positivo, inviandogli un messaggio per risvegliarlo */
            my_time->tv_nsec =
                    rand() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) + cfg.SO_MIN_TRANS_GEN_NSEC;
            nanosleep(my_time, NULL);
        }
    }

    /* Cleanup prima di uscire */

    /* Detach di tutte le shared memory */
    shmdt_error_checking(libroMastro);
    shmdt_error_checking(readerCounter);
    shmdt_error_checking(usersPIDs);
    shmdt_error_checking(nodePIDs);

    /* Chiusura delle code di messaggi utilizzate */
    msgctl(uID, IPC_RMID, NULL);

    /* Impostazione dello stato del nostro processo */
    usersPIDs[userIndex].status = PROCESS_FINISHED;
}
