#include "master.h"
#include "shared_memory.h"
#include "utilities.h"

#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>

void get_shared_ids() {
    ids.ledger = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE * sizeof(struct Blocco), GET_FLAGS);
    shmget_error_checking(ids.ledger);

    ids.ledger_read = shmget(IPC_PRIVATE, sizeof(unsigned int), GET_FLAGS);
    shmget_error_checking(ids.ledger_read);

    ids.nodes_pid = shmget(IPC_PRIVATE, cfg.SO_NODES_NUM * sizeof(struct ProcessoNode), GET_FLAGS);
    shmget_error_checking(ids.nodes_pid);

    ids.users_pid = shmget(IPC_PRIVATE, cfg.SO_USERS_NUM * sizeof(struct ProcessoUser), GET_FLAGS);
    shmget_error_checking(ids.users_pid);

    ids.stop = shmget(IPC_PRIVATE, sizeof(int), GET_FLAGS);
    shmget_error_checking(ids.stop);

    ids.ledger_free_block = shmget(IPC_PRIVATE, sizeof(int), GET_FLAGS);
    shmget_error_checking(ids.ledger_free_block);

    ids.stop_read = shmget(IPC_PRIVATE, sizeof(unsigned int), GET_FLAGS);
    shmget_error_checking(ids.stop_read);

    ids.mmts = shmget(IPC_PRIVATE, 10 * sizeof(struct Transazione), GET_FLAGS);
    shmget_error_checking(ids.mmts);

    ids.mmts_free_block = shmget(IPC_PRIVATE, sizeof(int), GET_FLAGS);
    shmget_error_checking(ids.mmts_free_block);

    ids.user_waiting = shmget(IPC_PRIVATE, sizeof(unsigned int), GET_FLAGS);
    shmget_error_checking(ids.user_waiting);

    ids.node_full = shmget(IPC_PRIVATE, sizeof(unsigned int), GET_FLAGS);
    shmget_error_checking(ids.user_waiting);

    ids.sem = semget(IPC_PRIVATE, NUM_SEMAFORI, GET_FLAGS);
    TEST_ERROR;
}

void attach_shared_memory() {
    sh.ledger = shmat(ids.ledger, NULL, 0);
    TEST_ERROR;

    sh.ledger_read = shmat(ids.ledger_read, NULL, 0);
    TEST_ERROR;

    sh.nodes_pid = shmat(ids.nodes_pid, NULL, 0);
    TEST_ERROR;

    sh.users_pid = shmat(ids.users_pid, NULL, 0);
    TEST_ERROR;

    sh.stop = shmat(ids.stop, NULL, 0);
    TEST_ERROR;

    sh.ledger_free_block = shmat(ids.ledger_free_block, NULL, 0);
    TEST_ERROR;

    sh.stop_read = shmat(ids.stop_read, NULL, 0);
    TEST_ERROR;

    sh.mmts = shmat(ids.mmts, NULL, 0);
    TEST_ERROR;

    sh.mmts_free_block = shmat(ids.mmts_free_block, NULL, 0);
    TEST_ERROR;

    sh.user_waiting = shmat(ids.user_waiting, NULL, 0);
    TEST_ERROR;

    sh.node_full = shmat(ids.node_full, NULL, 0);
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
