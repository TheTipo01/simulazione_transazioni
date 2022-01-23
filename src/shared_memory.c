#include "master.h"
#include "shared_memory.h"
#include "utilities.h"

#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>

void get_shared_ids() {
    /* Allocazione memoria per il libro mastro */
    ids.ledger = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE * sizeof(struct Blocco), GET_FLAGS);
    shmget_error_checking(ids.ledger);

    /* Allocazione del semaforo di lettura come variabile in memoria condivisa */
    ids.ledger_read = shmget(IPC_PRIVATE, sizeof(unsigned int), GET_FLAGS);
    shmget_error_checking(ids.ledger_read);

    /* Allocazione dell'array dello stato dei nodi */
    ids.nodes_pid = shmget(IPC_PRIVATE, cfg.SO_NODES_NUM * sizeof(struct ProcessoNode), GET_FLAGS);
    shmget_error_checking(ids.nodes_pid);

    /* Allocazione dell'array dello stato dei nodi */
    ids.users_pid = shmget(IPC_PRIVATE, cfg.SO_USERS_NUM * sizeof(struct ProcessoUser), GET_FLAGS);
    shmget_error_checking(ids.users_pid);

    /* Allocazione del flag per far terminare correttamente i processi */
    ids.stop = shmget(IPC_PRIVATE, sizeof(int), GET_FLAGS);
    shmget_error_checking(ids.stop);

    /* Allocazione del puntatore al primo blocco libero del libro mastro */
    ids.ledger_free_block = shmget(IPC_PRIVATE, sizeof(int), GET_FLAGS);
    shmget_error_checking(ids.ledger_free_block);

    /* Allocazione del puntatore al primo blocco libero del libro mastro */
    ids.stop_read = shmget(IPC_PRIVATE, sizeof(unsigned int), GET_FLAGS);
    shmget_error_checking(ids.stop_read);

    /* Allocazione del segmento dedicato alle transazioni effettuate via CLI */
    ids.mmts = shmget(IPC_PRIVATE, 10 * sizeof(struct Transazione), GET_FLAGS);
    shmget_error_checking(ids.mmts);

    /* Allocazione del puntatore al primo spazio libero nell'array delle transazioni man-made */
    ids.mmts_free_block = shmget(IPC_PRIVATE, sizeof(int), GET_FLAGS);
    shmget_error_checking(ids.mmts_free_block);

    ids.new_nodes_pid = shmget(IPC_PRIVATE, sizeof(int), GET_FLAGS);
    shmget_error_checking(ids.new_nodes_pid);

    ids.nodes_pid_read = shmget(IPC_PRIVATE, sizeof(unsigned int), GET_FLAGS);
    shmget_error_checking(ids.nodes_pid_read);

    ids.user_waiting = shmget(IPC_PRIVATE, sizeof(unsigned int), GET_FLAGS);
    shmget_error_checking(ids.user_waiting);

    /* Inizializziamo i semafori che usiamo */
    ids.sem = semget(IPC_PRIVATE, NUM_SEMAFORI, GET_FLAGS);
    TEST_ERROR;
}

void attach_shared_memory() {
    /* Collegamento del libro mastro (ci serve per controllo dati) */
    sh.ledger = shmat(ids.ledger, NULL, 0);
    TEST_ERROR;

    /* Inizializziamo il semaforo di lettura a 0 */
    sh.ledger_read = shmat(ids.ledger_read, NULL, 0);
    TEST_ERROR;

    /* Collegamento all'array dello stato dei processi */
    sh.nodes_pid = shmat(ids.nodes_pid, NULL, 0);
    TEST_ERROR;

    /* Collegamento all'array dello stato dei processi utente */
    sh.users_pid = shmat(ids.users_pid, NULL, 0);
    TEST_ERROR;

    /* Inizializziamo il flag di terminazione a -1 (verrà abbassato quando tutti i processi devono terminare) */
    sh.stop = shmat(ids.stop, NULL, 0);
    TEST_ERROR;

    /* Puntatore al primo blocco disponibile per la scrittura nel ledger */
    sh.ledger_free_block = shmat(ids.ledger_free_block, NULL, 0);
    TEST_ERROR;

    sh.stop_read = shmat(ids.stop_read, NULL, 0);
    TEST_ERROR;

    sh.mmts = shmat(ids.mmts, NULL, 0);
    TEST_ERROR;

    sh.new_nodes_pid = shmat(ids.new_nodes_pid, NULL, 0);
    TEST_ERROR;

    sh.nodes_pid_read = shmat(ids.nodes_pid_read, NULL, 0);
    TEST_ERROR;

    sh.user_waiting = shmat(ids.user_waiting, NULL, 0);
    TEST_ERROR;
}

void detach_and_delete() {
    shmdt_error_checking(sh.ledger);
    shmdt_error_checking(sh.nodes_pid);
    shmdt_error_checking(sh.users_pid);
    shmdt_error_checking(sh.ledger_read);
    shmdt_error_checking(sh.stop);
    shmdt_error_checking(sh.ledger_free_block);
    shmdt_error_checking(sh.stop_read);
    shmdt_error_checking(sh.new_nodes_pid);
    shmdt_error_checking(sh.nodes_pid_read);

    semctl(ids.sem, 0, IPC_RMID);
    shmctl(ids.nodes_pid, IPC_RMID, NULL);
    shmctl(ids.users_pid, IPC_RMID, NULL);
    shmctl(ids.ledger_read, IPC_RMID, NULL);
    shmctl(ids.stop, IPC_RMID, NULL);
    shmctl(ids.ledger_free_block, IPC_RMID, NULL);
    shmctl(ids.stop_read, IPC_RMID, NULL);
}

void delete_message_queue() {
    int i;

    for (i = 0; i < cfg.SO_NODES_NUM; i++) {
        msgctl(sh.nodes_pid[i].msg_id, IPC_RMID, NULL);
    }
}
