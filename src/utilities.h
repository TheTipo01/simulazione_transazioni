#ifndef UTILITIES_H
#define UTILITIES_H

#include "structure.h"
#include "config.h"
#include "utilities.c"

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

/*
 * Funzione per il controllo dell'assegnazione della memoria condivida. Se l'address ritornato dalla funzione shmget
 * è uguale a -1, si esce dal programma segnalando l'errore.
 */
void shmget_error_checking(int id);

/*
 * Funzione utile per stampare lo stato di ogni processo. Vengono stampati:
 *      N° di processi utente attivi
 *      N° di processi nodo attivi
 *      Processo utente/nodo con bilancio più alto
 *      Processo utente/nodo con bilancio più basso
 */
void printStatus();

/* Funzione wrapper per nanosleep. */
int sleeping(long waitingTime);

/* Funzione che dato un tempo time_t restituisce una stringa formattata a partire dal timestamp. */
char *formatTime(time_t rawtime);

#endif
