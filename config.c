#ifndef CONFIG_C
#define CONFIG_C

#include "config.h"

#include <stdlib.h>

Config newConfig() {
    Config config;

    config.SO_USERS_NUM = atoi(getenv("SO_USERS_NUM"));
    config.SO_NODES_NUM = atoi(getenv("SO_NODES_NUM"));
    config.SO_REWARD = atoi(getenv("SO_REWARD"));
    config.SO_MIN_TRANS_GEN_NSEC = atoi(getenv("SO_MIN_TRANS_GEN_NSEC"));
    config.SO_MAX_TRANS_GEN_NSEC = atoi(getenv("SO_MAX_TRANS_GEN_NSEC"));
    config.SO_RETRY = atoi(getenv("SO_RETRY"));
    config.SO_TP_SIZE = atoi(getenv("SO_TP_SIZE"));
    config.SO_MIN_TRANS_PROC_NSEC = atoi(getenv("SO_MIN_TRANS_PROC_NSEC"));
    config.SO_MAX_TRANS_PROC_NSEC = atoi(getenv("SO_MAX_TRANS_PROC_NSEC"));
    config.SO_BUDGET_INIT = atoi(getenv("SO_BUDGET_INIT"));
    config.SO_SIM_SEC = atoi(getenv("SO_SIM_SEC"));
    config.SO_NUM_FRIENDS = atoi(getenv("SO_NUM_FRIENDS"));
    config.SO_HOPS = atoi(getenv("SO_HOPS"));

    return config;
}

#endif