#define _GNU_SOURCE
#ifndef STRUCTURE_H
#define STRUCTURE_H

#include "config.h"

#include <time.h>

/* Struttura per tenere informazioni su una singola transazione */
struct Transazione {
    /*
     * Timestamp della transazione con risoluzione dei nanosecondi
     * (vedere funzione clock_gettime())
    */
    time_t timestamp;

    /* L'utente implicito che ha generato la transazione */
    pid_t sender;

    /* L'utente destinatario della somma di denaro */
    pid_t receiver;

    /* Quantità di denaro inviata */
    unsigned int quantity;

    /* Denaro pagato dal sender al nodo che processa la transazione */
    unsigned int reward;
};

/* Struttura wrapper per inviare un messaggio contenente una transazione */
struct Messaggio {
    /* Tipo di messaggio (richiesto dalle message queue) */
    long m_type;

    /* Transazione da inviare */
    struct Transazione transazione;

    /* Numero di salti effettuati dalla transazione */
    int hops;
};

/* Struttura per tenere lo stato di un processo nodo */
struct ProcessoNode {
    /* PID del processo */
    pid_t pid;

    /* Stato del processo. Vedasi process_status */
    int status;

    /* Bilancio del processo */
    unsigned int balance;

    /* ID della coda di messaggi usata dal processo nodo per ricevere transazioni */
    int msg_id;

    /* Array d'indici dei nodi amici in nodes_pid */
    int *friends;

    /* Puntatore alla transaction pool */
    unsigned int last;
};

/* Struttura per tenere lo stato di un processo user */
struct ProcessoUser {
    /* PID del processo */
    pid_t pid;

    /* Stato del processo. Vedasi process_status */
    int status;

    /* Bilancio del processo */
    unsigned int balance;
};

/* Struttura che identifica un singolo blocco nel ledger (libro mastro) */
struct Blocco {
    struct Transazione transazioni[SO_BLOCK_SIZE];
};

/* ID di tutte le shared memory usate all'interno del programma */
struct SharedMemoryID {
    /* Libro mastro */
    int ledger;

    /* Array di stato dei processi nodi */
    int nodes_pid;

    /* Array di stato dei processi utenti */
    int users_pid;

    /* Array per le transazione generate attraverso un segnale */
    int mmts;

    /* Prima riga libera nell'array mmts */
    int mmts_free_block;

    /* Numero di processi attualmente in lettura su ledger */
    int ledger_read;

    /* Flag usato per far uscire i processi e per sapere perchè sono usciti */
    int stop;

    /* Primo blocco libero nel ledger (libro mastro) */
    int ledger_free_block;

    /* Numero di processi in lettura su stop */
    int stop_read;

    /* ID dell'array di semafori */
    int sem;

    /* Coda di messaggi usata per inviare la transazioni indietro al processo master */
    int master_msg_id;

    /* ID della shared memory dei nodi. Usata per controllare se abbiamo agginto un nodo */
    int new_nodes_pid;

    /* Numero di processi in lettura su nodes_pid */
    int nodes_pid_read;

    /* Numero di processi utenti che aspettano un aumento del bilancio */
    int user_waiting;
};

/* Struttura dati per contenere la memoria condivisa usata nel programma */
struct SharedMemory {
    /* Libro mastro */
    struct Blocco *ledger;

    /* Array di stato dei processi nodi */
    struct ProcessoNode *nodes_pid;

    /* Array di stato dei processi utenti */
    struct ProcessoUser *users_pid;

    /* Array per le transazione generate attraverso un segnale */
    struct Transazione *mmts;

    /* Prima riga libera nell'array mmts */
    int *mmts_free_block;

    /* Numero di processi attualmente in lettura su ledger */
    unsigned int *ledger_read;

    /* Flag usato per far uscire i processi e per sapere perchè sono usciti */
    int *stop;

    /* Primo blocco libero nel ledger (libro mastro) */
    int *ledger_free_block;

    /* Numero di processi in lettura su stop */
    unsigned int *stop_read;

    /* ID della shared memory dei nodi. Usata per controllare se abbiamo agginto un nodo */
    int *new_nodes_pid;

    /* Numero di processi in lettura su nodes_pid */
    unsigned int *nodes_pid_read;

    /* Numero di processi utenti che aspettano un aumento del bilancio */
    unsigned int *user_waiting;
};

struct Messaggio_PID {
    /* Tipo di messaggio (richiesto dalle message queue) */
    long m_type;

    int index;
};

#endif
