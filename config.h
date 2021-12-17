struct Config {
    /* Numero di processi utente */
    unsigned int SO_USERS_NUM;

    /* Numero di processi nodo */
    unsigned int SO_NODES_NUM;

    /* La percentuale di reward pagata da ogni utente per il processamento di una transazione */
    unsigned int SO_REWARD;

    /* Minimo e massimo valore del tempo (espresso in nanosecondi) che trascorre fra la
    generazione di una transazione e la seguente da parte di un utente */
    int SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC;

    /* Numero di volte che il processo può ritentare una transazione prima di terminare  */
    int SO_RETRY;

    /* Numero massimo di transazioni nella transaction pool dei processi nodo */
    int SO_TP_SIZE;

    /* Numero di transazioni contenute in un blocco */
    unsigned int SO_BLOCK_SIZE;

    /* Minimo e massimo valore del tempo simulato (espresso in nanosecondi) di
    processamento di un blocco da parte di un nodo */
    int SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC;

    /* Numero massimo di blocchi nel libro mastro */
    int SO_REGISTRY_SIZE;

    /* Budget iniziale di ciascun processo utente */
    int SO_BUDGET_INIT;

    /* Durata massima della simulazione (espressa in secondi) */
    int SO_SIM_SEC;

    /* Numero di nodi amici dei processi nodo */
    int SO_NUM_FRIENDS;
};