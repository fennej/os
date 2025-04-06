/**

 * @file block.c
 * @brief Gestion des blocs dans une partition.
 
 * participation: Mestar samy:50% Tighilt idir:50%
 * Ce fichier contient les fonctions permettant d'allouer et de libérer des blocs
 * dans une partition, ainsi que la gestion des bitmaps associés.
 */

#include "block.h"



/**
 * @brief Libère un bloc dans la partition.
 * 
 * Cette fonction marque un bloc comme libre dans le bitmap des blocs, réinitialise
 * son contenu à zéro, et met à jour le nombre de blocs libres dans le superbloc.
 * 
 * @param part Pointeur vers la partition contenant le bloc à libérer.
 * @param block_num Numéro du bloc à libérer.
 */
void free_block(partition_t *part, int block_num) {
    int byte_index = block_num / 8;
    int bit_index = block_num % 8;
    
    // Marquer le bloc comme libre
    part->block_bitmap->bitmap[byte_index] &= ~(1 << bit_index);
    part->superblock->free_blocks_count++;
    
    // Effacer le contenu du bloc
    memset(part->space->data + block_num * BLOCK_SIZE, 0, BLOCK_SIZE);
}


/**
 * @brief Alloue un bloc dans la partition.
 * 
 * Cette fonction recherche un bloc libre dans la partition, le marque comme
 * utilisé dans le bitmap des blocs, initialise son contenu à zéro, et met à jour
 * le nombre de blocs libres dans le superbloc. La fonction commence la recherche
 * à partir de l'offset réservé aux utilisateurs.
 * 
 * @param part Pointeur vers la partition où allouer le bloc.
 * @return int Le numéro logique du bloc alloué, ou -1 si aucun bloc libre n'est
 *         disponible.
 */
int allocate_block(partition_t *part) {
    // Start search from USERSAPCE_OFSET but return logical block numbers
    for (int i = USERSAPCE_OFSET; i < MAX_BLOCKS; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        if (!(part->block_bitmap->bitmap[byte_index] & (1 << bit_index))) {
            part->block_bitmap->bitmap[byte_index] |= (1 << bit_index);
            part->superblock->free_blocks_count--;
            
            // Initialize block to zero
            memset(part->space->data + i * BLOCK_SIZE, 0, BLOCK_SIZE);
            
            return i - USERSAPCE_OFSET;  // Return logical block number
        }
    }
    return -1;
}

