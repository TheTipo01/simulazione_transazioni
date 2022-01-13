#define _GNU_SOURCE
#ifndef STRUCTURE_H
#define STRUCTURE_H

#include "time.h"

struct Transazione {
    /* Timestamp della transazione con risoluzione dei nanosecondi
        (vedere funzione clock_gettime()) */
    time_t timestamp;

    /* L'utente implicito che ha generato la transazione */
    pid_t sender;

    /* L'utente destinatario della somma di denaro */
    pid_t receiver;

    /* Quantità di denaro inviata */
    double quantity;

    /* Denaro pagato dal sender al nodo che processa la transazione */
    unsigned int reward;
};

struct Messaggio {
    long m_type;
    struct Transazione transazione;
};

/* Struttura per tenere lo stato di un processo e il suo pid */
struct ProcessoNode {
    /* PID del processo */
    pid_t pid;

    /* Stato del processo. Vedasi process_status */
    int status;

    /* Bilancio del processo */
    double balance;

    /* Numero di transazioni nella TP */
    int transactions;

    /* ID della coda di messaggi usata dal processo nodo per ricevere transazioni */
    int msgID;
};

struct ProcessoUser {
    /* PID del processo */
    pid_t pid;

    /* Stato del processo. Vedasi process_status */
    int status;

    /* Bilancio del processo */
    double balance;
};

struct Blocco {
    struct Transazione transazioni[SO_BLOCK_SIZE];
};

struct SharedMemoryID {
    int ledger;
    int nodePIDs;
    int usersPIDs;
    int ledgerRead;
    int stop;
    int freeBlock;
    int stopRead;
    int sem;
};

struct SharedMemory {
    struct Blocco *ledger;
    struct ProcessoNode *nodePIDs;
    struct ProcessoUser *usersPIDs;
    unsigned int *ledgerRead;
    unsigned int *stopRead;
    int *stop;
    int *freeBlock;
};

#endif
