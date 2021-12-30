#ifndef ENUM_H
#define ENUM_H
enum semafori {
    LEDGER_READ,
    LEDGER_WRITE,
    FINE_SEMAFORI
};

enum process_status {
    PROCESS_RUNNING,
    PROCESS_WAITING,
    PROCESS_FINISHED,
    PROCESS_FINISHED_PREMATURELY
};

enum termination_reason {
    TIMEDOUT,
    LEDGERFULL,
    FINISHED
};
#endif