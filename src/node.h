#ifndef SIMULAZIONE_TRANSAZIONI_NODE_H
#define SIMULAZIONE_TRANSAZIONI_NODE_H

/*
 * Funzione principale del processo nodo, responsabile del suo funzionamento.
 * Provvede a ricevere le transazioni dai processi utente, immagazzinarle in una pool e infine processarle (aggiungendole
 * al libro mastro) in blocco.
 * Come parametro è presente il solo puntatore alla sua posizione nell'array dei nodi presente in memoria condivisa,
 * in quanto il resto dei dati che gli servono per eseguire le sue operazioni sono presenti in memoria condivisa.
 */
void start_node(unsigned int index);

/*
 * Funzione incaricata di generare l'ultima transazione di un blocco, quella contente il reward per il nodo che ha
 * elaborato le transazioni.
 */
struct Transazione generate_reward(unsigned int tot_reward);

#ifdef trenta
/* Funzione incaricata di aggiungere un nodo alla lista dei nodi amici */
void enlarge_friends();

/* Invia le transazioni ad altri  */
void send_to_others();
#endif

#endif
