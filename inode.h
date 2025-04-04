#ifndef INODE_H
#define INODE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "structure.h"

int allocate_inode(partition_t *part);
void free_inode(partition_t *part, int inode_num);

#endif // INODE_H


