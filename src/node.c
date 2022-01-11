#define _GNU_SOURCE

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

#define BLOCK_SENDER -1

struct Transazione generateReward(int pos, struct Transazione *tp, int totrw) {
    struct Transazione rewtran;

    rewtran.timestamp = time(NULL);
    rewtran.sender = BLOCK_SENDER;
    rewtran.receiver = getpid();
    rewtran.quantity = totrw;
    rewtran.reward = 0;

    return rewtran;
}

void startNode(unsigned int nodePosition) {
    struct Transazione *transactionPool = malloc(cfg.SO_TP_SIZE * sizeof(struct Transazione));
    int sig, i = 0, last = 0, blockPointer, blockReward;
    struct Messaggio tRcv;
    sigset_t wset;
    long num_bytes, wt = 0;

    setvbuf(stdout, NULL, _IONBF, 0);

    /* Seeding di rand con il tempo attuale */
    srandom(getpid());

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
    sh.nodePIDs[nodePosition].status = PROCESS_RUNNING;
    while (*sh.stop < 0 && last != cfg.SO_TP_SIZE) {
        /* Riceve SO_TP_SIZE transazioni  */
        blockReward = 0;
        do {
            num_bytes = msgrcv(sh.nodePIDs[nodePosition].msgID, &tRcv, msg_size(), 1, 0);
            TEST_ERROR;
            fprintf(stdout, "Received message from %d. Byte %ld\n", tRcv.transazione.sender, num_bytes);

            transactionPool[last].quantity = tRcv.transazione.quantity * ((100 - tRcv.transazione.reward) / 100);
            transactionPool[last].reward = tRcv.transazione.reward;
            transactionPool[last].sender = tRcv.transazione.sender;
            transactionPool[last].receiver = tRcv.transazione.receiver;
            transactionPool[last].timestamp = tRcv.transazione.timestamp;
            sh.nodePIDs[nodePosition].balance += tRcv.transazione.quantity * (tRcv.transazione.reward / 100);
            blockReward += tRcv.transazione.quantity * (tRcv.transazione.reward / 100);
            last++;
        } while ((last % (SO_BLOCK_SIZE - 1)) != 0 && last != cfg.SO_TP_SIZE);

        fprintf(stdout, "fuck this shit i'm out; last=%d blockreward=%d\n", last, blockReward);

        /*
         * Passo successivo: creazione del blocco avente SO_BLOCK_SIZE-1 transazioni presenti nella TP.
         * Se però la TP è piena, la transazione viene scartata: si avvisa il sender
         */
        if (last != cfg.SO_TP_SIZE) {
            /* Simulazione elaborazione del blocco */
            wt = random() % (cfg.SO_MAX_TRANS_PROC_NSEC + 1 - cfg.SO_MIN_TRANS_PROC_NSEC) +
                 cfg.SO_MIN_TRANS_PROC_NSEC;
            sleeping(wt);

            /* Ci riserviamo un blocco nel libro mastro aumentando il puntatore dei blocchi liberi */
            sem_reserve(ids.sem, LEDGER_WRITE);

            /* Ledger pieno: usciamo */
            if (*sh.freeBlock == SO_REGISTRY_SIZE) {
                *sh.stop = LEDGERFULL;
            } else {
                *sh.freeBlock++;
                blockPointer = *sh.freeBlock;

                /* Scriviamo i primi SO_BLOCK_SIZE-1 blocchi nel libro mastro */
                for (i = last - (SO_BLOCK_SIZE - 1); i < last; i++) {
                    sh.ledger[blockPointer].transazioni[i] = transactionPool[i];
                }
                sh.ledger[blockPointer].transazioni[i + 1] = generateReward(i + 1, transactionPool, blockReward);
            }

            sem_release(ids.sem, LEDGER_WRITE);

            /* Notifichiamo il processo dell'arrivo di una transazione */
            kill(tRcv.transazione.receiver, SIGUSR1);
        } else {
            /* Se la TP è piena, chiudiamo la coda di messaggi, in modo che il nodo non possa più accettare transazioni. */
            msgctl(sh.nodePIDs[nodePosition].msgID, IPC_RMID, NULL);
        }
    }

    /* Cleanup prima di uscire */

    /* Impostazione dello stato del nostro processo */
    sh.nodePIDs[nodePosition].status = PROCESS_FINISHED;

    /* Detach di tutte le shared memory */
    shmdt_error_checking(sh.nodePIDs);
    shmdt_error_checking(sh.usersPIDs);
    shmdt_error_checking(sh.ledger);
    shmdt_error_checking(sh.stop);

    /* E liberiamo la memoria allocata con malloc */
    free(transactionPool);
}
