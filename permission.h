#ifndef PERMISSION_H
#define PERMISSION_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "structure.h"
#include "load.h"
int check_permission(partition_t *part, int inode_num, int perm_bit);
int chmod_file(partition_t *part, const char *name, int mode);
int chown_file(partition_t *part, const char *name, int uid, int gid);
int switch_user(partition_t *part, int uid, int gid);

#endif // PERMISSION_H