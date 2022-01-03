#include "config.c"
#include "enum.c"
#include "../vendor/libsem.h"
#include "structure.h"
#include "utilities.c"

#include <signal.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>

int lastBlockChecked = 0;

unsigned int calcEntrate(int semID, struct Transazione **lm, unsigned int *readCounter) {
    int pid = getpid(), j;
    unsigned int tmpBalance = 0;

    /* Dobbiamo eseguire una lettura del libro mastro, quindi impostiamo il
     * semaforo di lettura */
    read_start(semID, readCounter);

    /* Cicliamo all'interno del libro mastro per controllare ogni transazione
     * eseguita */
    for (; lastBlockChecked < SO_REGISTRY_SIZE && lm[lastBlockChecked] != NULL; lastBlockChecked++) {
        /* Se in posizione i c'è un blocco, controlliamo il contenuto */
        for (j = 0; j < SO_BLOCK_SIZE; j++) {
            /* Se nella transazione il ricevente è l'utente stesso, allora
             * aggiungiamo al bilancio la quantità della transazione */
            if (lm[lastBlockChecked][j].receiver == pid) {
                tmpBalance += lm[lastBlockChecked][j].quantity;
            }
        }
    }

    /* Rilasciamo il semaforo di lettura a fine operazione */
    read_end(semID, readCounter);

    return tmpBalance;
}

void startUser(Config cfg, struct SharedMemoryID ids, unsigned int index) {
    struct timespec *my_time;
    int sig, uID, nID, *stop, j, k = 0, failedTransaction = 0;
    struct Transazione **libroMastro;
    unsigned int *readerCounter;
    Processo *nodePIDs, *usersPIDs;
    struct Messaggio msg;
    struct Transazione *msg_feedback;
    sigset_t wset;

    /* Buffer dell'output sul terminale impostato ad asincrono in modo da ricevere
     * comunicazioni dai child */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Collegamento del libro mastro */
    libroMastro = shmat(ids.ledger, NULL, 0);

    readerCounter = shmat(ids.readCounter, NULL, 0);

    /* Creo la coda con chiave PID dell'utente */
    uID = msgget(getpid(), IPC_CREAT);

    /* Collegamento all'array dello stato dei processi utente */
    usersPIDs = shmat(ids.usersPIDs, NULL, 0);

    /* Collegamento all'array dello stato dei processi nodo */
    nodePIDs = shmat(ids.nodePIDs, NULL, 0);

    /* Booleano abbassato quando dobbiamo terminare i processi, per gracefully
     * terminare tutto
     */
    stop = shmat(ids.stop, NULL, 0);

    /* Creiamo un set in cui mettiamo il segnale che usiamo per far aspettare i processi */
    sigemptyset(&wset);
    sigaddset(&wset, SIGUSR1);

    /* Mascheriamo i segnali che usiamo */
    sigprocmask(SIG_BLOCK, &wset, NULL);

    /*
     * Child aspettano un segnale dal parent: possono iniziare la loro funzione solo dopo che vengono generati
     * tutti gli altri child
     */
    sigwait(&wset, &sig);


    while (failedTransaction < cfg.SO_RETRY && *stop < 0) {
        /* Calcolo del bilancio dell'utente: calcoliamo solo le entrate, in quanto
         * le uscite vengono registrate dopo */
        usersPIDs[index].balance +=
                calcEntrate(ids.sem, libroMastro, readerCounter);

        if (usersPIDs[index].balance >= 2) {
            pid_t receiverPID = usersPIDs[rand() % cfg.SO_USERS_NUM].pid;
            pid_t targetNodePID = nodePIDs[rand() % cfg.SO_NODES_NUM].pid;

            /* Creazione coda di messaggi del nodo scelto come target */
            nID = msgget(targetNodePID, IPC_CREAT);

            msg.transazione.sender = getpid();
            msg.transazione.receiver = receiverPID;
            clock_gettime(CLOCK_REALTIME, my_time);
            msg.transazione.timestamp = *my_time;
            if (cfg.SO_REWARD < 1)
                msg.transazione.reward = 1;
            else
                msg.transazione.reward = cfg.SO_REWARD;
            msg.transazione.quantity = rand() % (cfg.SO_BUDGET_INIT + 1 - 2) + 2;

            msg.m_type = 0;

            /* Invio al nodo la transazione */
            msgsnd(nID, &msg, sizeof(struct Transazione), 0);

            /* Aspetto il messaggio di successo/fallimento */
            msgrcv(uID, &msg_feedback, sizeof(struct Transazione), 0, 0);
            if (msg_feedback->sender == -1) {
                /* Se la transazione fallisce, aumentiamo il contatore delle transazioni
                 * fallite di 1 */
                failedTransaction++;
            } else {
                /* Se la transazione ha avuto successo, aggiorniamo il bilancio con
                 * l'uscita appena effettuata */
                usersPIDs[index].balance -=
                        (unsigned int) (msg.transazione.quantity * ((float) (100 - msg.transazione.reward) / 100.0));
            }

            my_time->tv_nsec =
                    rand() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) +
                    cfg.SO_MIN_TRANS_GEN_NSEC;
            nanosleep(my_time, NULL);
        } else {
            /* no more moners :( */
            my_time->tv_nsec =
                    rand() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) +
                    cfg.SO_MIN_TRANS_GEN_NSEC;
            nanosleep(my_time, NULL);

            /* Aspettiamo finchè l'utente non riceve una transazione */
            sigwait(&wset, &sig);
        }
    }

    /* Cleanup prima di uscire */

    if (failedTransaction == 5)
        usersPIDs[index].status = PROCESS_FINISHED;
    else
        usersPIDs[index].status = PROCESS_FINISHED_PREMATURELY;

    /* Se non c'è stato un ordine di uscire, controlliamo se gli altri processi
     * abbiano finito */
    if (*stop < 0) {
        /* Contiamo il numero di processi che non hanno finito */
        for (j = 0; j < cfg.SO_USERS_NUM; j++) {
            if (usersPIDs[j].status != PROCESS_FINISHED &&
                usersPIDs[j].status != PROCESS_FINISHED_PREMATURELY)
                k++;
        }

        /* Se non ne esistono più, facciamo terminare i nodi. */
        if (k == 0) {
            *stop = FINISHED;
        }
    }

    /* Detach di tutte le shared memory */
    shmdt_error_checking(libroMastro);
    shmdt_error_checking(readerCounter);
    shmdt_error_checking(usersPIDs);
    shmdt_error_checking(nodePIDs);
    shmdt_error_checking(stop);

    /* Chiusura delle code di messaggi utilizzate */
    msgctl(uID, IPC_RMID, NULL);
}
