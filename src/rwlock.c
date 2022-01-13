#include "../vendor/libsem.h"
#include "enum.h"
#include "master.h"

void read_start(int sem_id, unsigned int *readCounter, int read, int write) {
    sem_reserve(sem_id, read);
    *readCounter += 1;

    if (*readCounter == 1) {
        sem_reserve(sem_id, write);
    }
    sem_release(sem_id, read);
}

void read_end(int sem_id, unsigned int *readCounter, int read, int write) {
    sem_reserve(sem_id, read);
    *readCounter -= 1;

    if (*readCounter == 0) {
        sem_release(sem_id, write);
    }
    sem_release(sem_id, read);
}

int get_stop_value(int *stop, unsigned int *readCounter) {
    int tmp;
    read_start(ids.sem, readCounter, STOP_READ, STOP_WRITE);
    tmp = *stop;
    read_end(ids.sem, readCounter, STOP_READ, STOP_WRITE);
    return tmp;
}
