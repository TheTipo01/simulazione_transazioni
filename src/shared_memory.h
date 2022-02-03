#ifndef SHARED_MEMORY
#define SHARED_MEMORY

#include "enum.h"

/* Flag per le risorse IPC di System V */
#define GET_FLAGS IPC_CREAT | IPC_EXCL | SHM_R | SHM_W
/* Numero di semafori in uso */
#define NUM_SEMAFORI FINE_SEMAFORI + cfg.SO_USERS_NUM + 1

/* Funzione per inizializzare le memorie condivise in uso */
void get_shared_ids();

/* Attach di tutte le memorie condivise */
void attach_shared_memory();

/* Eliminazione delle memorie condivise in uso */
void delete_shared_memory();

/* Eliminazione delle code di messaggi in uso */
void delete_message_queue();

#endif
