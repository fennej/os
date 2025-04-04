#include "block.h"



// Fonction pour libérer un bloc
void free_block(partition_t *part, int block_num) {
    int byte_index = block_num / 8;
    int bit_index = block_num % 8;
    
    // Marquer le bloc comme libre
    part->block_bitmap->bitmap[byte_index] &= ~(1 << bit_index);
    part->superblock->free_blocks_count++;
    
    // Effacer le contenu du bloc
    memset(part->space->data + block_num * BLOCK_SIZE, 0, BLOCK_SIZE);
}


// Fonction pour allouer un bloc
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

