#ifndef UTILITIES_H
#define UTILITIES_H

#include "structure.h"
#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
 * Macro per il controllo del detach della memoria condivisa. Se l'address ritornato dalla funzione shmdt è uguale a -1,
 * si esce dal programma segnalando l'errore.
 */
#define shmdt_error_checking(addr) do \
        {                              \
        if (shmdt((addr)) == -1) \
        { \
            fprintf(stderr, "%s: %d. Errore in shmdt #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno)); \
            exit(EXIT_FAILURE); \
        } \
        } while(0)

/* Macro per controllare errori su errno */
#define TEST_ERROR do \
        {                      \
        if (errno && errno != 42)    \
        { \
            fprintf(stderr, "%s:%d: PID=%5d: Error %d (%s)\n", __FILE__, __LINE__, getpid(), errno,strerror(errno)); \
        } \
        } while (0)

/* Macro per pulire lo schermo */
#define clrscr() printf("\033[1;1H\033[2J")
/* Dimensione di un messaggio contenente una transazione */
#define msg_size() sizeof(struct Messaggio) - sizeof(long)

/*
 * Funzione per il controllo dell'assegnazione della memoria condivida. Se l'address ritornato dalla funzione shmget
 * è uguale a -1, si esce dal programma segnalando l'errore.
 */
void shmget_error_checking(int id);

/*
 * Funzione alternativa più completa per visualizzare lo stato dei processi. Vengono stampati:
 *      N° di processi utente attivi
 *      N° di processi nodo attivi
 *      I tre utenti con bilancio più alto e i tre con bilancio più basso
 *      I tre nodi con bilancio più alto e i tre con bilancio più basso
 *      PID, bilancio e stato dei processi osservati
 */
void print_more_status();

/* Funzione wrapper per nanosleep. */
int sleeping(long waiting_time);

/* Funzione che dato un tempo time_t restituisce una stringa formattata a partire dal timestamp. */
char *format_time(time_t rawtime);

/*
 * Funzione utile a ottenere una stringa appropriata per lo stato del processo osservato. Utilizzata principalmente
 * per la visualizzazione, nella funzione print_status e print_more_status.
 */
char *get_status(int status);

/* Funzione che calcola il bilancio del nodo leggendo l'ultima transazione di ogni blocco nel ledger */
int get_node_balance(int pos);

/* Funzione che ritorna un numero casuale in un range definito. */
long random_between_two(long min, long max);

/* Funzione delegata di allargare l'array sh.nodes_pid */
void expand_node();

/*
 * Funzione che controlla se bisogna eseguire il reattach della shared mem, solitamente accade dopo che viene creato un
 * nuovo nodo (l'array nodes_pid in memoria condivisa viene espanso)
 */
void check_for_update();

#endif
