#ifndef LOAD_H
#define LOAD_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "structure.h"
#include "init.h"
extern partition_t *global_partition;
void sigint_handler(int sig);
void free_partition(partition_t *part);
partition_t* create_new_partition();
int load_partition(partition_t *part, const char *filename);
int save_partition(partition_t *part, const char *filename);
#endif // LOAD_H