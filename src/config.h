#ifndef CONFIG_H
#define CONFIG_H

/* Numero di transazioni contenute in un blocco */
#define SO_BLOCK_SIZE 100

/* Numero massimo di blocchi nel libro mastro */
#define SO_REGISTRY_SIZE 1000

typedef struct Config {
    /* Numero di processi utente */
    unsigned int SO_USERS_NUM;

    /* Numero di processi nodo */
    unsigned int SO_NODES_NUM;

    /* La percentuale di reward pagata da ogni utente per il processamento di una transazione */
    unsigned int SO_REWARD;

    /* Minimo e massimo valore del tempo (espresso in nanosecondi) che trascorre fra la
    generazione di una transazione e la seguente da parte di un utente */
    long SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC;

    /* Numero di volte che il processo può ritentare una transazione prima di terminare  */
    int SO_RETRY;

    /* Numero massimo di transazioni nella transaction pool dei processi nodo */
    int SO_TP_SIZE;

    /* Minimo e massimo valore del tempo simulato (espresso in nanosecondi) di
    processamento di un blocco da parte di un nodo */
    long SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC;

    /* Budget iniziale di ciascun processo utente */
    int SO_BUDGET_INIT;

    /* Durata massima della simulazione (espressa in secondi) */
    int SO_SIM_SEC;

    /* Numero di nodi amici dei processi nodo */
    int SO_NUM_FRIENDS;

    /* Numero massimo di inoltri di una transazione verso nodi amici prima che il master crei un nuovo nodo */
    int SO_HOPS;
} Config;

Config newConfig();

/* Funzione utile per error-checking nella fase di inizializzazione delle variabili d'ambiente */
char *get_env(const char *name);

#endif

