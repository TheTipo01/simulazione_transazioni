#ifndef USER_H
#define USER_H

#include "structure.h"

/* Funzione di calcolo delle entrate utilizzata dal processo utente.
 * Si controlla ogni transazione dell'ultimo blocco pieno nella TP per trovare transazioni il cui receiver
 * è il processo utente stesso. Viene creata una variabile temporanea per immagazzinare tutte le entrate registrate. La
 * funzione ritorna quindi il valore trovato, che viene poi aggiunto al bilancio totale dell'utente. E' presente un semaforo
 * che permette al processo stesso e agli altri processi di leggere ma non di scrivere, in modo di evitare inconsistenze.
 * */
double calcEntrate(int semID, struct Blocco *lm, unsigned int *readCounter);

/* Funzione principale del processo utente, responsabile del suo funzionamento.
 * Essa provvede a creare transazioni da inviare ai processi nodo.
 * Come parametro è presente il solo puntatore alla sua posizione nell'array degli utenti presente in memoria condivisa,
 * in quanto il resto dei dati che gli servono per eseguire le sue operazioni sono presenti in mem. condivisa.
 * */
void startUser(unsigned int index);

/*
 * Questa funzione è incaricata di generare le transazioni. Viene usata nel ciclo del processo utente, e come signal
 * handler per generarne una sul segnale SIGUSR2.
 */
void transactionGenerator(int sig);

#endif
