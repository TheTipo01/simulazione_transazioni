#ifndef SHARED_MEMORY
#define SHARED_MEMORY

#include "shared_memory.c"

/*
 * Funzione per inizializzare le memorie condivise in uso
 */
void get_shared_ids();


void attach_shared_memory();


void detach_and_delete();

#endif
