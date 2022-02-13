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
    int sig, i, j, block_pointer, target_node, *node_friends_tmp;
    long tempval = 0;
    unsigned int block_reward;
    struct Messaggio t_rcv;
    struct Blocco block_to_process;
    sigset_t wset;

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
#ifdef trenta
    check_for_update();
#endif

    sem_reserve(ids.sem, NODES_PID_WRITE);
    sh.nodes_pid[node_position].status = PROCESS_RUNNING;
#ifdef trenta
    node_friends_tmp = shmat(sh.nodes_pid[node_position].friends, NULL, 0);
#endif
    sem_release(ids.sem, NODES_PID_WRITE);

#ifdef trenta
    node_friends = malloc(sizeof(int) * cfg.SO_NUM_FRIENDS);
    memcpy(node_friends, node_friends_tmp, sizeof(int) * cfg.SO_NUM_FRIENDS);


    shmdt_error_checking(node_friends_tmp);
    shmctl(sh.nodes_pid[node_position].friends, IPC_RMID, NULL);
#endif

    while (get_stop_value() == -1) {
        block_reward = 0;
        while (msgrcv(get_node(node_position).msg_id, &t_rcv, msg_size(), 1, IPC_NOWAIT) != -1) {

#ifdef trenta
            /* Controlliamo la presenza di friends nuovi */
            enlarge_friends();

            /* RNG: Una transazione su dieci viene inviata a un altro nodo */
            tempval = random_between_two(1, 10);
#endif
            if (tempval == 1) {
#ifdef trenta
                check_for_update();
#endif
                target_node = (int) random_between_two(0, cfg.SO_NUM_FRIENDS);
                while (get_node(node_friends[target_node]).msg_id == -1) {
                    target_node = (int) random_between_two(0, cfg.SO_NUM_FRIENDS);
                }
                msgsnd(get_node(node_friends[target_node]).msg_id, &t_rcv, msg_size(), 0);
            } else {
                /* Scrittura nella TP della transazione ricevuta e assegnazione del reward */
                transaction_pool[get_node(node_position).last].quantity =
                        t_rcv.transazione.quantity * ((100 - t_rcv.transazione.reward) / 100.0);
                transaction_pool[get_node(node_position).last].reward = t_rcv.transazione.reward;
                transaction_pool[get_node(node_position).last].sender = t_rcv.transazione.sender;
                transaction_pool[get_node(node_position).last].receiver = t_rcv.transazione.receiver;
                transaction_pool[get_node(node_position).last].timestamp = t_rcv.transazione.timestamp;
                block_reward += t_rcv.transazione.quantity * (t_rcv.transazione.reward / 100.0);

                /* Aggiornamento del bilancio del nodo tenendo conto del reward */
#ifdef trenta
                check_for_update();
#endif
                sem_reserve(ids.sem, NODES_PID_WRITE);
                sh.nodes_pid[node_position].balance +=
                        t_rcv.transazione.quantity * (t_rcv.transazione.reward / 100.0);

                sh.nodes_pid[node_position].last++;
                sem_release(ids.sem, NODES_PID_WRITE);
            }

            /* TP piena: inviamo le transazioni indietro per rifiutarle */
            if (get_node(node_position).last >= cfg.SO_TP_SIZE) {
                /* TP piena: inviamo le transazioni ad altri nodi */
#ifdef trenta
                send_to_others();
#endif
                t_rcv.m_type = t_rcv.transazione.sender;
                msgsnd(ids.user_msg_id, &t_rcv, msg_size(), 0);
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

        /* Ciclo per scrivere i blocchi sul libro mastro */
        j = 0;
        for (i = (int) (get_node(node_position).last) - 1;
             i >= 0 && get_node(node_position).last > (SO_BLOCK_SIZE - 1);
             i--) {
            /* Utilizzo di una struttura d'appoggio per il blocco da inserire */
            block_to_process.transazioni[j] = transaction_pool[i];
            j++;
            if (j % (SO_BLOCK_SIZE - 1) == 0) {
                /* Creazione dell'ultima transazione del blocco con il reward per il nodo */
                block_to_process.transazioni[j + 1] = generate_reward(block_reward);
                j++;

                /* Scrittura sul libro mastro protetta da semaforo */
                sem_reserve(ids.sem, LEDGER_WRITE);

                /* Controllo per il caso del libro mastro pieno */
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

                    sem_reserve(ids.sem, NODES_PID_WRITE);
                    /*
                     * Sottraiamo SO_BLOCK_SIZE - 1 al puntatore all'ultimo elemento libero della TP, in modo da "rimuovere"
                     * il blocco estratto dalla TP (si andrà a sovrascrivere sulle transazioni già scritte sul libro mastro)
                     */
                    sh.nodes_pid[node_position].last -= (SO_BLOCK_SIZE - 1);
                    sem_release(ids.sem, NODES_PID_WRITE);
                }
            }
        }
    }

    /* Cleanup prima di uscire */
    cleanup:
    /* Impostazione dello stato del nostro processo */
#ifdef trenta
    check_for_update();
#endif
    sem_reserve(ids.sem, NODES_PID_WRITE);
    sh.nodes_pid[node_position].status = PROCESS_FINISHED;
    sem_release(ids.sem, NODES_PID_WRITE);

    /* Liberiamo la memoria allocata con malloc */
    free(transaction_pool);
    free(node_friends);
}

#ifdef trenta
void enlarge_friends() {
    struct Messaggio_int msg;

    /* Lettura di tutti i messaggi ricevuti nella coda di messaggi dedicata agli amici */
    while (msgrcv(ids.msg_friends, &msg, sizeof(struct Messaggio_int) - sizeof(long), get_node(node_position).pid,
                  IPC_NOWAIT) != -1) {
        cfg.SO_NUM_FRIENDS += 1;

        /* Allarghiamo l'array degli amici */
        node_friends = realloc(node_friends, sizeof(int) * cfg.SO_NUM_FRIENDS);
        /* E aggiungiamo il nodo agli amici */
        node_friends[cfg.SO_NUM_FRIENDS - 1] = msg.n;
    }
}

void send_to_others() {
    int msg_id, i, target_node;
    struct Messaggio t_rcv;
    struct msqid_ds info;

    sem_reserve(ids.sem, NODES_PID_WRITE);

    msg_id = sh.nodes_pid[node_position].msg_id;
    sh.nodes_pid[node_position].msg_id = -1;

    sem_release(ids.sem, NODES_PID_WRITE);

    msgctl(msg_id, IPC_STAT, &info);

    i = 0;
    while (msgrcv(msg_id, &t_rcv, msg_size(), 1, IPC_NOWAIT) != -1 &&
           i < info.msg_qnum) {
        check:
        enlarge_friends();
        check_for_update();

        target_node = node_friends[random_between_two(0, cfg.SO_NUM_FRIENDS)];
        t_rcv.hops++;

        while (get_node(target_node).msg_id == -1 && t_rcv.hops <= cfg.SO_HOPS) {
            target_node = node_friends[random_between_two(0, cfg.SO_NUM_FRIENDS)];
            t_rcv.hops++;
        }

        if (t_rcv.hops > cfg.SO_HOPS) {
            msgsnd(ids.master_msg_id, &t_rcv, msg_size(), IPC_NOWAIT);
            TEST_ERROR;
        } else {
            msgsnd(get_node(target_node).msg_id, &t_rcv, msg_size(), IPC_NOWAIT);
            if (errno == 11) {
                goto check;
            }
            TEST_ERROR;
        }
        i++;
    }

    sem_reserve(ids.sem, NODES_PID_WRITE);

    sh.nodes_pid[node_position].msg_id = msg_id;

    sem_release(ids.sem, NODES_PID_WRITE);
}
#endif