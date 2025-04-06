/**
 * @file inode.c
 * @brief Gestion des inodes : allocation et libération.
 *
 * Ce fichier contient les fonctions nécessaires pour allouer et libérer les inodes
 * dans une partition simulée.
 */
#include "inode.h"


/**
 * @brief Alloue un inode libre dans la partition.
 * 
 * Cherche un inode libre dans la bitmap, le marque comme utilisé,
 * initialise sa structure et met à jour le superbloc.
 * 
 * @param part Pointeur vers la partition où allouer l'inode.
 * @return L'indice de l'inode alloué en cas de succès, -1 en cas d'échec (plus d'inodes disponibles).
 */
int allocate_inode(partition_t *part) {
    // Chercher un inode libre
    for (int i = 0; i < MAX_INODES; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        if (!(part->inode_bitmap->bitmap[byte_index] & (1 << bit_index))) {
            // Marquer l'inode comme utilisé
            part->inode_bitmap->bitmap[byte_index] |= (1 << bit_index);
            part->superblock->free_inodes_count--;
            
            // Initialiser l'inode
            memset(&part->inodes[i], 0, sizeof(inode_t));
            for (int j = 0; j < NUM_DIRECT_BLOCKS; j++) {
                part->inodes[i].direct_blocks[j] = -1;
            }
            part->inodes[i].indirect_block = -1;
            part->inodes[i].ctime = time(NULL);
            part->inodes[i].atime = time(NULL);
            part->inodes[i].mtime = time(NULL);
            
            return i;
        }
    }
    return -1;  // Pas d'inode libre
}


/**
 * @brief Libère un inode et tous les blocs associés dans la partition.
 * 
 * Marque l'inode comme libre dans la bitmap, libère les blocs de données directs
 * ainsi que le bloc indirect s'il existe, et réinitialise la structure de l'inode.
 * 
 * @param part Pointeur vers la partition contenant l'inode.
 * @param inode_num Numéro (indice) de l'inode à libérer.
 */
void free_inode(partition_t *part, int inode_num) {
    int byte_index = inode_num / 8;
    int bit_index = inode_num % 8;
    
    // Libérer tous les blocs associés à l'inode
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        if (part->inodes[inode_num].direct_blocks[i] != -1) {
            free_block(part, part->inodes[inode_num].direct_blocks[i]);
        }
    }
    
    // Libérer le bloc indirect si présent
    if (part->inodes[inode_num].indirect_block != -1) {
        int *indirect_table = (int *)(part->space->data + part->inodes[inode_num].indirect_block * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            if (indirect_table[i] != -1) {
                free_block(part, indirect_table[i]);
            }
        }
        free_block(part, part->inodes[inode_num].indirect_block);
    }
    
    // Marquer l'inode comme libre
    part->inode_bitmap->bitmap[byte_index] &= ~(1 << bit_index);
    part->superblock->free_inodes_count++;
    
    // Réinitialiser l'inode
    memset(&part->inodes[inode_num], 0, sizeof(inode_t));
}
