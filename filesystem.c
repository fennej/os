#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "filesystem.h"


// Fonction pour initialiser la partition
void init_partition(partition_t *part) {
    // Initialiser le superbloc
    part->superblock.magic = 0xEF53;  // Comme ext2
    part->superblock.num_blocks = MAX_BLOCKS;
    part->superblock.num_inodes = MAX_INODES;
    part->superblock.first_data_block = 1;  // Le bloc 0 est réservé au superbloc
    part->superblock.block_size = BLOCK_SIZE;
    part->superblock.inode_size = sizeof(inode_t);
    part->superblock.blocks_per_group = MAX_BLOCKS;
    part->superblock.inodes_per_group = MAX_INODES;
    part->superblock.free_blocks_count = MAX_BLOCKS - 1;  // -1 pour le superbloc
    part->superblock.free_inodes_count = MAX_INODES - 1;  // -1 pour l'inode racine
    
    // Initialiser les bitmaps
    memset(part->block_bitmap.bitmap, 0, sizeof(part->block_bitmap.bitmap));
    memset(part->inode_bitmap.bitmap, 0, sizeof(part->inode_bitmap.bitmap));
    
    // Marquer le bloc 0 comme utilisé (superbloc)
    part->block_bitmap.bitmap[0] |= 1;
    
    // Initialiser les inodes
    memset(part->inodes, 0, sizeof(part->inodes));
    
    // Initialiser l'utilisateur courant (root par défaut)
    part->current_user.id = 0;
    strcpy(part->current_user.name, "root");
    part->current_user.group_id = 0;
    
    // Initialiser les données
    memset(part->data, 0, PARTITION_SIZE);
    
    // Créer le répertoire racine (inode 0)
    int root_inode = 0;
    part->inodes[root_inode].mode = 040755;  // drwxr-xr-x
    part->inodes[root_inode].uid = 0;
    part->inodes[root_inode].gid = 0;
    part->inodes[root_inode].size = 2 * sizeof(dir_entry_t);  // . et ..
    part->inodes[root_inode].atime = time(NULL);
    part->inodes[root_inode].mtime = time(NULL);
    part->inodes[root_inode].ctime = time(NULL);
    part->inodes[root_inode].links_count = 2;  // . et ..
    
    // Allouer un bloc pour le contenu du répertoire racine
    int root_block = allocate_block(part);
    part->inodes[root_inode].direct_blocks[0] = root_block;
    
    // Initialiser le répertoire racine
    dir_entry_t *root_dir = (dir_entry_t *)(part->data + root_block * BLOCK_SIZE);
    root_dir[0].inode_num = root_inode;  // .
    strcpy(root_dir[0].name, ".");
    root_dir[1].inode_num = root_inode;  // ..
    strcpy(root_dir[1].name, "..");
    
    // Marquer l'inode racine comme utilisé
    part->inode_bitmap.bitmap[0] |= 1;
    part->superblock.free_inodes_count--;
    
    // Définir le répertoire courant à la racine
    part->current_dir_inode = root_inode;
}