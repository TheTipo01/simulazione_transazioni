#!/bin/bash
export SO_USERS_NUM=100
export SO_NODES_NUM=10
export SO_BUDGET_INIT=1000
export SO_REWARD=1
export SO_MIN_TRANS_GEN_NSEC=100000000
export SO_MAX_TRANS_GEN_NSEC=200000000
export SO_RETRY=20
export SO_TP_SIZE=1000
export SO_MIN_TRANS_PROC_NSEC=10000000
export SO_MAX_TRANS_PROC_NSEC=20000000
export SO_SIM_SEC=10
export SO_NUM_FRIENDS=3
export SO_HOPS=10

./master
