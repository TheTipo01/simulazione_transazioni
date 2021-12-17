#define _GNU_SOURCE
#ifndef CONFIG_H
#define CONFIG_H

/* Numero di processi utente */
#define SO_USERS_NUM 10

/* Numero di processi nodo */
#define SO_NODES_NUM 10

/* La percentuale di reward pagata da ogni utente per il processamento di una transazione */
#define SO_REWARD 50

/* Minimo e massimo valore del tempo (espresso in nanosecondi) che trascorre fra la
generazione di una transazione e la seguente da parte di un utente */
#define SO_MIN_TRANS_GEN_NSEC 0
#define SO_MAX_TRANS_GEN_NSEC 500

/* Numero di volte che il processo può ritentare una transazione prima di terminare  */
#define SO_RETRY 5

/* Numero massimo di transazioni nella transaction pool dei processi nodo */
#define SO_TP_SIZE 100

/* Numero di transazioni contenute in un blocco */
#define SO_BLOCK_SIZE 100

/* Minimo e massimo valore del tempo simulato (espresso in nanosecondi) di
processamento di un blocco da parte di un nodo */
#define SO_MIN_TRANS_PROC_NSEC 0
#define SO_MAX_TRANS_PROC_NSEC 500

/* Numero massimo di blocchi nel libro mastro */
#define SO_REGISTRY_SIZE 100

/* Budget iniziale di ciascun processo utente */
#define SO_BUDGET_INIT 10000

/* Durata massima della simulazione (espressa in secondi) */
#define SO_SIM_SEC 3600

/* Numero di nodi amici dei processi nodo */
#define SO_NUM_FRIENDS 10

#endif