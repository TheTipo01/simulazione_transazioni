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
#include <sys/shm.h>

#define BLOCK_SENDER -1

int *node_friends;
unsigned int node_position;
struct Transazione *transaction_pool;

struct Transazione generate_reward(unsigned int tot_reward) {
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
    int sig, i, block_pointer, target_node, *node_friends_tmp, msg_id = 0;
    long tempval;
    unsigned int block_reward;
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

    signal(SIGALRM, enlarge_friends);

    /* Ciclo principale di ricezione e processing delle transazioni */
    check_for_update();
    sem_reserve(ids.sem, NODES_PID_WRITE);
    sh.nodes_pid[node_position].status = PROCESS_RUNNING;
    node_friends_tmp = shmat(sh.nodes_pid[node_position].friends, NULL, 0);
    sem_release(ids.sem, NODES_PID_WRITE);

    node_friends = malloc(sizeof(int) * cfg.SO_NUM_FRIENDS);
    memcpy(node_friends, node_friends_tmp, sizeof(int) * cfg.SO_NUM_FRIENDS);

    shmdt_error_checking(node_friends_tmp);
    shmctl(sh.nodes_pid[node_position].friends, IPC_RMID, NULL);


    while (get_stop_value() == -1) {
        /* Variabile per tenere conto del reward ottenuto dal nodo per ogni blocco processato*/
        block_reward = 0;

        /* Ciclo che riceve SO_BLOCK_SIZE-1 transazioni */
        do {
            if (get_node(node_position).last < cfg.SO_TP_SIZE) {
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
                    target_node = (int) random_between_two(0, cfg.SO_NUM_FRIENDS);
                    msg_id = get_node(node_friends[target_node]).msg_id;
                    msgsnd(msg_id, &t_rcv, msg_size(), 0);
                } else {
                    /* Scrittura nella TP della transazione ricevuta e assegnazione del reward */
                    transaction_pool[get_node(node_position).last].quantity =
                            t_rcv.transazione.quantity * ((100 - t_rcv.transazione.reward) / 100.0);
                    transaction_pool[get_node(node_position).last].reward = t_rcv.transazione.reward;
                    transaction_pool[get_node(node_position).last].sender = t_rcv.transazione.sender;
                    transaction_pool[get_node(node_position).last].receiver = t_rcv.transazione.receiver;
                    transaction_pool[get_node(node_position).last].timestamp = t_rcv.transazione.timestamp;
                    block_reward += t_rcv.transazione.quantity * (t_rcv.transazione.reward / 100.0);

                    check_for_update();
                    sem_reserve(ids.sem, NODES_PID_WRITE);
                    sh.nodes_pid[node_position].balance +=
                            t_rcv.transazione.quantity * (t_rcv.transazione.reward / 100.0);

                    sh.nodes_pid[node_position].last++;
                    sem_release(ids.sem, NODES_PID_WRITE);
                }
            }
            /*fprintf(stderr, "last in=%d\n", get_node(node_position).last);*/
        } while (((get_node(node_position).last % (SO_BLOCK_SIZE - 1)) != 0 &&
                  get_node(node_position).last < cfg.SO_TP_SIZE) ||
                 get_node(node_position).last == 0);

        /*
         * Passo successivo: creazione del blocco avente SO_BLOCK_SIZE-1 transazioni presenti nella TP.
         * Se però la TP è piena, la transazione viene scartata: si avvisa il sender
         */
        if (get_node(node_position).last < cfg.SO_TP_SIZE) {
            /* Simulazione elaborazione del blocco */
            sleeping(random() % (cfg.SO_MAX_TRANS_PROC_NSEC + 1 - cfg.SO_MIN_TRANS_PROC_NSEC) +
                     cfg.SO_MIN_TRANS_PROC_NSEC);

            /* Ci riserviamo un blocco nel libro mastro aumentando il puntatore dei blocchi liberi */
            sem_reserve(ids.sem, LEDGER_WRITE);

            /* Ledger pieno: usciamo */
            if (*sh.ledger_free_block == SO_REGISTRY_SIZE) {
                sem_release(ids.sem, LEDGER_WRITE);

                sem_reserve(ids.sem, STOP_WRITE);
                *sh.stop = LEDGERFULL;
                sem_release(ids.sem, STOP_WRITE);
            } else {
                *sh.ledger_free_block += 1;
                block_pointer = *sh.ledger_free_block;

                /* Scriviamo i primi SO_BLOCK_SIZE-1 blocchi nel libro mastro */
                for (i = (int) (get_node(node_position).last - (SO_BLOCK_SIZE - 1));
                     i < get_node(node_position).last; i++) {
                    sh.ledger[block_pointer].transazioni[i] = transaction_pool[i];
                }
                sh.ledger[block_pointer].transazioni[i + 1] = generate_reward(block_reward);
                sem_release(ids.sem, LEDGER_WRITE);


                /*
                 * Sottraiamo SO_BLOCK_SIZE - 1 al puntatore all'ultimo elemento libero della TP, in modo da "rimuovere"
                 * il blocco estratto dalla TP (si andrà a sovrascrivere sulle transazioni già scritte sul libro mastro)
                 */
                sem_reserve(ids.sem, NODES_PID_WRITE);
                sh.nodes_pid[node_position].last -= (SO_BLOCK_SIZE - 1);
                sem_release(ids.sem, NODES_PID_WRITE);
            }

        } else {
            fprintf(stderr, "oh no our node it's broken\n");
            check_for_update();

            /* Ogni nuova transazione verrà inoltrata ad altri nodi scelti casualmente */
            msgrcv(get_node(node_position).msg_id, &t_rcv, msg_size(), 1, 0);
            if (errno) {
                if (get_stop_value() != -1) {
                    goto cleanup;
                } else {
                    fprintf(stderr, "%s:%d: PID=%5d: Error %d (%s)\n", __FILE__, __LINE__, getpid(), errno,
                            strerror(errno));
                }
            }

            if (t_rcv.hops == cfg.SO_HOPS) {
                msgsnd(ids.master_msg_id, &t_rcv, msg_size(), 0);
            } else {
                check_for_update();
                target_node = node_friends[random_between_two(0, cfg.SO_NUM_FRIENDS)];
                t_rcv.hops++;
                msgsnd(get_node(target_node).msg_id, &t_rcv, msg_size(), 0);
            }
        }
    }

    /* Cleanup prima di uscire */
    cleanup:
    /* Impostazione dello stato del nostro processo */
    check_for_update();
    sem_reserve(ids.sem, NODES_PID_WRITE);
    sh.nodes_pid[node_position].status = PROCESS_FINISHED;
    sem_release(ids.sem, NODES_PID_WRITE);

    /* Liberiamo la memoria allocata con malloc */
    free(transaction_pool);
    free(node_friends);
}

void enlarge_friends(int sig) {
    struct Messaggio_PID msg;
    if (sig == SIGALRM) {
        msgrcv(get_node(node_position).msg_id, &msg, sizeof(int), 2, 0);

        cfg.SO_NUM_FRIENDS += 1;

        sem_reserve(ids.sem, NODES_PID_WRITE);
        node_friends = realloc(node_friends, sizeof(int) * cfg.SO_NUM_FRIENDS);
        node_friends[cfg.SO_NUM_FRIENDS - 1] = msg.index;
        sem_release(ids.sem, NODES_PID_WRITE);
    }
}