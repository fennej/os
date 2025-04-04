#ifndef INIT_H
#define INIT_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "structure.h"
#include "load.h"

void init_partition(partition_t *part);

#endif // INIT_H