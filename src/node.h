#ifndef SIMULAZIONE_TRANSAZIONI_NODE_H
#define SIMULAZIONE_TRANSAZIONI_NODE_H

#include "node.c"
#include "utilities.c"

/*
 * Funzione principale del processo nodo, responsabile del suo funzionamento.
 * Provvede a ricevere le transazioni dai processi utente, immagazzinarle in una pool e infine processarle (aggiungendole
 * al libro mastro) in blocco.
 * Oltre ai valori di configurazione, ha bisogno degli ID per accedere alle memorie condivise, e un puntatore al proprio
 * PID nella lista condivisa dei PID dei processi nodo.
 */
void startNode(unsigned int nodePosition);

/*
 * Funzione incaricata di generare l'ultima transazione di un blocco, quella contente il reward per il nodo che ha
 * elaborato le transazioni
 */
struct Transazione generateReward(double tot_reward);

#endif
