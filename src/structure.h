#define _GNU_SOURCE
#ifndef STRUCTURE_H
#define STRUCTURE_H

#include "time.h"

struct Transazione {
    /* Timestamp della transazione con risoluzione dei nanosecondi
        (vedere funzione clock_gettime()) */
    struct timespec timestamp;

    /* L'utente implicito che ha generato la transazione */
    int sender;

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
typedef struct Processo {
    /* PID del processo */
    int pid;

    /* Stato del processo. Vedasi process_status */
    int status;

    /* Bilancio del processo */
    unsigned int balance;

    /* Numero di transazioni nella TP (USATO SOLO DAI NODES) */
    int transactions;
} Processo;

#endif
