#include "config.h"

#include <stdlib.h>
#include <stdio.h>

char *get_env(const char *name) {
    char *value = getenv(name);

    if (value == NULL) {
        fprintf(stderr, "%s: variabile d'ambiente non esistente!\n", name);
        exit(EXIT_FAILURE);
    } else {
        return value;
    }
}

Config newConfig() {
    Config config;

    config.SO_USERS_NUM = atoi(get_env("SO_USERS_NUM"));

    config.SO_NODES_NUM = atoi(get_env("SO_NODES_NUM"));

    config.SO_REWARD = atoi(get_env("SO_REWARD"));
    if (config.SO_REWARD > 100 || config.SO_REWARD < 0) {
        fprintf(stderr, "SO_REWARD: deve essere compreso tra 0 e 100!\n");
        exit(EXIT_FAILURE);
    }

    config.SO_MIN_TRANS_GEN_NSEC = atoi(get_env("SO_MIN_TRANS_GEN_NSEC"));

    config.SO_MAX_TRANS_GEN_NSEC = atoi(get_env("SO_MAX_TRANS_GEN_NSEC"));

    config.SO_RETRY = atoi(get_env("SO_RETRY"));

    config.SO_TP_SIZE = atoi(get_env("SO_TP_SIZE"));

    config.SO_MIN_TRANS_PROC_NSEC = atoi(get_env("SO_MIN_TRANS_PROC_NSEC"));

    config.SO_MAX_TRANS_PROC_NSEC = atoi(get_env("SO_MAX_TRANS_PROC_NSEC"));

    config.SO_BUDGET_INIT = atoi(get_env("SO_BUDGET_INIT"));

    config.SO_SIM_SEC = atoi(get_env("SO_SIM_SEC"));

    config.SO_NUM_FRIENDS = atoi(get_env("SO_NUM_FRIENDS"));

    config.SO_HOPS = atoi(get_env("SO_HOPS"));

    return config;
}

