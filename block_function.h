#ifndef BLOCK_FUN_H
#define BLOCK_FUN_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "block.h"


int allocate_block(partition_t *part);
void free_block(partition_t *part, int block_num);

#endif // BLOCK_H