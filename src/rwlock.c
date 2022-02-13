#include "../vendor/libsem.h"
#include "enum.h"
#include "master.h"
#include "structure.h"
#include "utilities.h"

void read_start(int sem_id, unsigned int *read_counter, int read, int write) {
    sem_reserve(sem_id, read);
    *read_counter += 1;

    if (*read_counter == 1) {
        sem_reserve(sem_id, write);
    }
    sem_release(sem_id, read);
}

void read_end(int sem_id, unsigned int *read_counter, int read, int write) {
    sem_reserve(sem_id, read);
    *read_counter -= 1;

    if (*read_counter == 0) {
        sem_release(sem_id, write);
    }
    sem_release(sem_id, read);
}

int get_stop_value() {
    int tmp;

    read_start(ids.sem, sh.stop_read, STOP_READ, STOP_WRITE);
    tmp = *sh.stop;
    read_end(ids.sem, sh.stop_read, STOP_READ, STOP_WRITE);

    return tmp;
}


struct ProcessoNode get_node(unsigned int i) {
#ifdef trenta
    struct ProcessoNode tmp;

    check_for_update();

    read_start(ids.sem, sh.nodes_pid_read, NODES_PID_READ, NODES_PID_WRITE);
    tmp = sh.nodes_pid[i];
    read_end(ids.sem, sh.nodes_pid_read, NODES_PID_READ, NODES_PID_WRITE);

    return tmp;
#endif
    return sh.nodes_pid[i];
}
