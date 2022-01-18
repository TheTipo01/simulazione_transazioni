#ifndef SHARED_MEMORY
#define SHARED_MEMORY

#include "enum.h"

#define GET_FLAGS IPC_CREAT | IPC_EXCL | SHM_R | SHM_W
#define NUM_SEMAFORI FINE_SEMAFORI + cfg.SO_USERS_NUM + 1

/*
 * Funzione per inizializzare le memorie condivise in uso
 */
void get_shared_ids();


void attach_shared_memory();


void detach_and_delete();

void delete_message_queue();

#endif
