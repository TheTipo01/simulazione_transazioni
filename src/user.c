#define _GNU_SOURCE

#include "config.h"
#include "enum.h"
#include "../vendor/libsem.h"
#include "structure.h"
#include "utilities.h"
#include "master.h"
#include "rwlock.h"

#include <signal.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

int lastBlockCheckedUser = 0, failedTransactionUser = 0;
unsigned int user_position;

double calcEntrate(int semID, struct Blocco *lm, unsigned int *readCounter) {
    int pid = getpid(), j;
    double tmpBalance = 0;

    /* Dobbiamo eseguire una lettura del libro mastro, quindi impostiamo il
     * semaforo di lettura */
    read_start(semID, readCounter, LEDGER_READ, LEDGER_WRITE);

    /* Cicliamo all'interno del libro mastro per controllare ogni transazione
     * eseguita */
    for (; lastBlockCheckedUser < SO_REGISTRY_SIZE &&
           lastBlockCheckedUser < *sh.ledger_free_block; lastBlockCheckedUser++) {
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
    read_end(semID, readCounter, LEDGER_READ, LEDGER_WRITE);

    return tmpBalance;
}

void transactionGenerator(int sig) {
    struct Messaggio msg;
    int feedback;

    /* Puntatore al mittente e destinatario scelto casualmente (il PID verrà preso dalla lista di nodi/utenti a cui
     * si accede col puntatore ottenuto) */
    long receiver = random() % cfg.SO_USERS_NUM;
    long targetNode = random() % cfg.SO_NODES_NUM;

    /* Generazione della transazione */
    msg.transazione.sender = getpid();
    msg.transazione.receiver = sh.users_pid[receiver].pid;
    msg.transazione.timestamp = time(NULL);
    if (cfg.SO_REWARD < 1)
        msg.transazione.reward = 1;
    else
        msg.transazione.reward = cfg.SO_REWARD;

    msg.transazione.quantity = random() % (cfg.SO_BUDGET_INIT + 1 - 2) + 2;

    /* Se abbiamo generato una quantità superiore al bilancio, prendiamo il rimanente del bilancio */
    if (msg.transazione.quantity > sh.users_pid[user_position].balance) {
        msg.transazione.quantity = sh.users_pid[user_position].balance - 2;
    }

    msg.m_type = 1;

    /* Invio al nodo la transazione */
    feedback = msgsnd(sh.nodes_pid[targetNode].msg_id, &msg, msg_size(), 0);

    sem_reserve(ids.sem, (int) (FINE_SEMAFORI + user_position));
    if (feedback == -1) {
        /* Se la transazione fallisce, aumentiamo il contatore delle transazioni
         * fallite di 1 */
        failedTransactionUser++;
    } else {
        /* Se la transazione ha avuto successo, aggiorniamo il bilancio con
         * l'uscita appena effettuata */
        sh.users_pid[user_position].balance -=
                (unsigned int) (msg.transazione.quantity * ((float) (100 - msg.transazione.reward) / 100.0));

        /* Se il segnale ricevuto è SIGUSR2, significa che questa funzione è stata chiamata inviando un segnale ad un processo utente */
        if (sig == SIGUSR2) {
            sem_reserve(ids.sem, MMTS);
            sh.mmts[*sh.mmts_free_block] = msg.transazione;
            *sh.mmts_free_block += 1;
            sem_release(ids.sem, MMTS);
        }
    }
    sem_release(ids.sem, (int) (FINE_SEMAFORI + user_position));
}

void startUser(unsigned int index) {
    int sig, j, k = 0;
    sigset_t wset;

    /* Seeding di rand con il pid del processo */
    srandom(getpid());

    /* Copia parametri in variabili globali */
    user_position = index;

    /* Creiamo un set in cui mettiamo il segnale che usiamo per far aspettare i processi */
    sigemptyset(&wset);
    sigaddset(&wset, SIGUSR1);

    /*
     * Child aspettano un segnale dal parent: possono iniziare la loro funzione solo dopo che vengono generati
     * tutti gli altri child
     */
    sigwait(&wset, &sig);

    /* Mascheriamo i segnali che usiamo */
    sigprocmask(SIG_SETMASK, &wset, NULL);

    /* Aggiunge l'handler del segnale SIGUSR2, designato per far scatenare la generazione di una transazione */
    signal(SIGUSR2, transactionGenerator);

    /* Ciclo principale di generazione delle transazioni */
    while (failedTransactionUser < cfg.SO_RETRY && get_stop_value(sh.stop, sh.stop_read) == -1) {
        sh.users_pid[user_position].status = PROCESS_RUNNING;
        sem_reserve(ids.sem, (int) (FINE_SEMAFORI + user_position));

        /* Calcolo del bilancio dell'utente: calcoliamo solo le entrate, in quanto
         * le uscite vengono registrate dopo */
        sh.users_pid[user_position].balance +=
                calcEntrate(ids.sem, sh.ledger, sh.ledger_read);

        sem_release(ids.sem, (int) (FINE_SEMAFORI + user_position));

        /* Se l'utente ha un bilancio abbastanza alto... */
        if (sh.users_pid[user_position].balance >= 2) {
            /* ...viene generata la transazione. */
            transactionGenerator(0);

            sleeping(random() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) +
                     cfg.SO_MIN_TRANS_GEN_NSEC);
        } else {
            sleeping(random() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) +
                     cfg.SO_MIN_TRANS_GEN_NSEC);

            /* Se non c'è abbastanza bilancio, aspettiamo finchè l'utente non riceve una transazione */
            sh.users_pid[user_position].status = PROCESS_WAITING;
            sigwait(&wset, &sig);
        }
    }

    /* Cleanup prima di uscire */

    /* Set dello status basandoci sulla quantità di transazioni fallite */
    if (failedTransactionUser != cfg.SO_RETRY)
        sh.users_pid[user_position].status = PROCESS_FINISHED;
    else
        sh.users_pid[user_position].status = PROCESS_FINISHED_PREMATURELY;

    /* Se non c'è stato un ordine di uscire, controlliamo se gli altri processi
     * abbiano finito */
    if (get_stop_value(sh.stop, sh.stop_read) < 0) {
        /* Contiamo il numero di processi che non hanno finito */
        for (j = 0; j < cfg.SO_USERS_NUM; j++) {
            if (sh.users_pid[j].status != PROCESS_FINISHED &&
                sh.users_pid[j].status != PROCESS_FINISHED_PREMATURELY)
                k++;
        }

        /* Se non ne esistono più, facciamo terminare i nodi. */
        if (k == 0) {
            sem_reserve(ids.sem, STOP_WRITE);
            *sh.stop = FINISHED;
            sem_release(ids.sem, STOP_WRITE);
        }
    }
}
