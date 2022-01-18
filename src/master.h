#ifndef MASTER_H
#define MASTER_H

#include "config.h"
#include "structure.h"

/* Dichiarazioni globali per la configurazione e per le memorie condivise. */
extern Config cfg;
extern struct SharedMemory sh;
extern struct SharedMemoryID ids;

#endif
