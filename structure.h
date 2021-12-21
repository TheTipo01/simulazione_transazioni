#define _GNU_SOURCE
#ifndef STRUCTURE_H
#define STRUCTURE_H

typedef struct Transazione {
    /* Timestamp della transazione con risoluzione dei nanosecondi
        (vedere funzione clock_gettime()) */
    unsigned long timestamp;

    /* L'utente implicito che ha generato la transazione */
    int sender;

    /* L'utente destinatario della somma di denaro */
    unsigned int receiver;

    /* Quantità di denaro inviata */
    unsigned int quantity;

    /* Denaro pagato dal sender al nodo che processa la transazione */
    unsigned int reward;
} Transazione;

#endif