#include "config.h"
#include "structure.h"
#include "../vendor/libsem.h"
#include "enum.c"
#include "time.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define BLOCK_SENDER -1

struct Transazione generateReward(int pos, struct Transazione *tp, int totrw) {
    struct Transazione rewtran;
    struct timespec *curtime;

    clock_gettime(CLOCK_REALTIME, curtime);
    rewtran.timestamp = *curtime;
    rewtran.sender = BLOCK_SENDER;
    rewtran.receiver = getpid();
    rewtran.quantity = totrw;
    rewtran.reward = 0;

    return rewtran;
}

void startNode(Config cfg, struct SharedMemoryID ids, unsigned int position) {
    struct Transazione *transactionPool = malloc(cfg.SO_TP_SIZE * sizeof(struct Transazione));
    int sig, i = 0, last = 0, blockPointer, blockReward;
    struct Messaggio tRcv;
    struct Messaggio msg;
    struct SharedMemory sh;
    sigset_t wset;

    /* Seeding di rand con il tempo attuale */
    srand(getpid());

    /* Collegamento all'array dello stato dei processi utente */
    sh.usersPIDs = shmat(ids.usersPIDs, NULL, 0);
    TEST_ERROR;

    /* Collegamento all'array dello stato dei processi nodo */
    sh.nodePIDs = shmat(ids.nodePIDs, NULL, 0);
    TEST_ERROR;

    /* Booleano abbassato quando dobbiamo terminare i processi, per gracefully terminare tutto */
    sh.stop = shmat(ids.stop, NULL, 0);
    TEST_ERROR;

    /* Ci attacchiamo al libro mastro */
    sh.libroMastro = shmat(ids.ledger, NULL, 0);
    TEST_ERROR;

    sh.readerCounter = shmat(ids.readCounter, NULL, 0);
    TEST_ERROR;

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
    sh.nodePIDs[position].status = PROCESS_RUNNING;

    while (*sh.stop < 0) {
        /* Riceve SO_TP_SIZE transazioni  */
        blockReward = 0;
        while (last % SO_BLOCK_SIZE - 1 != 0 && last != cfg.SO_TP_SIZE) {
            msgrcv(sh.nodePIDs[position].msgID, &tRcv, sizeof(struct Messaggio), 0, 0);
            fprintf(stdout, "Received message from %d", tRcv.transazione.sender);
            transactionPool[last].quantity = tRcv.transazione.quantity * ((100 - tRcv.transazione.reward) / 100);
            transactionPool[last].reward = tRcv.transazione.reward;
            transactionPool[last].sender = tRcv.transazione.sender;
            transactionPool[last].receiver = tRcv.transazione.receiver;
            transactionPool[last].timestamp = tRcv.transazione.timestamp;
            sh.nodePIDs[position].balance += tRcv.transazione.quantity * (tRcv.transazione.reward / 100);
            blockReward += tRcv.transazione.quantity * (tRcv.transazione.reward / 100);
            last++;
        }

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
            if (sh.libroMastro->freeBlock == SO_REGISTRY_SIZE) {
                *sh.stop = LEDGERFULL;
            } else {
                sh.libroMastro->freeBlock++;
                blockPointer = sh.libroMastro->freeBlock;

                /* Scriviamo i primi SO_BLOCK_SIZE-1 blocchi nel libro mastro */
                for (i = last - (SO_BLOCK_SIZE - 1); i < last; i++) {
                    sh.libroMastro[blockPointer].transazioni[i] = transactionPool[i];
                }
                sh.libroMastro[blockPointer].transazioni[i + 1] = generateReward(i + 1, transactionPool, blockReward);
            }

            sem_release(ids.sem, LEDGER_WRITE);

            /* Notifichiamo il processo dell'arrivo di una transazione */
            kill(tRcv.transazione.receiver, SIGUSR1);
        } else {
            /* Se la TP è piena, chiudiamo la coda di messaggi, in modo che il nodo non possa più accettare transazioni. */
            msgctl(sh.nodePIDs[position].msgID, IPC_RMID, NULL);
        }
    }

    /* Cleanup prima di uscire */

    /* Impostazione dello stato del nostro processo */
    sh.nodePIDs[position].status = PROCESS_FINISHED;

    /* Detach di tutte le shared memory */
    shmdt_error_checking(&sh.nodePIDs);
    shmdt_error_checking(&sh.usersPIDs);
    shmdt_error_checking(&sh.libroMastro);
    shmdt_error_checking(&sh.stop);

    /* E liberiamo la memoria allocata con malloc */
    free(transactionPool);
}