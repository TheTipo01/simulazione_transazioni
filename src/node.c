#define _GNU_SOURCE

#include "config.h"
#include "enum.h"
#include "structure.h"
#include "utilities.h"
#include "node.h"
#include "rwlock.h"

#include <signal.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define BLOCK_SENDER -1

unsigned int nodePosition;
struct Transazione *transactionPool;

void endNode(int signum){
    switch(signum){
        case SIGUSR2:
            sh.nodePIDs[nodePosition].status = PROCESS_FINISHED;
            free(transactionPool);
            fprintf(stderr, "Node #%d terminated.", getpid());
            exit(EXIT_SUCCESS);
    }
}

struct Transazione generateReward(double tot_reward) {
    struct Transazione reward_tran;

    reward_tran.timestamp = time(NULL);
    reward_tran.sender = BLOCK_SENDER;
    reward_tran.receiver = getpid();
    reward_tran.quantity = tot_reward;
    reward_tran.reward = 0;

    return reward_tran;
}

void startNode(unsigned int index) {
    int sig, i, last = 0, blockPointer;
    double blockReward;
    struct Messaggio tRcv;
    struct sigaction sa;
    sigset_t wset;

    /* Seeding di rand con il tempo attuale */
    srandom(getpid());

    transactionPool = malloc(cfg.SO_TP_SIZE * sizeof(struct Transazione));
    nodePosition = index;

    sa.sa_handler = endNode;
    sigaction(SIGUSR2, &sa, NULL);

    /* Creiamo un set in cui mettiamo il segnale che usiamo per far aspettare i processi */
    sigemptyset(&wset);
    sigaddset(&wset, SIGUSR1);

    /*
     * Child aspettano un segnale dal parent: possono iniziare la loro funzione solo dopo che vengono generati
     * tutti gli altri child
     */
    sigwait(&wset, &sig);

    /* Mascheriamo i segnali che usiamo */
    sigprocmask(SIG_BLOCK, &wset, NULL);

    sh.nodePIDs[nodePosition].status = PROCESS_RUNNING;
    while (get_stop_value(sh.stop, sh.stopRead) < 0 && last != cfg.SO_TP_SIZE) {
        /* Riceve SO_TP_SIZE transazioni  */
        blockReward = 0;
        do {
            msgrcv(sh.nodePIDs[nodePosition].msgID, &tRcv, msg_size(), 1, 0);
            TEST_ERROR;

            transactionPool[last].quantity = tRcv.transazione.quantity * ((100.0 - tRcv.transazione.reward) / 100);
            transactionPool[last].reward = tRcv.transazione.reward;
            transactionPool[last].sender = tRcv.transazione.sender;
            transactionPool[last].receiver = tRcv.transazione.receiver;
            transactionPool[last].timestamp = tRcv.transazione.timestamp;
            sh.nodePIDs[nodePosition].balance += tRcv.transazione.quantity * (tRcv.transazione.reward / 100.0);
            blockReward += tRcv.transazione.quantity * (tRcv.transazione.reward / 100.0);
            sh.nodePIDs[nodePosition].transactions++;
            last++;
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
            if (*sh.freeBlock == SO_REGISTRY_SIZE) {
                sem_reserve(ids.sem, STOP_WRITE);
                *sh.stop = LEDGERFULL;
                sem_release(ids.sem, STOP_WRITE);
            } else {
                *sh.freeBlock += 1;
                blockPointer = *sh.freeBlock;

                /* Scriviamo i primi SO_BLOCK_SIZE-1 blocchi nel libro mastro */
                for (i = last - (SO_BLOCK_SIZE - 1); i < last; i++) {
                    sh.ledger[blockPointer].transazioni[i] = transactionPool[i];
                }
                sh.ledger[blockPointer].transazioni[i + 1] = generateReward(blockReward);
            }

            sem_release(ids.sem, LEDGER_WRITE);

            /* Notifichiamo il processo dell'arrivo di una transazione */
            kill(tRcv.transazione.receiver, SIGUSR1);
        } else {
            /* Se la TP è piena, chiudiamo la coda di messaggi, in modo che il nodo non possa più accettare transazioni. */
            msgctl(sh.nodePIDs[nodePosition].msgID, IPC_RMID, NULL);
            sh.nodePIDs[nodePosition].status = PROCESS_WAITING;
        }
    }

    /* Cleanup prima di uscire */

    /* Impostazione dello stato del nostro processo */
    sh.nodePIDs[nodePosition].status = PROCESS_FINISHED;

    /* Liberiamo la memoria allocata con malloc */
    free(transactionPool);
}
