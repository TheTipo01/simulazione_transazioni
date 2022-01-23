#define _GNU_SOURCE

#include "config.h"
#include "enum.h"
#include "structure.h"
#include "utilities.h"
#include "node.h"
#include "rwlock.h"
#include "../vendor/libsem.h"
#include "master.h"

#include <signal.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define BLOCK_SENDER -1

unsigned int node_position;
struct Transazione *transaction_pool;

struct Transazione generate_reward(double tot_reward) {
    struct Transazione reward_tran;

    /* Generazione transazione di reward */
    reward_tran.timestamp = time(NULL);
    reward_tran.sender = BLOCK_SENDER;
    reward_tran.receiver = getpid();
    reward_tran.quantity = tot_reward;
    reward_tran.reward = 0;

    return reward_tran;
}

void start_node(unsigned int index) {
    int sig, i, last = 0, block_pointer;
    double block_reward;
    struct Messaggio t_rcv;
    struct sigaction sa;
    sigset_t wset;

    /* Seeding di rand con il tempo attuale */
    srandom(getpid());

    /* Allocazione della memoria per la transaction pool */
    transaction_pool = malloc(cfg.SO_TP_SIZE * sizeof(struct Transazione));
    node_position = index;

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

    /* Ciclo principale di ricezione e processing delle transazioni */
    sh.nodes_pid[node_position].status = PROCESS_RUNNING;
    while (get_stop_value(sh.stop, sh.stop_read) < 0 && last != cfg.SO_TP_SIZE) {
        /* Variabile per tenere conto del reward ottenuto dal nodo per ogni blocco processato*/
        block_reward = 0;

        /* Ciclo che riceve SO_BLOCK_SIZE-1 transazioni */
        do {
            /* Ricezione di un messaggio di tipo 1 (transazione da utente) usando coda di messaggi + error checking */
            msgrcv(sh.nodes_pid[node_position].msg_id, &t_rcv, msg_size(), 1, 0);
            if (errno) {
                if (get_stop_value(sh.stop, sh.stop_read) != -1) {
                    goto cleanup;
                } else {
                    fprintf(stderr, "%s:%d: PID=%5d: Error %d (%s)\n", __FILE__, __LINE__, getpid(), errno,
                            strerror(errno));
                }
            }

            /* Scrittura nella TP della transazione ricevuta e assegnazione del reward */
            transaction_pool[last].quantity = t_rcv.transazione.quantity * ((100.0 - t_rcv.transazione.reward) / 100);
            transaction_pool[last].reward = t_rcv.transazione.reward;
            transaction_pool[last].sender = t_rcv.transazione.sender;
            transaction_pool[last].receiver = t_rcv.transazione.receiver;
            transaction_pool[last].timestamp = t_rcv.transazione.timestamp;
            sh.nodes_pid[node_position].balance += t_rcv.transazione.quantity * (t_rcv.transazione.reward / 100.0);
            block_reward += t_rcv.transazione.quantity * (t_rcv.transazione.reward / 100.0);
            sh.nodes_pid[node_position].transactions++;
            last++;
        } while ((last % (SO_BLOCK_SIZE - 1)) != 0 && last != cfg.SO_TP_SIZE);

        /*
         * Passo successivo: creazione del blocco avente SO_BLOCK_SIZE-1 transazioni presenti nella TP.
         * Se però la TP è piena, la transazione viene scartata: si avvisa il sender
         */
        if (last != cfg.SO_TP_SIZE) {
            /* Simulazione elaborazione del blocco */
            sleeping(random_between_two(cfg.SO_MIN_TRANS_PROC_NSEC, cfg.SO_MAX_TRANS_PROC_NSEC));

            /* Ci riserviamo un blocco nel libro mastro aumentando il puntatore dei blocchi liberi */
            sem_reserve(ids.sem, LEDGER_WRITE);

            /* Ledger pieno: usciamo */
            if (*sh.ledger_free_block == SO_REGISTRY_SIZE) {
                sem_reserve(ids.sem, STOP_WRITE);
                *sh.stop = LEDGERFULL;
                sem_release(ids.sem, STOP_WRITE);
            } else {
                *sh.ledger_free_block += 1;
                block_pointer = *sh.ledger_free_block;

                /* Scriviamo i primi SO_BLOCK_SIZE-1 blocchi nel libro mastro */
                for (i = last - (SO_BLOCK_SIZE - 1); i < last; i++) {
                    sh.ledger[block_pointer].transazioni[i] = transaction_pool[i];
                }
                sh.ledger[block_pointer].transazioni[i + 1] = generate_reward(block_reward);
            }

            sem_release(ids.sem, LEDGER_WRITE);

            /* Notifichiamo il processo dell'arrivo di una transazione */
            kill(t_rcv.transazione.receiver, SIGUSR1);
        } else {
            /* Se la TP è piena, chiudiamo la coda di messaggi, in modo che il nodo non possa più accettare transazioni. */
            msgctl(sh.nodes_pid[node_position].msg_id, IPC_RMID, NULL);
            sh.nodes_pid[node_position].status = PROCESS_WAITING;

            sem_reserve(ids.sem, NODE_FULL);
            *sh.node_full += 1;
            if (*sh.user_waiting == cfg.SO_USERS_NUM) {
                sem_reserve(ids.sem, STOP_WRITE);
                *sh.stop = ALLTPFULL;
                sem_release(ids.sem, STOP_WRITE);
                kill(0, SIGUSR1);
            }
            sem_release(ids.sem, NODE_FULL);
        }
    }

    cleanup:
    /* Cleanup prima di uscire */
    /* Impostazione dello stato del nostro processo */
    sh.nodes_pid[node_position].status = PROCESS_FINISHED;

    /* Liberiamo la memoria allocata con malloc */
    free(transaction_pool);
}
