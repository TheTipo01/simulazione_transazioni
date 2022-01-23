#ifndef ENUM_H
#define ENUM_H

/* Enumerazione per i semafori in uso nel programma */
enum semafori {
    LEDGER_READ,
    LEDGER_WRITE,
    STOP_READ,
    STOP_WRITE,
    MMTS,
    USER_WAITING,
    NODE_FULL,
    FINE_SEMAFORI
};

/* Possibili stati dei processi */
enum process_status {
    PROCESS_RUNNING,
    PROCESS_WAITING,
    PROCESS_FINISHED,
    PROCESS_FINISHED_PREMATURELY
};

/*
 * Motivo di terminazione della simulazione */
enum termination_reason {
    TIMEDOUT,
    LEDGERFULL,
    FINISHED,
    NOBALANCE,
    ALLTPFULL
};
#endif
