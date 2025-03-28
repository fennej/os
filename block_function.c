#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "block_function.h"

// Fonction pour allouer un bloc
int allocate_block(partition_t *part) {
    // Chercher un bloc libre
    for (int i = 0; i < MAX_BLOCKS; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        if (!(part->block_bitmap.bitmap[byte_index] & (1 << bit_index))) {
            // Marquer le bloc comme utilisé
            part->block_bitmap.bitmap[byte_index] |= (1 << bit_index);
            part->superblock.free_blocks_count--;
            return i;
        }
    }
    return -1;  // Pas de bloc libre
}


// Fonction pour libérer un bloc
void free_block(partition_t *part, int block_num) {
    int byte_index = block_num / 8;
    int bit_index = block_num % 8;
    
    // Marquer le bloc comme libre
    part->block_bitmap.bitmap[byte_index] &= ~(1 << bit_index);
    part->superblock.free_blocks_count++;
    
    // Effacer le contenu du bloc
    memset(part->data + block_num * BLOCK_SIZE, 0, BLOCK_SIZE);
}
