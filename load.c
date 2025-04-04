#include "load.h"

extern partition_t *global_partition = NULL;

// Fonction pour sauvegarder l'état de la partition dans un fichier
int save_partition(partition_t *part, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Erreur: Impossible d'ouvrir le fichier '%s' pour ecriture\n", filename);
        return -1;
    }

    // Écrire les données de la partition
    // D'abord la structure du superblock
    fwrite(part->superblock, sizeof(superblock_t), 1, file);
    
    // Ensuite la bitmap des blocs
    fwrite(part->block_bitmap, sizeof(block_bitmap_t), 1, file);
    
    // La bitmap des inodes
    fwrite(part->inode_bitmap, sizeof(inode_bitmap_t), 1, file);
    
    // La table d'inodes complète
    fwrite(part->inodes, sizeof(inode_t), MAX_INODES, file);
    
    // Les données utilisateur (l'espace complet)
    fwrite(part->space->data, sizeof(char), PARTITION_SIZE, file);
    
    // Informations supplémentaires sur l'état courant
    fwrite(&part->current_dir_inode, sizeof(int), 1, file);
    fwrite(&part->current_user, sizeof(user_t), 1, file);
    
    fclose(file);
    printf("Partition sauvegardée avec succès dans '%s'\n", filename);
    return 0;
}

// Fonction pour charger l'état d'une partition depuis un fichier
int load_partition(partition_t *part, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Erreur: Impossible d'ouvrir le fichier '%s' pour lecture\n", filename);
        return -1;
    }
    
    // Vérifier si la partition a déjà été allouée
    if (part->space == NULL) {
        // Si non, allouer l'espace pour la partition
        part->space = (espace_utilisable_t*)malloc(sizeof(espace_utilisable_t));
        if (part->space == NULL) {
            printf("Erreur: Impossible d'allouer de la mémoire pour la partition\n");
            fclose(file);
            return -1;
        }
        memset(part->space, 0, sizeof(espace_utilisable_t));
    }
    
    // Allouer temporairement la mémoire pour le superblock
    superblock_t *temp_superblock = (superblock_t*)malloc(sizeof(superblock_t));
    if (temp_superblock == NULL) {
        printf("Erreur: Allocation mémoire échouée\n");
        fclose(file);
        return -1;
    }
    
    // Lire le superblock
    if (fread(temp_superblock, sizeof(superblock_t), 1, file) != 1) {
        printf("Erreur: Lecture du superblock échouée\n");
        free(temp_superblock);
        fclose(file);
        return -1;
    }
    
    // Vérifier le numéro magique pour s'assurer qu'il s'agit d'un fichier de partition valide
    if (temp_superblock->magic != 0x12345678) {
        printf("Erreur: Format de fichier de partition invalide\n");
        free(temp_superblock);
        fclose(file);
        return -1;
    }
    
    // Maintenant, initialiser les pointeurs dans part->space->data
    part->superblock = (superblock_t*)(part->space->data + SUPERBLOCK_OFSET * BLOCK_SIZE);
    part->block_bitmap = (block_bitmap_t*)(part->space->data + BLOCKB_OFSET * BLOCK_SIZE);
    part->inode_bitmap = (inode_bitmap_t*)(part->space->data + INODEB_OFSET * BLOCK_SIZE);
    part->inodes = (inode_t*)(part->space->data + INODE_OFSET * BLOCK_SIZE);
    
    // Copier les données du superblock temporaire
    memcpy(part->superblock, temp_superblock, sizeof(superblock_t));
    free(temp_superblock);
    
    // Lire la bitmap des blocs
    if (fread(part->block_bitmap, sizeof(block_bitmap_t), 1, file) != 1) {
        printf("Erreur: Lecture de la bitmap des blocs échouée\n");
        fclose(file);
        return -1;
    }
    
    // Lire la bitmap des inodes
    if (fread(part->inode_bitmap, sizeof(inode_bitmap_t), 1, file) != 1) {
        printf("Erreur: Lecture de la bitmap des inodes échouée\n");
        fclose(file);
        return -1;
    }
    
    // Lire la table d'inodes
    if (fread(part->inodes, sizeof(inode_t), MAX_INODES, file) != MAX_INODES) {
        printf("Erreur: Lecture de la table d'inodes échouée\n");
        fclose(file);
        return -1;
    }
    
    // Lire les données de l'espace utilisable
    if (fread(part->space->data, sizeof(char), PARTITION_SIZE, file) != PARTITION_SIZE) {
        printf("Erreur: Lecture des données de la partition échouée\n");
        fclose(file);
        return -1;
    }
    
    // Lire l'inode du répertoire courant
    if (fread(&part->current_dir_inode, sizeof(int), 1, file) != 1) {
        printf("Erreur: Lecture de l'inode du répertoire courant échouée\n");
        fclose(file);
        return -1;
    }
    
    // Lire les informations de l'utilisateur courant
    if (fread(&part->current_user, sizeof(user_t), 1, file) != 1) {
        printf("Erreur: Lecture des informations de l'utilisateur courant échouée\n");
        fclose(file);
        return -1;
    }
    
    fclose(file);
    printf("Partition chargée avec succès depuis '%s'\n", filename);
    return 0;
}

// Fonction pour allouer et initialiser une nouvelle partition (helper)
partition_t* create_new_partition() {
    partition_t *part = (partition_t*)malloc(sizeof(partition_t));
    if (part == NULL) {
        printf("Erreur: Impossible d'allouer de la mémoire pour la partition\n");
        return NULL;
    }
    
    // Allouer l'espace utilisable
    part->space = (espace_utilisable_t*)malloc(sizeof(espace_utilisable_t));
    if (part->space == NULL) {
        printf("Erreur: Impossible d'allouer de la mémoire pour l'espace utilisable\n");
        free(part);
        return NULL;
    }
    
    // Initialiser à zéro
    memset(part->space, 0, sizeof(espace_utilisable_t));
    
    // Initialiser la partition
    init_partition(part);
    
    return part;
}

// Fonction pour libérer la mémoire d'une partition (helper)
void free_partition(partition_t *part) {
    if (part != NULL) {
        if (part->space != NULL) {
            free(part->space);
        }
        free(part);
    }
}

void sigint_handler(int sig) {
    char choice;
    char filename[256];
    
    // Ignorer temporairement SIGINT pour éviter une interruption pendant le prompt
    signal(SIGINT, SIG_IGN);
    
    printf("\n\nInterruption détectée. Souhaitez-vous sauvegarder avant de quitter ? (O/N): ");
    fflush(stdout);
    
    // Lire la réponse
    if (scanf(" %c", &choice) == 1) {
        if (choice == 'O' || choice == 'o') {
            printf("Nom du fichier pour la sauvegarde: ");
            fflush(stdout);
            
            if (scanf("%255s", filename) == 1) {
                if (global_partition != NULL) {
                    if (save_partition(global_partition, filename) == 0) {
                        printf("Sauvegarde réussie, fermeture en cours...\n");
                    } else {
                        printf("Échec de la sauvegarde, fermeture en cours...\n");
                    }
                } else {
                    printf("Aucune partition active à sauvegarder, fermeture en cours...\n");
                }
            }
        } else {
            printf("Fermeture sans sauvegarde...\n");
        }
    }
    
    // Terminer le programme
    exit(0);
}