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
    int sig, i, j, block_pointer, target_node, *node_friends_tmp, msg_id = 0;
    long tempval;
    unsigned int block_reward;
    struct Messaggio t_rcv;
    struct sigaction sa;
    struct Blocco block_to_process;
    struct msqid_ds info;
    sigset_t wset;

    /* Seeding di rand con il PID del processo */
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
    if (node_position > 9) fprintf(stderr, "aspettando segnale\n");
    sigwait(&wset, &sig);
    if (node_position > 9) fprintf(stderr, "avviato\n");

    /* Mascheriamo i segnali che usiamo */
    sigprocmask(SIG_SETMASK, &wset, NULL);

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
        enlarge_friends();

        block_reward = 0;
        while (msgrcv(get_node(node_position).msg_id, &t_rcv, msg_size(), 1, IPC_NOWAIT) != -1 &&
               get_node(node_position).last < cfg.SO_TP_SIZE) {

            /* RNG: Una transazione su dieci viene inviata a un altro nodo */
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
        if (errno && errno != ENOMSG) {
            if (get_stop_value() != -1) {
                goto cleanup;
            } else {
                fprintf(stderr, "%s:%d: PID=%5d: Error %d (%s)\n", __FILE__, __LINE__, getpid(), errno,
                        strerror(errno));
            }
        }

        if (node_position == 10) fprintf(stderr, "last = %u\n", sh.nodes_pid[node_position].last);

        /* TP piena: inviamo le transazioni ad altri nodi */
        if (get_node(node_position).last >= cfg.SO_TP_SIZE) {
            msgctl(get_node(node_position).msg_id, IPC_STAT, &info);
            i = 0;
            while (msgrcv(get_node(node_position).msg_id, &t_rcv, msg_size(), 1, IPC_NOWAIT) != -1 &&
                   i < info.msg_qnum) {
                check_for_update();
                target_node = node_friends[random_between_two(0, cfg.SO_NUM_FRIENDS)];
                t_rcv.hops++;
                if (t_rcv.hops >= cfg.SO_HOPS) {
                    fprintf(stderr, "good\n");
                    msgsnd(ids.master_msg_id, &t_rcv, msg_size(), 0);
                } else {
                    msgsnd(get_node(target_node).msg_id, &t_rcv, msg_size(), 0);
                }
                i++;
            }
            fprintf(stderr, "msg queue empty for node #%d\n", getpid());
            if (errno && errno != ENOMSG) {
                if (get_stop_value() != -1) {
                    goto cleanup;
                } else {
                    fprintf(stderr, "%s:%d: PID=%5d: Error %d (%s)\n", __FILE__, __LINE__, getpid(), errno,
                            strerror(errno));
                }
            }
        }

        /*
        fprintf(stderr, "finito\n");
         */

        j = 0;
        for (i = get_node(node_position).last - 1; i >= 0 && get_node(node_position).last > (SO_BLOCK_SIZE - 1); i--) {
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
                } else {
                    *sh.ledger_free_block += 1;
                    block_pointer = *sh.ledger_free_block;

                    /* Scriviamo i primi SO_BLOCK_SIZE-1 blocchi nel libro mastro */
                    sh.ledger[block_pointer] = block_to_process;
                    sem_release(ids.sem, LEDGER_WRITE);

                    sem_reserve(ids.sem, NODES_PID_WRITE);
                    /* Sottraiamo SO_BLOCK_SIZE - 1 al puntatore all'ultimo elemento libero della TP, in modo da "rimuovere"
                     * il blocco estratto dalla TP (si andrà a sovrascrivere sulle transazioni già scritte sul libro mastro)*/
                    sh.nodes_pid[node_position].last -= (SO_BLOCK_SIZE - 1);
                    sem_release(ids.sem, NODES_PID_WRITE);
                }
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

void enlarge_friends() {
    struct Messaggio_PID msg;
    if (msgrcv(ids.master_msg_id, &msg, sizeof(int), get_node(node_position).pid, IPC_NOWAIT) != -1) {
        fprintf(stderr, "YOU'RE MY FRIEND NOW PID=%d. WE'RE HAVING SOFT TACOS LATER with %d :)\n", getpid(), msg.index);

        cfg.SO_NUM_FRIENDS += 1;

        sem_reserve(ids.sem, NODES_PID_WRITE);
        node_friends = realloc(node_friends, sizeof(int) * cfg.SO_NUM_FRIENDS);
        node_friends[cfg.SO_NUM_FRIENDS - 1] = msg.index;
        sem_release(ids.sem, NODES_PID_WRITE);
    }
}