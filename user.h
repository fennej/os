#ifndef USER_H
#define USER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "block.h"


int check_permission(partition_t *part, int inode_num, int perm_bit);
int switch_user(partition_t *part, int uid, int gid);


#endif // USER_H