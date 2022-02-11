#include "master.h"
#include "config.h"
#include "enum.h"
#include "structure.h"
#include "utilities.h"
#include "node.h"
#include "rwlock.h"
#include "../vendor/libsem.h"

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

void start_node(unsigned int index, int id_friends) {
    int sig, i, j, block_pointer, target_node, *node_friends_tmp, last = 0;
    long tempval;
    unsigned int block_reward;
    struct Messaggio t_rcv;
    struct Blocco block_to_process;
    sigset_t wset;
    struct Messaggio_tp tp_remaining;

    /* Seeding di rand con il PID del processo */
    srandom(getpid());

    /* Allocazione della memoria per la transaction pool */
    transaction_pool = calloc(cfg.SO_TP_SIZE, sizeof(struct Transazione));
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

    /* Copia dei friends in memoria locale */
    node_friends_tmp = shmat(id_friends, NULL, 0);

    node_friends = calloc(cfg.SO_NUM_FRIENDS, sizeof(int));
    memcpy(node_friends, node_friends_tmp, sizeof(int) * cfg.SO_NUM_FRIENDS);

    shmdt_error_checking(node_friends_tmp);

    /* E copia dell'array nodes_pid */
    nodes_pid = calloc(cfg.SO_NODES_NUM, sizeof(struct ProcessoNode));
    memcpy(nodes_pid, sh.nodes_pid, sizeof(struct ProcessoNode) * cfg.SO_NODES_NUM);
    shmdt_error_checking(sh.nodes_pid);

    nodes_pid[node_position].pid = getpid();

    while (get_stop_value() == -1) {
        block_reward = 0;
        while (msgrcv(get_node(node_position).msg_id, &t_rcv, msg_size(), 1, IPC_NOWAIT) != -1) {
            enlarge_friends();

            /* RNG: Una transazione su dieci viene inviata a un altro nodo */
            tempval = random_between_two(1, 10);
            if (tempval == 1) {
                check_for_update(getpid());
                target_node = (int) random_between_two(0, cfg.SO_NUM_FRIENDS);
                while (get_node(node_friends[target_node]).msg_id == -1) {
                    target_node = (int) random_between_two(0, cfg.SO_NUM_FRIENDS);
                }
                msgsnd(get_node(node_friends[target_node]).msg_id, &t_rcv, msg_size(), 0);
            } else {
                /* Scrittura nella TP della transazione ricevuta e assegnazione del reward */
                transaction_pool[last].quantity =
                        t_rcv.transazione.quantity * ((100 - t_rcv.transazione.reward) / 100.0);
                transaction_pool[last].reward = t_rcv.transazione.reward;
                transaction_pool[last].sender = t_rcv.transazione.sender;
                transaction_pool[last].receiver = t_rcv.transazione.receiver;
                transaction_pool[last].timestamp = t_rcv.transazione.timestamp;
                block_reward += t_rcv.transazione.quantity * (t_rcv.transazione.reward / 100.0);

                check_for_update(getpid());
                last++;
            }

            /* TP piena: inviamo le transazioni ad altri nodi */
            if (last >= cfg.SO_TP_SIZE) {
                send_to_others(last);
            }
        }
        if (errno && errno != ENOMSG) {
            if (get_stop_value() != -1) {
                goto cleanup;
            } else {

                fprintf(stderr, "%s:%d: PID=%5d: Error %d (%s)\n", __FILE__, __LINE__, getpid(), errno,
                        strerror(errno));

            }
        }

        j = 0;
        for (i = (int) (last) - 1;
             i >= 0 && last > (SO_BLOCK_SIZE - 1);
             i--) {
            block_to_process.transazioni[j] = transaction_pool[i];
            j++;
            if (j % (SO_BLOCK_SIZE - 1) == 0) {
                block_to_process.transazioni[j + 1] = generate_reward(block_reward);
                j++;

                sem_reserve(ids.sem, LEDGER_WRITE);

                if (*sh.ledger_free_block == SO_REGISTRY_SIZE) {
                    sem_release(ids.sem, LEDGER_WRITE);

                    sem_reserve(ids.sem, STOP_WRITE);
                    *sh.stop = LEDGERFULL;
                    sem_release(ids.sem, STOP_WRITE);

                    goto cleanup;
                } else {
                    block_pointer = *sh.ledger_free_block;
                    *sh.ledger_free_block += 1;

                    /* Scriviamo i primi SO_BLOCK_SIZE-1 blocchi nel libro mastro */
                    sh.ledger[block_pointer] = block_to_process;
                    sem_release(ids.sem, LEDGER_WRITE);

                    /*
                     * Sottraiamo SO_BLOCK_SIZE - 1 al puntatore all'ultimo elemento libero della TP, in modo da "rimuovere"
                     * il blocco estratto dalla TP (si andrà a sovrascrivere sulle transazioni già scritte sul libro mastro)
                     */
                    last -= (SO_BLOCK_SIZE - 1);
                }
            }
        }
    }

    /* Cleanup prima di uscire */
    cleanup:

    tp_remaining.m_type = 1;
    tp_remaining.n = last;
    tp_remaining.pid = getpid();
    msgsnd(ids.msg_tp_remaining, &tp_remaining, sizeof(struct Messaggio_tp) - sizeof(long), 0);

    /* Liberiamo la memoria allocata con malloc */
    free(transaction_pool);
    free(node_friends);
    free(nodes_pid);
}

void enlarge_friends() {
    struct Messaggio_int msg;

    /* Lettura di tutti i messaggi ricevuti nella coda di messaggi dedicata agli amici */
    while (msgrcv(ids.msg_friends, &msg, sizeof(struct Messaggio_int) - sizeof(long), get_node(node_position).pid,
                  IPC_NOWAIT) != -1) {
        fprintf(stderr, "YOU'RE MY FRIEND NOW PID=%d. WE'RE HAVING SOFT TACOS LATER with %d :)\n", getpid(), msg.n);
        cfg.SO_NUM_FRIENDS += 1;

        /* Allarghiamo l'array degli amici */
        node_friends = realloc(node_friends, sizeof(int) * cfg.SO_NUM_FRIENDS);
        /* E aggiungiamo il nodo agli amici */
        node_friends[cfg.SO_NUM_FRIENDS - 1] = msg.n;
    }
}

void send_to_others(int last) {
    int i, target_node;
    struct Messaggio t_rcv;
    struct msqid_ds info;

    if (last >= cfg.SO_TP_SIZE) {
        msgctl(get_node(node_position).msg_id, IPC_STAT, &info);
        i = 0;
        while (msgrcv(get_node(node_position).msg_id, &t_rcv, msg_size(), 1, IPC_NOWAIT) != -1 &&
               i < info.msg_qnum) {
            check_for_update();
            target_node = node_friends[random_between_two(0, cfg.SO_NUM_FRIENDS)];
            t_rcv.hops++;
            if (t_rcv.hops >= cfg.SO_HOPS) {
                msgsnd(ids.msg_master, &t_rcv, msg_size(), 0);
            } else {
                msgsnd(get_node(target_node).msg_id, &t_rcv, msg_size(), 0);
            }
            i++;
        }
    }
}