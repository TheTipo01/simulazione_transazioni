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

int last_block_checked_user = 0, failed_transaction_user = 0;
unsigned int user_position;

unsigned int calc_entrate(int sem_id, struct Blocco *lm, unsigned int *read_counter) {
    int pid = getpid(), j;
    unsigned int tmp_balance = 0;

    /* Dobbiamo eseguire una lettura del libro mastro, quindi impostiamo il
     * semaforo di lettura */
    read_start(sem_id, read_counter, LEDGER_READ, LEDGER_WRITE);

    /* Cicliamo all'interno del libro mastro per controllare ogni transazione
     * eseguita */
    for (; last_block_checked_user < SO_REGISTRY_SIZE &&
           last_block_checked_user < *sh.ledger_free_block; last_block_checked_user++) {
        /* Se in posizione i c'è un blocco, controlliamo il contenuto */
        for (j = 0; j < SO_BLOCK_SIZE; j++) {
            /* Se nella transazione il ricevente è l'utente stesso, allora
             * aggiungiamo al bilancio la quantità della transazione */
            if (lm[last_block_checked_user].transazioni[j].receiver == pid) {
                tmp_balance += lm[last_block_checked_user].transazioni[j].quantity;
            }
        }
    }

    /* Rilasciamo il semaforo di lettura a fine operazione */
    read_end(sem_id, read_counter, LEDGER_READ, LEDGER_WRITE);

    return tmp_balance;
}

void transaction_generator(int sig) {
    struct Messaggio msg;
    long receiver, target_node;

    /* Puntatore al mittente e destinatario scelto casualmente (il PID verrà preso dalla lista di nodi/utenti a cui
     * si accede col puntatore ottenuto) */
    receiver = random() % cfg.SO_USERS_NUM;
    check_for_update();
    target_node = random() % cfg.SO_NODES_NUM;

    /* Generazione della transazione */
    msg.transazione.sender = getpid();
    msg.transazione.receiver = sh.users_pid[receiver].pid;
    msg.transazione.timestamp = time(NULL);
    if (cfg.SO_REWARD < 1)
        msg.transazione.reward = 1;
    else
        msg.transazione.reward = cfg.SO_REWARD;

    msg.transazione.quantity = random_between_two(2, sh.users_pid[user_position].balance);

    msg.m_type = 1;
    msg.hops = 0;

    /* Invio al nodo la transazione */
    check_for_update();
    msgsnd(get_node(target_node).msg_id, &msg, msg_size(), 0);

    sem_reserve(ids.sem, (int) (FINE_SEMAFORI + user_position));
    /* Aggiorniamo il bilancio del nodo rimuovendo la quantità della transazione appena inviata */
    sh.users_pid[user_position].balance -=
            (unsigned int) (msg.transazione.quantity * ((float) (100 - msg.transazione.reward) / 100.0));

    /* Se il segnale ricevuto è SIGUSR2, significa che questa funzione è stata chiamata inviando un segnale ad un processo utente */
    if (sig == SIGUSR2) {
        fprintf(stderr, "AO\n");
        sem_reserve(ids.sem, MMTS);
        sh.mmts[*sh.mmts_free_block] = msg.transazione;
        *sh.mmts_free_block += 1;
        sem_release(ids.sem, MMTS);
    }
    sem_release(ids.sem, (int) (FINE_SEMAFORI + user_position));
}

void start_user(unsigned int index) {
    int sig, j, k = 0;
    sigset_t wset;

    /* Seeding di rand con il index del processo */
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
    signal(SIGUSR2, transaction_generator);

    /* Ciclo principale di generazione delle transazioni */
    while (failed_transaction_user < cfg.SO_RETRY && get_stop_value() == -1) {
        sh.users_pid[user_position].status = PROCESS_RUNNING;
        sem_reserve(ids.sem, (int) (FINE_SEMAFORI + user_position));

        /* Calcolo del bilancio dell'utente: calcoliamo solo le entrate, in quanto
         * le uscite vengono registrate dopo */
        sh.users_pid[user_position].balance +=
                calc_entrate(ids.sem, sh.ledger, sh.ledger_read);

        sem_release(ids.sem, (int) (FINE_SEMAFORI + user_position));

        /* Se l'utente ha un bilancio abbastanza alto... */
        if (sh.users_pid[user_position].balance >= 2) {
            /* ...viene generata la transazione. */
            transaction_generator(0);

            /* Dato che l'utente ha inviato la transazione, il contatore delle transazioni fallite consecutive si azzera */
            failed_transaction_user = 0;
        } else {
            /* Altrimenti, significa che la transazione è fallita */
            failed_transaction_user++;
        }

        /* Simulazione del passaggio del tempo per la generazione di una transazione */
        sleeping(random() % (cfg.SO_MAX_TRANS_GEN_NSEC + 1 - cfg.SO_MIN_TRANS_GEN_NSEC) +
                 cfg.SO_MIN_TRANS_GEN_NSEC);
    }

    /* Cleanup prima di uscire */

    /* Set dello status basandoci sulla quantità di transazioni fallite */
    if (failed_transaction_user != cfg.SO_RETRY)
        sh.users_pid[user_position].status = PROCESS_FINISHED;
    else
        sh.users_pid[user_position].status = PROCESS_FINISHED_PREMATURELY;

    /* Se non c'è stato un ordine di uscire, controlliamo se gli altri processi
     * abbiano finito */
    if (get_stop_value() < 0) {
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
