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

void startNode(Config cfg, struct SharedMemoryID ids, unsigned int position) {
    struct Transazione **transactionPool = malloc(cfg.SO_TP_SIZE * sizeof(struct Transazione));
    int sig, i = 0, j, k, last = 0, qID, sID, *stop;
    unsigned int sum, *readerCounter;
    struct timespec *cur_time, wait_time;
    struct Transazione tmp;
    struct Transazione **libroMastro;
    Processo *nodePIDs, *usersPIDs;
    struct Messaggio msg;
    sigset_t wset;

    /* Collegamento all'array dello stato dei processi utente */
    usersPIDs = shmat(ids.usersPIDs, NULL, 0);

    /* Collegamento all'array dello stato dei processi nodo */
    nodePIDs = shmat(ids.nodePIDs, NULL, 0);

    /* Creo la coda con chiave PID del nodo */
    qID = msgget(getpid(), IPC_CREAT);

    /* Booleano abbassato quando dobbiamo terminare i processi, per gracefully terminare tutto */
    stop = shmat(ids.stop, NULL, 0);

    /* Ci attacchiamo al libro mastro */
    libroMastro = shmat(ids.ledger, NULL, 0);

    readerCounter = shmat(ids.readCounter, NULL, 0);

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

    while (*stop < 0) {
        while (transactionPool[i + SO_BLOCK_SIZE - 2] == NULL && last != cfg.SO_TP_SIZE) {
            msgrcv(qID, &tmp, sizeof(struct Transazione), 0, 0);
            transactionPool[last]->quantity = tmp.quantity * ((100 - tmp.reward) / 100);
            transactionPool[last]->reward = tmp.reward;
            transactionPool[last]->sender = tmp.sender;
            transactionPool[last]->receiver = tmp.receiver;
            transactionPool[last]->timestamp = tmp.timestamp;
            nodePIDs[position].balance += tmp.quantity * (tmp.reward / 100);
            last++;
        }

        sID = msgget(tmp.sender, IPC_CREAT);

        /*
         * Passo successivo: creazione del blocco avente SO_BLOCK_SIZE-1 transazioni presenti nella TP.
         * Se però la TP è piena, la transazione viene scartata: si avvisa il sender
         */
        if (last != cfg.SO_TP_SIZE) {
            /* Transazione accettata: inviamo la transazione indietro (= successo) */
            /* TODO: maybe smettere di leakare memoria, idk (però solo quando funziona tutto™) */

            msg.m_type = 0;
            msg.transazione = tmp;
            msgsnd(sID, &msg, sizeof(struct Transazione), IPC_NOWAIT);
            /* free(msg); */

            /* Popolazione blocco */
            clock_gettime(CLOCK_REALTIME, cur_time);
            transactionPool[SO_BLOCK_SIZE - 1]->timestamp = *cur_time;
            transactionPool[SO_BLOCK_SIZE - 1]->sender = BLOCK_SENDER;
            transactionPool[SO_BLOCK_SIZE - 1]->receiver = getpid();
            transactionPool[SO_BLOCK_SIZE - 1]->reward = 0;
            for (i = 0; i < SO_BLOCK_SIZE - 1; i++) {
                sum += transactionPool[i]->quantity;
            }
            transactionPool[SO_BLOCK_SIZE - 1]->quantity = sum;

            /* Simulazione elaborazione del blocco */
            wait_time.tv_nsec =
                    rand() % (cfg.SO_MAX_TRANS_PROC_NSEC + 1 - cfg.SO_MIN_TRANS_PROC_NSEC) + cfg.SO_MIN_TRANS_PROC_NSEC;
            nanosleep(&wait_time, NULL);

            /* Scrittura in libro mastro con sincronizzazione (semaforo) */
            /* Can we have a non-blockin */
            sem_reserve(ids.sem, LEDGER_WRITE);
            for (k = 0; k < SO_REGISTRY_SIZE; k++) {
                if (libroMastro[k] == NULL) {
                    libroMastro[k] = *transactionPool;
                }
            }
            if (libroMastro[SO_REGISTRY_SIZE] != NULL) *stop = LEDGERFULL;
            sem_release(ids.sem, LEDGER_WRITE);

            /* Notifichiamo il processo dell'arrivo di una transazione */
            kill(tmp.receiver, SIGUSR1);
        } else {
            /* Notifichiamo l'utente, rinviandogli la transazione con uno dei campi non valido () */
            msg.m_type = 0;
            msg.transazione.sender = BLOCK_SENDER;
            msgsnd(sID, &msg, sizeof(struct Transazione), IPC_NOWAIT);
        }
    }

    /* Cleanup prima di uscire */

    /* Impostazione dello stato del nostro processo */
    nodePIDs[position].status = PROCESS_FINISHED;

    /* Detach di tutte le shared memory */
    shmdt_error_checking(&nodePIDs);
    shmdt_error_checking(&usersPIDs);
    shmdt_error_checking(&libroMastro);
    shmdt_error_checking(&stop);

    /* Chiusura delle code di messaggi */
    msgctl(qID, IPC_RMID, NULL);
}
