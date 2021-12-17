#define _GNU_SOURCE

#include "config.h"
#include <unistd.h>

/*
 * master: crea SO_USERS_NUM processi utente, gestisce simulazione
 * user: fa transaction
 * node: elabora transaction e riceve reward
 *
 * TODO: libro mastro come linked list, per sincronizzare usiamo shared memory shit
 * TODO: ogni processo node avrà una message queue a cui gli user inviano messaggi
 * TODO: fare in modo che si possano inviare segnali al master per fare roba™️
 */



struct Config cfg;

int main(int argc, char *argv[]) {

}