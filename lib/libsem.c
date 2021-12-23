#ifndef LIBSEM_C
#define LIBSEM_C

#include "libsem.h"

#include <errno.h>
#include <unistd.h>


#define TEST_ERROR if (errno) {fprintf(stderr,                \
                       "%s:%d: PID=%5d: Error %d (%s)\n", \
                       __FILE__,            \
                       __LINE__,            \
                       getpid(),            \
                       errno,                \
                       strerror(errno));}

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