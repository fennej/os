#ifndef DIRECTORY_H
#define DIRECTORY_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "block.h"
#include "user.h"
#include "file_function.h"
// Structure pour stocker les composants d'un chemin


int change_directory(partition_t *part, const char *name);
void list_directory(partition_t *part);
void print_current_path(partition_t *part);
int add_dir_entry(partition_t *part, int dir_inode, const char *name, int inode_num);
int remove_dir_entry(partition_t *part, int dir_inode, const char *name);
void split_path(const char *path, path_components_t *components);
int resolve_path(partition_t *part, const char *path, int *parent_inode);





#endif // DIRECTORY_H