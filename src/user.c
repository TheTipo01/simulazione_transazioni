#include "config.h"
#include "enum.c"
#include "../vendor/libsem.h"
#include "structure.h"
#include "utilities.h"

#include <signal.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

int lastBlockCheckedUser = 0, failedTransactionUser = 0, uIDUser;
struct SharedMemory shUser;
Config cfgUser;
unsigned int positionUser;
struct SharedMemoryID idsUser;

unsigned int calcEntrate(int semID, Blocco *lm, unsigned int *readCounter) {
    int pid = getpid(), j;
    unsigned int tmpBalance = 0;

    /* Dobbiamo eseguire una lettura del libro mastro, quindi impostiamo il
     * semaforo di lettura */
    read_start(semID, readCounter);

    /* Cicliamo all'interno del libro mastro per controllare ogni transazione
     * eseguita */
    for (; lastBlockCheckedUser < SO_REGISTRY_SIZE &&
           lastBlockCheckedUser < shUser.libroMastro->freeBlock; lastBlockCheckedUser++) {
        /* Se in posizione i c'è un blocco, controlliamo il contenuto */
        for (j = 0; j < SO_BLOCK_SIZE; j++) {
            /* Se nella transazione il ricevente è l'utente stesso, allora
             * aggiungiamo al bilancio la quantità della transazione */
            if (lm[lastBlockCheckedUser].transazioni[j].receiver == pid) {
                tmpBalance += lm[lastBlockCheckedUser].transazioni[j].quantity;
            }
        }
    }

    /* Rilasciamo il semaforo di lettura a fine operazione */
    read_end(semID, readCounter);

    return tmpBalance;
}

void transactionGenerator(int signal) {
    struct Messaggio msg;
    int feedback;

    long receiver = random() % cfgUser.SO_USERS_NUM;
    long targetNode = random() % cfgUser.SO_NODES_NUM;

    msg.transazione.sender = getpid();
    msg.transazione.receiver = shUser.usersPIDs[receiver].pid;
    msg.transazione.timestamp = time(NULL);
    if (cfgUser.SO_REWARD < 1)
        msg.transazione.reward = 1;
    else
        msg.transazione.reward = cfgUser.SO_REWARD;

    msg.transazione.quantity = random() % (cfgUser.SO_BUDGET_INIT + 1 - 2) + 2;

    msg.m_type = 1;

    /* Invio al nodo la transazione */
    feedback = msgsnd(shUser.nodePIDs[targetNode].msgID, &msg, msg_size(), 0);
    fprintf(stdout, "feedback: %d - quantity %d\n", feedback, msg.transazione.quantity);
    fflush(stdout);

    if (feedback == -1) {
        /* Se la transazione fallisce, aumentiamo il contatore delle transazioni
         * fallite di 1 */
        failedTransactionUser++;
    } else {
        /* Se la transazione ha avuto successo, aggiorniamo il bilancio con
         * l'uscita appena effettuata */
        shUser.usersPIDs[positionUser].balance -=
                (unsigned int) (msg.transazione.quantity * ((float) (100 - msg.transazione.reward) / 100.0));
    }
}

void startUser(Config lclCfg, struct SharedMemoryID lclIds, unsigned int lclIndex) {
    int sig, j, k = 0, cont = 0;
    long wt;
    sigset_t wset;

    setvbuf(stdout, NULL, _IONBF, SHM_W | SHM_R);

    /* Copia parametri in variabili globali */
    cfgUser = lclCfg;
    positionUser = lclIndex;
    idsUser = lclIds;

    /* Seeding di rand con il pid del processo */
    srand(getpid());

    /* Collegamento del libro mastro */
    shUser.libroMastro = shmat(idsUser.ledger, NULL, 0);
    TEST_ERROR;

    shUser.readCounter = shmat(idsUser.readCounter, NULL, 0);
    TEST_ERROR;

    /* Creo la coda con chiave PID dell'utente */
    uIDUser = msgget(getpid(), IPC_CREAT | 0666);
    msgget_error_checking(uIDUser);

    /* Collegamento all'array dello stato dei processi utente */
    shUser.usersPIDs = shmat(idsUser.usersPIDs, NULL, 0);
    TEST_ERROR;

    /* Collegamento all'array dello stato dei processi nodo */
    shUser.nodePIDs = shmat(idsUser.nodePIDs, NULL, 0);
    TEST_ERROR;

    /* Booleano abbassato quando dobbiamo terminare i processi, per gracefully
     * terminare tutto
     */
    shUser.stop = shmat(idsUser.stop, NULL, 0);
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
    shUser.usersPIDs[positionUser].status = PROCESS_RUNNING;

    /* Aggiunge l'handler del segnale SIGUSR2, designato per far scatenare la generazione di una transazione */
    signal(SIGUSR2, transactionGenerator);

    while (failedTransactionUser < cfgUser.SO_RETRY && *shUser.stop == -1) {
        sem_reserve(idsUser.sem, FINE_SEMAFORI + positionUser);

        /* Calcolo del bilancio dell'utente: calcoliamo solo le entrate, in quanto
         * le uscite vengono registrate dopo */
        shUser.usersPIDs[positionUser].balance +=
                calcEntrate(idsUser.sem, shUser.libroMastro, shUser.readCounter);

        if (shUser.usersPIDs[positionUser].balance >= 2) {
            fprintf(stdout, "PID=%d: generazioni transazione numero %d - bilancio %d\n", getpid(), cont,
                    shUser.usersPIDs[positionUser].balance);
            transactionGenerator(2580);
            cont++;

            wt = random() % (cfgUser.SO_MAX_TRANS_GEN_NSEC + 1 - cfgUser.SO_MIN_TRANS_GEN_NSEC) +
                 cfgUser.SO_MIN_TRANS_GEN_NSEC;
            sleeping(wt);

        } else {
            /* no more moners :( */
            fprintf(stdout, "PID=%d: finiti soldi\n", getpid());

            wt = random() % (cfgUser.SO_MAX_TRANS_GEN_NSEC + 1 - cfgUser.SO_MIN_TRANS_GEN_NSEC) +
                 cfgUser.SO_MIN_TRANS_GEN_NSEC;
            sleeping(wt);

            /* Aspettiamo finchè l'utente non riceve una transazione */
            sigwait(&wset, &sig);
        }

        sem_release(idsUser.sem, FINE_SEMAFORI + positionUser);
    }

    fprintf(stdout, "PID=%d: esco\n", getpid());
    /* Cleanup prima di uscire */

    if (failedTransactionUser == cfgUser.SO_RETRY)
        shUser.usersPIDs[positionUser].status = PROCESS_FINISHED;
    else
        shUser.usersPIDs[positionUser].status = PROCESS_FINISHED_PREMATURELY;

    /* Se non c'è stato un ordine di uscire, controlliamo se gli altri processi
     * abbiano finito */
    if (*shUser.stop < 0) {
        /* Contiamo il numero di processi che non hanno finito */
        for (j = 0; j < cfgUser.SO_USERS_NUM; j++) {
            if (shUser.usersPIDs[j].status != PROCESS_FINISHED &&
                shUser.usersPIDs[j].status != PROCESS_FINISHED_PREMATURELY)
                k++;
        }

        /* Se non ne esistono più, facciamo terminare i nodi. */
        if (k == 0) {
            *shUser.stop = FINISHED;
        }
    }

    /* Detach di tutte le shared memory */
    shmdt_error_checking(shUser.libroMastro);
    shmdt_error_checking(shUser.readCounter);
    shmdt_error_checking(shUser.usersPIDs);
    shmdt_error_checking(shUser.nodePIDs);
    shmdt_error_checking(shUser.stop);

    /* Chiusura delle code di messaggi utilizzate */
    msgctl(uIDUser, IPC_RMID, NULL);
}
