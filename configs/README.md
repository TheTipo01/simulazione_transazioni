# Configurazioni di esempio

In questa cartella sono presenti alcuni script che eseguono il programma con dei valori di configurazione di esempio
estrapolate dalla documentazione ufficiale del Progetto (versione 19/12/2021).

I parametri `SO_BLOCK_SIZE` e `SO_REGISTRY_SIZE` sono definiti a compile time nel file [config.h](../src/config.h), e di
conseguenza da modificare a mano

| parametro              | conf #1     | conf #2  | conf #3  |
|------------------------|-------------|----------|----------|
| SO_USERS_NUM           | 100         | 1000     | 20       |
| SO_NODES_NUM           | 10          | 10       | 10       |
| SO_BUDGET_INIT         | 1000        | 1000     | 10000    |
| SO_REWARD              | 1           | 20       | 1        |
| SO_MIN_TRANS_GEN_NSEC  | 10000000000 | 10000000 | 10000000 |
| SO_MAX_TRANS_GEN_NSEC  | 200000000   | 10000000 | 20000000 |
| SO_RETRY               | 20          | 2        | 10       |
| SO_TP_SIZE             | 1000        | 20       | 100      |
| SO_BLOCK_SIZE          | 100         | 10       | 10       |
| SO_MIN_TRANS_PROC_NSEC | 10000000    | 1000000  |          |
| SO_MAX_TRANS_PROC_NSEC | 20000000    | 1000000  |          |
| SO_REGISTRY_SIZE       | 1000        | 10000    | 1000     |
| SO_SIM_SEC             | 10          | 20       | 20       |
| SO_FRIENDS_NUM         | 3           | 5        | 3        |
| SO_HOPS                | 10          | 2        | 10       |
