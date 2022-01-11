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
    unsigned int receiver;

    /* Quantità di denaro inviata */
    unsigned int quantity;

    /* Denaro pagato dal sender al nodo che processa la transazione */
    unsigned int reward;
};

struct Messaggio {
    long m_type;
    struct Transazione transazione;
};

/* Struttura per tenere lo stato di un processo e il suo pid */
typedef struct ProcessoNode {
    /* PID del processo */
    pid_t pid;

    /* Stato del processo. Vedasi process_status */
    int status;

    /* Bilancio del processo */
    unsigned int balance;

    /* Numero di transazioni nella TP */
    int transactions;

    /* ID della coda di messaggi usata dal processo nodo per ricevere transazioni */
    int msgID;
} ProcessoNode;

typedef struct ProcessoUser {
    /* PID del processo */
    pid_t pid;

    /* Stato del processo. Vedasi process_status */
    int status;

    /* Bilancio del processo */
    unsigned int balance;
} ProcessoUser;

typedef struct Blocco {
    struct Transazione transazioni[SO_BLOCK_SIZE];
} Blocco;

struct SharedMemoryID {
    int ledger;
    int nodePIDs;
    int usersPIDs;
    int readCounter;
    int stop;
    int sem;
    int freeBlock;
};

struct SharedMemory {
    Blocco *ledger;
    ProcessoNode *nodePIDs;
    ProcessoUser *usersPIDs;
    unsigned int *readCounter;
    int *stop;
    int *freeBlock;
};

#endif
