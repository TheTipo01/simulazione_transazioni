# Questo file è presente per fornire ad IDE come CLion l'autocompletamento del codice
cmake_minimum_required(VERSION 3.16.3)
project(simulazione_transazioni C)

set(CMAKE_C_STANDARD 90)

set(SOURCES
        src/master.h
        src/master.c
        src/user.c
        src/node.c
        src/config.c
        src/rwlock.c
        src/shared_memory.c
        src/utilities.c
        vendor/libsem.c
        )

add_executable(simulazione_transazioni ${SOURCES})
