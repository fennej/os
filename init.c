/**
 * @file init.c
 * @brief Initialisation de la partition et de ses composants.
 *
 * participation: Mestar sami:50% Tighilt idir:50%
 * Ce fichier contient des fonctions pour initialiser une partition avec ses
 * composants de base, comme le superbloc, les bitmaps, la table d'inodes et
 * les répertoires de base.
 */

#include "init.h"

/**
 * @brief Initialise les structures d'une partition.
 * 
 * Cette fonction initialise les éléments essentiels d'une partition : le superbloc,
 * les bitmaps des blocs et des inodes, la table des inodes, ainsi que les entrées
 * de répertoire de base (comme le répertoire racine). Elle configure également les
 * attributs de la partition, tels que l'utilisateur actuel et le répertoire courant.
 * 
 * @param part Pointeur vers la partition à initialiser.
 */
void init_partition(partition_t *part) {


    // Initialiser les pointeurs vers les zones dans part->space->data
    part->superblock = (superblock_t*)(part->space->data + SUPERBLOCK_OFSET * BLOCK_SIZE);
    
    // Initialiser le bitmap des blocs
    part->block_bitmap = (block_bitmap_t*)(part->space->data + BLOCKB_OFSET* BLOCK_SIZE );
    

    // Initialiser le bitmap des inodes
    part->inode_bitmap = (inode_bitmap_t*)(part->space->data + INODEB_OFSET * BLOCK_SIZE);
    
    // Initialiser la table d'inodes
    part->inodes = (inode_t*)(part->space->data + INODE_OFSET * BLOCK_SIZE);
    memset(part->inodes, 0, sizeof(inode_t) * MAX_INODES); // S'assurer que la mémoire est initialisée
    
    // Initialiser d'autres éléments de la partition
    part->current_dir_inode = 0; // Root directory
    
    // Configurer le superbloc
    part->superblock->magic = 0x12345678;
    part->superblock->num_blocks = MAX_BLOCKS;
    part->superblock->num_inodes = MAX_INODES;
    part->superblock->first_data_block = USERSAPCE_OFSET;
    part->superblock->block_size = BLOCK_SIZE;
    part->superblock->inode_size = sizeof(inode_t);
    part->superblock->blocks_per_group = MAX_BLOCKS;
    part->superblock->inodes_per_group = MAX_INODES;
    part->superblock->free_blocks_count = MAX_BLOCKS - USERSAPCE_OFSET;
    part->superblock->free_inodes_count = MAX_INODES - 1; // Le premier inode est réservé pour le répertoire racine
    
    // Initialiser les bitmaps - utiliser des offsets directs pour l'initialisation
    unsigned char* block_bitmap_data = (unsigned char*)(part->space->data + BLOCKB_OFSET * BLOCK_SIZE);
    unsigned char* inode_bitmap_data = (unsigned char*)(part->space->data + INODEB_OFSET * BLOCK_SIZE);
    
    memset(block_bitmap_data, 0, MAX_BLOCKS / 8 + 1);
    memset(inode_bitmap_data, 0, MAX_INODES / 8 + 1);
    
    // Marquer les blocs système comme utilisés
    for (int i = 0; i < USERSAPCE_OFSET; i++) {
        // Marquer le bloc comme utilisé dans le bitmap
        block_bitmap_data[i / 8] |= (1 << (i % 8));
    }
    
    // Créer le répertoire racine (inode 0)
    inode_bitmap_data[0 / 8] |= (1 << (0 % 8)); // Marquer l'inode 0 comme utilisé

    // Initialiser l'utilisateur courant (root par défaut)
    part->current_user.id = 0;
    strcpy(part->current_user.name, "root");
    part->current_user.group_id = 0;
    
    // Initialiser l'inode du répertoire racine
    part->inodes[0].mode = 0040755; // Répertoire avec permission rwxr-xr-x
    part->inodes[0].uid = 0; // root
    part->inodes[0].gid = 0; // root
    part->inodes[0].size = 2 * sizeof(dir_entry_t); // "." et ".."
    part->inodes[0].ctime = part->inodes[0].mtime = part->inodes[0].atime = time(NULL);
    part->inodes[0].links_count = 2;  // . et ..

    // Allouer un bloc pour le répertoire racine (simulé ici)
    int root_block = USERSAPCE_OFSET; // Premier bloc disponible
    block_bitmap_data[root_block / 8] |= (1 << (root_block % 8)); // Marquer comme utilisé
    
    part->inodes[0].direct_blocks[0] = root_block;
    
    // Initialiser les entrées de répertoire "." et ".."
    dir_entry_t entries[2];
    entries[0].inode_num = 0;
    strcpy(entries[0].name, ".");
    entries[1].inode_num = 0;
    strcpy(entries[1].name, "..");
    
    // Écrire les entrées dans le bloc du répertoire racine
    memcpy(part->space->data + root_block * BLOCK_SIZE, entries, sizeof(entries));


    part->inode_bitmap->bitmap[0] |= 1;
    
    // Définir le répertoire courant à la racine
    part->current_dir_inode = 0;

}
