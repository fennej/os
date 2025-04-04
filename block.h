#ifndef BLOCK_H
#define BLOCK_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "structure.h"
#include "load.h"

int allocate_block(partition_t *part);
void free_block(partition_t *part, int block_num);

#endif // BLOCK_H