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

#define TEST_ERROR do \
        {                      \
        if (errno)    \
        { \
            fprintf(stderr, "%s:%d: PID=%5d: Error %d (%s)\n", __FILE__, __LINE__, getpid(), errno,strerror(errno)); \
        } \
        } while (0)

#define clrscr() printf("\033[1;1H\033[2J")
#define msg_size() sizeof(struct Messaggio) - sizeof(long)

/*
 * Funzione per il controllo dell'assegnazione della memoria condivida. Se l'address ritornato dalla funzione shmget
 * è uguale a -1, si esce dal programma segnalando l'errore.
 */
void shmget_error_checking(int id);

/*
 * Funzione minimale per stampare lo stato dei processi. Vengono stampati:
 *      N° di processi utente attivi
 *      N° di processi nodo attivi
 *      Processo utente/nodo con bilancio più alto
 *      Processo utente/nodo con bilancio più basso
 */
void printStatus();

/*
 * Funzione alternativa più completa per visualizzare lo stato dei processi. Vengono stampati:
 *      N° di processi utente attivi
 *      N° di processi nodo attivi
 *      I 3 utenti con bilancio più alto e i 3 con bilancio più basso
 *      I 3 nodi con bilancio più alto e i 3 con bilancio più basso
 *      PID, bilancio e stato dei processi osservati
 */
void print_more_status();

/* Funzione wrapper per nanosleep. */
int sleeping(long waitingTime);

/* Funzione che dato un tempo time_t restituisce una stringa formattata a partire dal timestamp. */
char *format_time(time_t rawtime);

/* Funzione utile ad ottenere una stringa appropriata per lo stato del processo osservato. Utilizzata principalmente
 * per la visualizzazione, nella funzione printStatus e print_more_status. */
char *get_status(int status);

#endif
