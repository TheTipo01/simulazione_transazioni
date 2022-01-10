#ifndef LIBSEM_C
#define LIBSEM_C

#include <sys/sem.h>
#include "libsem.h"

/* Set a semaphore to a user defined value */
int sem_set_val(int sem_id, int sem_num, int sem_val) {
    return semctl(sem_id, sem_num, SETVAL, sem_val);
}

/* Try to access the resource */
int sem_reserve(int sem_id, int sem_num) {
    struct sembuf sops;

    sops.sem_num = sem_num;
    sops.sem_op = -1;
    sops.sem_flg = 0;
    return semop(sem_id, &sops, 1);
}

/* Release the resource */
int sem_release(int sem_id, int sem_num) {
    struct sembuf sops;

    sops.sem_num = sem_num;
    sops.sem_op = 1;
    sops.sem_flg = 0;

    return semop(sem_id, &sops, 1);
}

#endif
