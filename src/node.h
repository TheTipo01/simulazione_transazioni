#ifndef SIMULAZIONE_TRANSAZIONI_NODE_H
#define SIMULAZIONE_TRANSAZIONI_NODE_H

/*
 * Funzione principale del processo nodo, responsabile del suo funzionamento.
 * Provvede a ricevere le transazioni dai processi utente, immagazzinarle in una pool e infine processarle (aggiungendole
 * al libro mastro) in blocco.
 * Come parametro è presente il solo puntatore alla sua posizione nell'array dei nodi presente in memoria condivisa,
 * in quanto il resto dei dati che gli servono per eseguire le sue operazioni sono presenti in mem. condivisa.
 */
void startNode(unsigned int index);

/*
 * Funzione incaricata di generare l'ultima transazione di un blocco, quella contente il reward per il nodo che ha
 * elaborato le transazioni.
 */
struct Transazione generateReward(double tot_reward);

#endif
