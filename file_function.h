#ifndef FILE_H
#define FILE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "block.h"
#include "inode.h"
#include "user.h"
#include "directory.h"
int create_file(partition_t *part, const char *name, int mode);
int find_file_in_dir(partition_t *part, int dir_inode, const char *name);
int create_symlink(partition_t *part, const char *link_name, const char *target_name);
int delete_file(partition_t *part, const char *name);
int chmod_file(partition_t *part, const char *name, int mode);
int chown_file(partition_t *part, const char *name, int uid, int gid);
int resolve_symlink(partition_t *part, int inode_num);
void extract_filename(const char *path, char *filename);
int copy_file(partition_t *part, const char *source, const char *destination);
int move_file_with_paths(partition_t *part, const char *source_path, const char *dest_path);
#endif // FILE_H