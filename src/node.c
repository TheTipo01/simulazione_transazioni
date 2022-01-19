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
    int sig, i, last = 0, block_pointer, target_node;
    long tempval;
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
    check_for_update();
    sh.nodes_pid[node_position].status = PROCESS_RUNNING;
    while (get_stop_value() < 0) {
        /* Variabile per tenere conto del reward ottenuto dal nodo per ogni blocco processato*/
        block_reward = 0;

        /* Ciclo che riceve SO_BLOCK_SIZE-1 transazioni */
        do {
            if (last != cfg.SO_TP_SIZE) {
                /* Ricezione di un messaggio di tipo 1 (transazione da utente) usando coda di messaggi + error checking */
                msgrcv(get_node(node_position).msg_id, &t_rcv, msg_size(), 1, 0);
                if (errno) {
                    if (get_stop_value() != -1) {
                        goto cleanup;
                    } else {
                        fprintf(stderr, "%s:%d: PID=%5d: Error %d (%s)\n", __FILE__, __LINE__, getpid(), errno,
                                strerror(errno));
                    }
                }

                tempval = random_between_two(1, 10);
                if (tempval == 1) {
                    check_for_update();
                    target_node = (int) (random() % cfg.SO_NODES_NUM);
                    msgsnd(get_node(target_node).msg_id, &t_rcv, msg_size(), 0);
                } else {
                    /* Scrittura nella TP della transazione ricevuta e assegnazione del reward */
                    transaction_pool[last].quantity =
                            t_rcv.transazione.quantity * ((100.0 - t_rcv.transazione.reward) / 100);
                    transaction_pool[last].reward = t_rcv.transazione.reward;
                    transaction_pool[last].sender = t_rcv.transazione.sender;
                    transaction_pool[last].receiver = t_rcv.transazione.receiver;
                    transaction_pool[last].timestamp = t_rcv.transazione.timestamp;
                    block_reward += t_rcv.transazione.quantity * (t_rcv.transazione.reward / 100.0);

                    check_for_update();
                    sh.nodes_pid[node_position].balance +=
                            t_rcv.transazione.quantity * (t_rcv.transazione.reward / 100.0);
                    sh.nodes_pid[node_position].transactions++;

                    last++;
                }
            }
        } while ((last % (SO_BLOCK_SIZE - 1)) != 0 && last != cfg.SO_TP_SIZE);

        /*
         * Passo successivo: creazione del blocco avente SO_BLOCK_SIZE-1 transazioni presenti nella TP.
         * Se però la TP è piena, la transazione viene scartata: si avvisa il sender
         */
        if (last != cfg.SO_TP_SIZE) {
            /* Simulazione elaborazione del blocco */
            sleeping(random() % (cfg.SO_MAX_TRANS_PROC_NSEC + 1 - cfg.SO_MIN_TRANS_PROC_NSEC) +
                     cfg.SO_MIN_TRANS_PROC_NSEC);

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
            check_for_update();
            sh.nodes_pid[node_position].status = PROCESS_WAITING;
            msgrcv(get_node(node_position).msg_id, &t_rcv, msg_size(), 1, 0);
            if (t_rcv.hops == cfg.SO_HOPS) {
                msgsnd(ids.master_msg_id, &t_rcv, msg_size(), 0);
            } else {
                check_for_update();
                target_node = (int) (random() % cfg.SO_NODES_NUM);
                t_rcv.hops++;
                msgsnd(get_node(target_node).msg_id, &t_rcv, msg_size(), 0);
            }
        }
    }

    cleanup:
    /* Cleanup prima di uscire */
    /* Impostazione dello stato del nostro processo */
    check_for_update();
    sh.nodes_pid[node_position].status = PROCESS_FINISHED;

    /* Liberiamo la memoria allocata con malloc */
    free(transaction_pool);
}
