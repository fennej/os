#ifndef FOLDER_OPERATION_H
#define FOLDER_OPERATION_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "structure.h"
#include "load.h"
#include "block.h" 
#include "permission.h"
#include "file_operation.h"
#include "permission.h"


int change_directory(partition_t *part, const char *name);
void list_directory(partition_t *part,char* parem);
void print_current_path(partition_t *part);
int add_dir_entry(partition_t *part, int dir_inode, const char *name, int inode_num);
int remove_dir_entry(partition_t *part, int dir_inode, const char *name);
int resolve_path(partition_t *part, const char *path, int *parent_inode);
void split_path(const char *path, path_components_t *components);
int write_to_file(partition_t *part, const char *name, const char *data, int size);

int move_file_with_paths(partition_t *part, const char *source_path, const char *dest_path,int mode);
void extract_filename(const char *path, char *filename);

#endif // FOLDER_OPERATION_H


