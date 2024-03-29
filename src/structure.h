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

    /* ID della coda di messaggi usata dal processo nodo per ricevere transazioni */
    int msg_id;
};

struct PrintNode {
    pid_t pid;

    int balance;
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

    /* Flag usato per far uscire i processi e per sapere perché sono usciti */
    int stop;

    /* Primo blocco libero nel ledger (libro mastro) */
    int ledger_free_block;

    /* Numero di processi in lettura su stop */
    int stop_read;

    /* ID dell'array di semafori */
    int sem;

    /* Numero di processi nodo avviati */
    int nodes_num;

    /* Coda di messaggi usata per inviare la transazioni indietro al processo master */
    int msg_master;

    /* Coda di messaggi usata per ricevere i nuovi nodi amici da aggiungere dai nodi */
    int msg_friends;

    int msg_new_node;

    int msg_tp_remaining;
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

    /* Flag usato per far uscire i processi e per sapere perché sono usciti */
    int *stop;

    /* Primo blocco libero nel ledger (libro mastro) */
    int *ledger_free_block;

    /* Numero di processi in lettura su stop */
    unsigned int *stop_read;
};

struct Messaggio_int {
    /* Tipo di messaggio (richiesto dalle message queue) */
    long m_type;

    /* Semplice numero */
    int n;
};

struct Messaggio_node {
    /* Tipo di messaggio (richiesto dalle message queue) */
    long m_type;

    struct ProcessoNode nodo;
};

struct Messaggio_tp {
    /* Tipo di messaggio (richiesto dalle message queue) */
    long m_type;

    int pid;
    int n;
};

#endif
