#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "block.h"
#include "block_function.h"

void init_partition(partition_t *part);

#endif // FILESYSTEM_H