#include "config.h"
#include "enum.c"
#include "../vendor/libsem.h"
#include "structure.h"
#include "utilities.h"

#include <signal.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>

int lastBlockChecked = 0, failedTransaction = 0, uID;
struct SharedMemory sh;
Config cfg;
unsigned int position;
struct SharedMemoryID ids;

unsigned int calcEntrate(int semID, Blocco *lm, unsigned int *readCounter) {
    int pid = getpid(), j;
    unsigned int tmpBalance = 0;

    /* Dobbiamo eseguire una lettura del libro mastro, quindi impostiamo il
     * semaforo di lettura */
    read_start(semID, readCounter);

    /* Cicliamo all'interno del libro mastro per controllare ogni transazione
     * eseguita */
    for (; lastBlockChecked < SO_REGISTRY_SIZE && lastBlockChecked < sh.libroMastro->freeBlock; lastBlockChecked++) {
        /* Se in posizione i c'è un blocco, controlliamo il contenuto */
        for (j = 0; j < SO_BLOCK_SIZE; j++) {
            /* Se nella transazione il ricevente è l'utente stesso, allora
             * aggiungiamo al bilancio la quantità della transazione */
            if (lm[lastBlockChecked].transazioni[j].receiver == pid) {
                tmpBalance += lm[lastBlockChecked].transazioni[j].quantity;
            }
        }
    }

    /* Rilasciamo il semaforo di lettura a fine operazione */
    read_end(semID, readCounter);

    return tmpBalance;
}

void transactionGenerator(int signal) {
    struct Messaggio msg;
    struct Transazione *msg_feedback;
    struct timespec *time;
    int nID;

    pid_t receiverPID = sh.usersPIDs[rand() % cfg.SO_USERS_NUM].pid;
    pid_t targetNodePID = sh.nodePIDs[rand() % cfg.SO_NODES_NUM].pid;

    /* Creazione coda di messaggi del nodo scelto come target */
    nID = msgget(targetNodePID, IPC_CREAT | 0666);
    TEST_ERROR;

    msg.transazione.sender = getpid();
    msg.transazione.receiver = receiverPID;
    clock_gettime(CLOCK_REALTIME, time);
    msg.transazione.timestamp = *time;
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
        sh.usersPIDs[position].balance -=
                (unsigned int) (msg.transazione.quantity * ((float) (100 - msg.transazione.reward) / 100.0));
    }
}

void startUser(Config lclCfg, struct SharedMemoryID lclIds, unsigned int lclIndex) {
    struct timespec *my_time;
    int sig, j, k = 0;
    sigset_t wset;

    cfg = lclCfg;
    position = lclIndex;
    ids = lclIds;

    /* Seeding di rand con il tempo attuale */
    srand(getpid());

    /* Collegamento del libro mastro */
    sh.libroMastro = shmat(ids.ledger, NULL, 0);
    TEST_ERROR;

    sh.readerCounter = shmat(ids.readCounter, NULL, 0);
    TEST_ERROR;

    /* Creo la coda con chiave PID dell'utente */
    uID = msgget(getpid(), IPC_CREAT | 0666);
    msgget_error_checking(uID);

    /* Collegamento all'array dello stato dei processi utente */
    sh.usersPIDs = shmat(ids.usersPIDs, NULL, 0);
    TEST_ERROR;

    /* Collegamento all'array dello stato dei processi nodo */
    sh.nodePIDs = shmat(ids.nodePIDs, NULL, 0);
    TEST_ERROR;

    /* Booleano abbassato quando dobbiamo terminare i processi, per gracefully
     * terminare tutto
     */
    sh.stop = shmat(ids.stop, NULL, 0);
    TEST_ERROR;

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

    /* Aggiunge l'handler del segnale SIGUSR2, designato per far scatenare la generazione di una transazione */
    signal(SIGUSR2, transactionGenerator);

    while (failedTransaction < cfg.SO_RETRY && *sh.stop == -1) {
        sem_reserve(ids.sem, FINE_SEMAFORI + position);

        /* Calcolo del bilancio dell'utente: calcoliamo solo le entrate, in quanto
         * le uscite vengono registrate dopo */
        sh.usersPIDs[position].balance +=
                calcEntrate(ids.sem, sh.libroMastro, sh.readerCounter);

        if (sh.usersPIDs[position].balance >= 2) {
            transactionGenerator(2580);

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

        sem_release(ids.sem, FINE_SEMAFORI + position);
    }

    /* Cleanup prima di uscire */

    if (failedTransaction == cfg.SO_RETRY)
        sh.usersPIDs[position].status = PROCESS_FINISHED;
    else
        sh.usersPIDs[position].status = PROCESS_FINISHED_PREMATURELY;

    /* Se non c'è stato un ordine di uscire, controlliamo se gli altri processi
     * abbiano finito */
    if (*sh.stop < 0) {
        /* Contiamo il numero di processi che non hanno finito */
        for (j = 0; j < cfg.SO_USERS_NUM; j++) {
            if (sh.usersPIDs[j].status != PROCESS_FINISHED &&
                sh.usersPIDs[j].status != PROCESS_FINISHED_PREMATURELY)
                k++;
        }

        /* Se non ne esistono più, facciamo terminare i nodi. */
        if (k == 0) {
            *sh.stop = FINISHED;
        }
    }

    /* Detach di tutte le shared memory */
    shmdt_error_checking(sh.libroMastro);
    shmdt_error_checking(&sh.readerCounter);
    shmdt_error_checking(&sh.usersPIDs);
    shmdt_error_checking(&sh.nodePIDs);
    shmdt_error_checking(&sh.stop);

    /* Chiusura delle code di messaggi utilizzate */
    msgctl(uID, IPC_RMID, NULL);
}
