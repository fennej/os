#ifndef FILE_OPERATIONH
#define FILE_OPERATION_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "structure.h"
#include "load.h"
#include "permission.h"
#include "inode.h"
#include "block.h"
#include "folder_operation.h"
int create_file(partition_t *part, const char *name, int mode);
int find_file_in_dir(partition_t *part, int dir_inode, const char *name);
int create_symlink(partition_t *part, const char *link_name, const char *target_name);
int delete_file(partition_t *part, const char *name);
int resolve_symlink(partition_t *part, int inode_num);
int resolve_symlink2(partition_t *part, int symlink_inode);
int resolve_pathAB(partition_t *part, const char *path);
int read_from_file(partition_t *part, const char *name, char *buffer, int max_size);
void cat_command(partition_t *part, const char *name);
int cat_write_command(partition_t *part, const char *name, const char *content);
int create_hard_link(partition_t *part, const char *target_path, const char *link_path);
int delete_recursive(partition_t *part, const char *path);
#endif // FILE_OPERATION_H


