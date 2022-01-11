#include "master.h"
#include "shared_memory.h"

#define PERMS 0666
#define NUM_SEMAFORI FINE_SEMAFORI + cfg.SO_USERS_NUM + 1

void get_shared_ids() {
    /* Allocazione memoria per il libro mastro */
    ids.ledger = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE * sizeof(Blocco), IPC_CREAT | PERMS);
    shmget_error_checking(ids.ledger);

    /* Allocazione del semaforo di lettura come variabile in memoria condivisa */
    ids.readCounter = shmget(IPC_PRIVATE, sizeof(unsigned int), IPC_CREAT | PERMS);
    shmget_error_checking(ids.readCounter);

    /* Allocazione dell'array dello stato dei nodi */
    ids.nodePIDs = shmget(IPC_PRIVATE, cfg.SO_NODES_NUM * sizeof(ProcessoNode), IPC_CREAT | PERMS);
    shmget_error_checking(ids.nodePIDs);

    /* Allocazione dell'array dello stato dei nodi */
    ids.usersPIDs = shmget(IPC_PRIVATE, cfg.SO_USERS_NUM * sizeof(ProcessoUser), IPC_CREAT | PERMS);
    shmget_error_checking(ids.usersPIDs);

    /* Allocazione del flag per far terminare correttamente i processi */
    ids.stop = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | PERMS);
    shmget_error_checking(ids.stop);

    /* Allocazione del puntatore al primo blocco libero del libro mastro */
    ids.freeBlock = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | PERMS);
    shmget_error_checking(ids.freeBlock);

    /* Inizializziamo i semafori che usiamo */
    ids.sem = semget(IPC_PRIVATE, NUM_SEMAFORI, IPC_CREAT | PERMS);
    TEST_ERROR;
}

void attach_shared_memory() {
    /* Collegamento del libro mastro (ci serve per controllo dati) */
    sh.ledger = shmat(ids.ledger, NULL, 0);
    TEST_ERROR;

    /* Inizializziamo il semaforo di lettura a 0 */
    sh.readCounter = shmat(ids.readCounter, NULL, 0);
    TEST_ERROR;

    /* Collegamento all'array dello stato dei processi */
    sh.nodePIDs = shmat(ids.nodePIDs, NULL, 0);
    TEST_ERROR;

    /* Collegamento all'array dello stato dei processi utente */
    sh.usersPIDs = shmat(ids.usersPIDs, NULL, 0);
    TEST_ERROR;

    /* Inizializziamo il flag di terminazione a -1 (verrà abbassato quando tutti i processi devono terminare) */
    sh.stop = shmat(ids.stop, NULL, 0);
    TEST_ERROR;

    /* Puntatore al primo blocco disponibile per la scrittura nel ledger */
    sh.freeBlock = shmat(ids.freeBlock, NULL, 0);
    TEST_ERROR;
}