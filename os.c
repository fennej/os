#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#define BLOCK_SIZE 512
#define MAX_NAME_LENGTH 32
#define MAX_BLOCKS 1024
#define MAX_INODES 100
#define NUM_DIRECT_BLOCKS 12  // Nombre de blocs directs par inode (comme dans Unix)
#define INDIRECT_BLOCKS 1     // Nombre de blocs indirects par inode
#define PARTITION_SIZE (BLOCK_SIZE * MAX_BLOCKS)

// Types d'utilisateurs
#define USER_OWNER 0
#define USER_GROUP 1
#define USER_OTHER 2

// Structure pour la carte des blocs (bitmap)
typedef struct {
    unsigned char bitmap[MAX_BLOCKS / 8 + 1];  // Chaque bit représente un bloc
} block_bitmap_t;

// Structure pour la carte des inodes (bitmap)
typedef struct {
    unsigned char bitmap[MAX_INODES / 8 + 1];  // Chaque bit représente un inode
} inode_bitmap_t;

// Structure pour un inode
typedef struct {
    int mode;                // Type et permissions (format style UNIX)
    int uid;                 // ID utilisateur
    int gid;                 // ID groupe
    int size;                // Taille en octets
    time_t atime;            // Temps d'accès
    time_t mtime;            // Temps de modification
    time_t ctime;            // Temps de création
    int direct_blocks[NUM_DIRECT_BLOCKS];  // Blocs directs
    int indirect_block;      // Bloc indirect
    int links_count;         // Nombre de liens
} inode_t;

// Structure pour une entrée de répertoire
typedef struct {
    int inode_num;           // Numéro d'inode
    char name[MAX_NAME_LENGTH];  // Nom du fichier/répertoire
} dir_entry_t;

// Structure pour représenter un utilisateur
typedef struct {
    int id;
    char name[MAX_NAME_LENGTH];
    int group_id;
} user_t;

// Structure pour le superbloc
typedef struct {
    int magic;               // Numéro magique pour identifier le système de fichiers
    int num_blocks;          // Nombre total de blocs
    int num_inodes;          // Nombre total d'inodes
    int first_data_block;    // Premier bloc de données
    int block_size;          // Taille d'un bloc
    int inode_size;          // Taille d'un inode
    int blocks_per_group;    // Blocs par groupe
    int inodes_per_group;    // Inodes par groupe
    int free_blocks_count;   // Nombre de blocs libres
    int free_inodes_count;   // Nombre d'inodes libres
} superblock_t;

// Structure pour représenter la partition
typedef struct {
    superblock_t superblock;         // Superbloc
    block_bitmap_t block_bitmap;     // Bitmap des blocs
    inode_bitmap_t inode_bitmap;     // Bitmap des inodes
    inode_t inodes[MAX_INODES];      // Table d'inodes
    char data[PARTITION_SIZE];       // Données stockées
    int current_dir_inode;           // Inode du répertoire courant
    user_t current_user;             // Utilisateur courant
} partition_t;

// Prototypes de fonctions
void init_partition(partition_t *part);
int allocate_block(partition_t *part);
void free_block(partition_t *part, int block_num);
int allocate_inode(partition_t *part);
void free_inode(partition_t *part, int inode_num);
int check_permission(partition_t *part, int inode_num, int perm_bit);
int create_file(partition_t *part, const char *name, int mode);
int find_file_in_dir(partition_t *part, int dir_inode, const char *name);
int create_symlink(partition_t *part, const char *link_name, const char *target_name);
int delete_file(partition_t *part, const char *name);
int change_directory(partition_t *part, const char *name);
void list_directory(partition_t *part);
int chmod_file(partition_t *part, const char *name, int mode);
int chown_file(partition_t *part, const char *name, int uid, int gid);
int switch_user(partition_t *part, int uid, int gid);
void print_current_path(partition_t *part);

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

// Fonction pour allouer un inode
int allocate_inode(partition_t *part) {
    // Chercher un inode libre
    for (int i = 0; i < MAX_INODES; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        if (!(part->inode_bitmap.bitmap[byte_index] & (1 << bit_index))) {
            // Marquer l'inode comme utilisé
            part->inode_bitmap.bitmap[byte_index] |= (1 << bit_index);
            part->superblock.free_inodes_count--;
            
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

// Fonction pour libérer un inode
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
        int *indirect_table = (int *)(part->data + part->inodes[inode_num].indirect_block * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            if (indirect_table[i] != -1) {
                free_block(part, indirect_table[i]);
            }
        }
        free_block(part, part->inodes[inode_num].indirect_block);
    }
    
    // Marquer l'inode comme libre
    part->inode_bitmap.bitmap[byte_index] &= ~(1 << bit_index);
    part->superblock.free_inodes_count++;
    
    // Réinitialiser l'inode
    memset(&part->inodes[inode_num], 0, sizeof(inode_t));
}

// Fonction pour vérifier les permissions
int check_permission(partition_t *part, int inode_num, int perm_bit) {
    // Si c'est root, tout est permis
    if (part->current_user.id == 0) {
        return 1;
    }
    
    int mode = part->inodes[inode_num].mode;
    int user_type;
    
    // Déterminer le type d'utilisateur (propriétaire, groupe, autre)
    if (part->current_user.id == part->inodes[inode_num].uid) {
        user_type = USER_OWNER;
    } else if (part->current_user.group_id == part->inodes[inode_num].gid) {
        user_type = USER_GROUP;
    } else {
        user_type = USER_OTHER;
    }
    
    // Vérifier les permissions
    int permission_mask = 0;
    switch (user_type) {
        case USER_OWNER:
            permission_mask = (mode & 0700) >> 6;
            break;
        case USER_GROUP:
            permission_mask = (mode & 0070) >> 3;
            break;
        case USER_OTHER:
            permission_mask = mode & 0007;
            break;
    }
    
    return (permission_mask & perm_bit) != 0;
}

// Fonction pour trouver un fichier dans un répertoire
int find_file_in_dir(partition_t *part, int dir_inode, const char *name) {
    if (!(part->inodes[dir_inode].mode & 040000)) {  // Vérifier si c'est un répertoire
        return -1;
    }
    
    // Parcourir tous les blocs du répertoire
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[dir_inode].direct_blocks[i];
        if (block_num == -1) continue;
        
        dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int j = 0; j < num_entries; j++) {
            if (dir_entries[j].inode_num != 0 && strcmp(dir_entries[j].name, name) == 0) {
                return dir_entries[j].inode_num;
            }
        }
    }
    
    // Vérifier le bloc indirect
    if (part->inodes[dir_inode].indirect_block != -1) {
        int *indirect_table = (int *)(part->data + part->inodes[dir_inode].indirect_block * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
          if (indirect_table[i] != -1) {
                dir_entry_t *dir_entries = (dir_entry_t *)(part->data + indirect_table[i] * BLOCK_SIZE);
                int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
                
                for (int j = 0; j < num_entries; j++) {
                    if (dir_entries[j].inode_num != 0 && strcmp(dir_entries[j].name, name) == 0) {
                        return dir_entries[j].inode_num;
                    }
                }
            }
        }
    }
    
    return -1;  // Fichier non trouvé
}

// Fonction pour ajouter une entrée dans un répertoire
int add_dir_entry(partition_t *part, int dir_inode, const char *name, int inode_num) {
    if (!(part->inodes[dir_inode].mode & 040000)) {  // Vérifier si c'est un répertoire
        return -1;
    }
    
    // Parcourir tous les blocs du répertoire pour trouver une place libre
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[dir_inode].direct_blocks[i];
        if (block_num == -1) {
            // Allouer un nouveau bloc pour le répertoire
            block_num = allocate_block(part);
            if (block_num == -1) return -1;  // Plus de bloc disponible
            
            part->inodes[dir_inode].direct_blocks[i] = block_num;
            
            // Initialiser le bloc avec des entrées vides
            dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            for (int j = 0; j < num_entries; j++) {
                dir_entries[j].inode_num = 0;
                memset(dir_entries[j].name, 0, MAX_NAME_LENGTH);
            }
        }
        
        // Chercher une entrée libre dans le bloc
        dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int j = 0; j < num_entries; j++) {
            if (dir_entries[j].inode_num == 0) {
                // Entrée libre trouvée
                dir_entries[j].inode_num = inode_num;
                strncpy(dir_entries[j].name, name, MAX_NAME_LENGTH - 1);
                dir_entries[j].name[MAX_NAME_LENGTH - 1] = '\0';
                
                // Mettre à jour la taille du répertoire
                part->inodes[dir_inode].size += sizeof(dir_entry_t);
                part->inodes[dir_inode].mtime = time(NULL);
                
                return 0;
            }
        }
    }
    
    // Si on arrive ici, c'est qu'il n'y a plus de place dans les blocs directs
    // On doit utiliser le bloc indirect
    if (part->inodes[dir_inode].indirect_block == -1) {
        // Allouer un bloc pour la table indirecte
        int indirect_block = allocate_block(part);
        if (indirect_block == -1) return -1;  // Plus de bloc disponible
        
        part->inodes[dir_inode].indirect_block = indirect_block;
        
        // Initialiser la table indirecte
        int *indirect_table = (int *)(part->data + indirect_block * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            indirect_table[i] = -1;
        }
    }
    
    // Utiliser le bloc indirect
    int *indirect_table = (int *)(part->data + part->inodes[dir_inode].indirect_block * BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
        if (indirect_table[i] == -1) {
            // Allouer un nouveau bloc pour le répertoire
            int block_num = allocate_block(part);
            if (block_num == -1) return -1;  // Plus de bloc disponible
            
            indirect_table[i] = block_num;
            
            // Initialiser le bloc avec des entrées vides
            dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            for (int j = 0; j < num_entries; j++) {
                dir_entries[j].inode_num = 0;
                memset(dir_entries[j].name, 0, MAX_NAME_LENGTH);
            }
            
            // Ajouter l'entrée dans le premier emplacement
            dir_entries[0].inode_num = inode_num;
            strncpy(dir_entries[0].name, name, MAX_NAME_LENGTH - 1);
            dir_entries[0].name[MAX_NAME_LENGTH - 1] = '\0';
            
            // Mettre à jour la taille du répertoire
            part->inodes[dir_inode].size += sizeof(dir_entry_t);
            part->inodes[dir_inode].mtime = time(NULL);
            
            return 0;
        } else {
            // Chercher une entrée libre dans le bloc
            dir_entry_t *dir_entries = (dir_entry_t *)(part->data + indirect_table[i] * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            
            for (int j = 0; j < num_entries; j++) {
                if (dir_entries[j].inode_num == 0) {
                    // Entrée libre trouvée
                    dir_entries[j].inode_num = inode_num;
                    strncpy(dir_entries[j].name, name, MAX_NAME_LENGTH - 1);
                    dir_entries[j].name[MAX_NAME_LENGTH - 1] = '\0';
                    
                    // Mettre à jour la taille du répertoire
                    part->inodes[dir_inode].size += sizeof(dir_entry_t);
                    part->inodes[dir_inode].mtime = time(NULL);
                    
                    return 0;
                }
            }
        }
    }
    
    return -1;  // Plus de place dans le répertoire
}

// Fonction pour supprimer une entrée d'un répertoire
int remove_dir_entry(partition_t *part, int dir_inode, const char *name) {
    if (!(part->inodes[dir_inode].mode & 040000)) {  // Vérifier si c'est un répertoire
        return -1;
    }
    
    // Parcourir tous les blocs du répertoire
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[dir_inode].direct_blocks[i];
        if (block_num == -1) continue;
        
        dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int j = 0; j < num_entries; j++) {
            if (dir_entries[j].inode_num != 0 && strcmp(dir_entries[j].name, name) == 0) {
                // Entrée trouvée, la supprimer
                dir_entries[j].inode_num = 0;
                memset(dir_entries[j].name, 0, MAX_NAME_LENGTH);
                
                // Mettre à jour la taille du répertoire
                part->inodes[dir_inode].size -= sizeof(dir_entry_t);
                part->inodes[dir_inode].mtime = time(NULL);
                
                return 0;
            }
        }
    }
    
    // Vérifier le bloc indirect
    if (part->inodes[dir_inode].indirect_block != -1) {
        int *indirect_table = (int *)(part->data + part->inodes[dir_inode].indirect_block * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            if (indirect_table[i] != -1) {
                dir_entry_t *dir_entries = (dir_entry_t *)(part->data + indirect_table[i] * BLOCK_SIZE);
                int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
                
                for (int j = 0; j < num_entries; j++) {
                    if (dir_entries[j].inode_num != 0 && strcmp(dir_entries[j].name, name) == 0) {
                        // Entrée trouvée, la supprimer
                        dir_entries[j].inode_num = 0;
                        memset(dir_entries[j].name, 0, MAX_NAME_LENGTH);
                        
                        // Mettre à jour la taille du répertoire
                        part->inodes[dir_inode].size -= sizeof(dir_entry_t);
                        part->inodes[dir_inode].mtime = time(NULL);
                        
                        return 0;
                    }
                }
            }
        }
    }
    
    return -1;  // Entrée non trouvée
}

// Fonction pour créer un fichier
int create_file(partition_t *part, const char *name, int mode) {
    // Vérifier les permissions du répertoire courant pour l'écriture (bit 2)
    if (!check_permission(part, part->current_dir_inode, 2)) {
        printf("Erreur: Permissions insuffisantes pour créer dans ce répertoire\n");
        return -1;
    }
    
    // Vérifier si le fichier existe déjà
    if (find_file_in_dir(part, part->current_dir_inode, name) != -1) {
        printf("Erreur: Un fichier avec ce nom existe déjà\n");
        return -1;
    }
    
    // Allouer un nouvel inode
    int inode_num = allocate_inode(part);
    if (inode_num == -1) {
        printf("Erreur: Plus d'inodes disponibles\n");
        return -1;
    }
    
    // Initialiser l'inode
    part->inodes[inode_num].mode = mode;
    part->inodes[inode_num].uid = part->current_user.id;
    part->inodes[inode_num].gid = part->current_user.group_id;
    part->inodes[inode_num].size = 0;
    part->inodes[inode_num].atime = time(NULL);
    part->inodes[inode_num].mtime = time(NULL);
    part->inodes[inode_num].ctime = time(NULL);
    
    // Si c'est un répertoire, initialiser le contenu
    if (mode & 040000) {
        // Allouer un bloc pour le contenu du répertoire
        int block_num = allocate_block(part);
        if (block_num == -1) {
            free_inode(part, inode_num);
            printf("Erreur: Plus de blocs disponibles\n");
            return -1;
        }
        
        part->inodes[inode_num].direct_blocks[0] = block_num;
        part->inodes[inode_num].size = 2 * sizeof(dir_entry_t);  // . et ..
        
        // Initialiser le contenu du répertoire
        dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
        
        // Entrée pour .
        dir_entries[0].inode_num = inode_num;
        strcpy(dir_entries[0].name, ".");
        
        // Entrée pour ..
        dir_entries[1].inode_num = part->current_dir_inode;
        strcpy(dir_entries[1].name, "..");
        
        // Initialiser le reste du bloc
        for (int i = 2; i < BLOCK_SIZE / sizeof(dir_entry_t); i++) {
            dir_entries[i].inode_num = 0;
            memset(dir_entries[i].name, 0, MAX_NAME_LENGTH);
        }
        
        // Mettre à jour le nombre de liens
        part->inodes[inode_num].links_count = 2;  // . et entry dans le parent
        part->inodes[part->current_dir_inode].links_count++;  // .. dans le nouveau répertoire
    } else {
        // Fichier ordinaire
        part->inodes[inode_num].links_count = 1;  // Un seul lien (l'entrée dans le répertoire parent)
    }
    
    // Ajouter l'entrée dans le répertoire courant
    if (add_dir_entry(part, part->current_dir_inode, name, inode_num) != 0) {
        free_inode(part, inode_num);
        printf("Erreur: Impossible d'ajouter l'entrée dans le répertoire\n");
        return -1;
    }
    
    printf("%s '%s' créé avec succès\n", (mode & 040000) ? "Répertoire" : "Fichier", name);
    return inode_num;
}

// Fonction pour créer un lien symbolique
int create_symlink(partition_t *part, const char *link_name, const char *target_name) {
    // Vérifier les permissions du répertoire courant pour l'écriture (bit 2)
    if (!check_permission(part, part->current_dir_inode, 2)) {
        printf("Erreur: Permissions insuffisantes pour créer dans ce répertoire\n");
        return -1;
    }
    
    // Vérifier si le nom du lien existe déjà
    if (find_file_in_dir(part, part->current_dir_inode, link_name) != -1) {
        printf("Erreur: Un fichier avec ce nom existe déjà\n");
        return -1;
    }
    
    // Vérifier si la cible existe
    int target_inode = find_file_in_dir(part, part->current_dir_inode, target_name);
    if (target_inode == -1) {
        printf("Erreur: Fichier cible '%s' non trouvé\n", target_name);
        return -1;
    }
    
    // Allouer un nouvel inode pour le lien symbolique
    int symlink_inode = allocate_inode(part);
    if (symlink_inode == -1) {
        printf("Erreur: Plus d'inodes disponibles\n");
        return -1;
    }
    
    // Initialiser l'inode du lien symbolique
    part->inodes[symlink_inode].mode = 0120777;  // lrwxrwxrwx
    part->inodes[symlink_inode].uid = part->current_user.id;
    part->inodes[symlink_inode].gid = part->current_user.group_id;
    part->inodes[symlink_inode].atime = time(NULL);
    part->inodes[symlink_inode].mtime = time(NULL);
    part->inodes[symlink_inode].ctime = time(NULL);
    part->inodes[symlink_inode].links_count = 1;
    
    // Stocker le chemin de la cible dans le bloc de données
    int data_block = allocate_block(part);
    if (data_block == -1) {
        free_inode(part, symlink_inode);
        printf("Erreur: Plus de blocs disponibles\n");
        return -1;
    }
    
    part->inodes[symlink_inode].direct_blocks[0] = data_block;
    part->inodes[symlink_inode].size = strlen(target_name) + 1;
    
    // Copier le nom de la cible dans le bloc de données
    strcpy(part->data + data_block * BLOCK_SIZE, target_name);
    
    // Ajouter l'entrée dans le répertoire courant
    if (add_dir_entry(part, part->current_dir_inode, link_name, symlink_inode) != 0) {
        free_block(part, data_block);
        free_inode(part, symlink_inode);
        printf("Erreur: Impossible d'ajouter l'entrée dans le répertoire\n");
        return -1;
    }
    
    printf("Lien symbolique '%s' vers '%s' créé avec succès\n", link_name, target_name);
    return symlink_inode;
}

// Fonction pour supprimer un fichier
int delete_file(partition_t *part, const char *name) {
    // Ne pas permettre la suppression de "." et ".."
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        printf("Erreur: Impossible de supprimer '%s'\n", name);
        return -1;
    }
    
    // Vérifier les permissions du répertoire parent pour l'écriture
    if (!check_permission(part, part->current_dir_inode, 2)) {
        printf("Erreur: Permissions insuffisantes pour supprimer depuis ce répertoire\n");
        return -1;
    }
    
    // Trouver le fichier dans le répertoire courant
    int inode_num = find_file_in_dir(part, part->current_dir_inode, name);
    if (inode_num == -1) {
        printf("Erreur: Fichier '%s' non trouvé\n", name);
        return -1;
    }
    
    // Vérifier si c'est un répertoire et s'il est vide
    if (part->inodes[inode_num].mode & 040000) {
        // Parcourir le contenu du répertoire pour voir s'il est vide
        for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
            int block_num = part->inodes[inode_num].direct_blocks[i];
            if (block_num == -1) continue;
            
            dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            
            for (int j = 0; j < num_entries; j++) {
                if (dir_entries[j].inode_num != 0 && 
                    strcmp(dir_entries[j].name, ".") != 0 && 
                    strcmp(dir_entries[j].name, "..") != 0) {
                    printf("Erreur: Le répertoire n'est pas vide\n");
                    return -1;
                }
            }
        }
        
        // Décréments le nombre de liens du répertoire parent (lien "..")
        part->inodes[part->current_dir_inode].links_count--;
    }
    
    // Décréments le nombre de liens
    part->inodes[inode_num].links_count--;
    
    // Si le nombre de liens atteint 0, supprimer le fichier
    if (part->inodes[inode_num].links_count == 0) {
        free_inode(part, inode_num);
    }
    
    // Supprimer l'entrée du répertoire
    remove_dir_entry(part, part->current_dir_inode, name);
    
    printf("%s '%s' supprimé avec succès\n", (part->inodes[inode_num].mode & 040000) ? "Répertoire" : "Fichier", name);
    return 0;
}

// Fonction pour résoudre un chemin symbolique
int resolve_symlink(partition_t *part, int inode_num) {
    int max_depth = 10;  // Limiter la profondeur pour éviter les boucles infinies
    
    for (int depth = 0; depth < max_depth; depth++) {
        if (!(part->inodes[inode_num].mode & 0120000)) {  // Pas un lien symbolique
            return inode_num;
        }
        
        int data_block = part->inodes[inode_num].direct_blocks[0];
        if (data_block == -1) {
            return -1;  // Lien corrompu
        }
        
        char target_name[MAX_NAME_LENGTH];
        strncpy(target_name, part->data + data_block * BLOCK_SIZE, MAX_NAME_LENGTH - 1);
        target_name[MAX_NAME_LENGTH - 1] = '\0';
        
        int target_inode = find_file_in_dir(part, part->current_dir_inode, target_name);
        if (target_inode == -1) {
            printf("Erreur: Cible du lien '%s' non trouvée\n", target_name);
            return -1;
        }
        
        inode_num = target_inode;
    }
    
    printf("Erreur: Trop de niveaux de liens symboliques\n");
    return -1;
}

// Fonction pour changer de répertoire
int change_directory(partition_t *part, const char *name) {
    // Cas spécial pour ".." (remonter d'un niveau)
    if (strcmp(name, "..") == 0) {
        // Trouver l'inode du parent
        int parent_inode = -1;
        
        // Chercher l'entrée ".." dans le répertoire courant
        for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
            int block_num = part->inodes[part->current_dir_inode].direct_blocks[i];
            if (block_num == -1) continue;
            
            dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            
            for (int j = 0; j < num_entries; j++) {
                if (dir_entries[j].inode_num != 0 && strcmp(dir_entries[j].name, "..") == 0) {
                    parent_inode = dir_entries[j].inode_num;
                    break;
                }
            }
            if (parent_inode != -1) break;
        }
        
        if (parent_inode == -1) {
            printf("Erreur: Impossible de trouver le répertoire parent\n");
            return -1;
        }
        
        // Vérifier les permissions pour accéder au répertoire parent
        if (!check_permission(part, parent_inode, 1)) {  // Besoin droit x
            printf("Erreur: Permissions insuffisantes pour accéder au répertoire parent\n");
            return -1;
        }
        
        part->current_dir_inode = parent_inode;
        printf("Changement vers le répertoire parent\n");
        return 0;
    }
    
    // Cas spécial pour "." (rester dans le même répertoire)
    if (strcmp(name, ".") == 0) {
        return 0;
    }
    
    // Cas spécial pour "/" (aller à la racine)
    if (strcmp(name, "/") == 0) {
        // Vérifier les permissions pour accéder à la racine
        if (!check_permission(part, 0, 1)) {  // Besoin droit x
            printf("Erreur: Permissions insuffisantes pour accéder à la racine\n");
            return -1;
        }
        
        part->current_dir_inode = 0;
        printf("Changement vers le répertoire racine\n");
        return 0;
    }
    
    // Chercher le fichier ou répertoire
    int inode_num = find_file_in_dir(part, part->current_dir_inode, name);
    if (inode_num == -1) {
        printf("Erreur: '%s' non trouvé\n", name);
        return -1;
    }
    
    // Si c'est un lien symbolique, le résoudre
    if (part->inodes[inode_num].mode & 0120000) {
        printf("Suivi du lien symbolique...\n");
        inode_num = resolve_symlink(part, inode_num);
        if (inode_num == -1) {
            return -1;
        }
    }
    
    // Vérifier si c'est un répertoire
    if (!(part->inodes[inode_num].mode & 040000)) {
        printf("Erreur: '%s' n'est pas un répertoire\n", name);
        return -1;
    }
    
    // Vérifier les permissions pour entrer dans le répertoire
    if (!check_permission(part, inode_num, 1)) {  // Besoin droit x
        printf("Erreur: Permissions insuffisantes pour accéder au répertoire '%s'\n", name);
        return -1;
    }
    
    // Mettre à jour le répertoire courant
    part->current_dir_inode = inode_num;
    printf("Changement vers le répertoire '%s'\n", name);
    
    // Mettre à jour le temps d'accès
    part->inodes[inode_num].atime = time(NULL);
    
    return 0;
}

// Fonction pour lister le contenu d'un répertoire
void list_directory(partition_t *part) {
    // Vérifier les permissions pour lire le répertoire (bit 4 = r)
    if (!check_permission(part, part->current_dir_inode, 4)) {
        printf("Erreur: Permissions insuffisantes pour lire le contenu de ce répertoire\n");
        return;
    }
    
    printf("Droits  Liens  Prop  Groupe  Taille       Date       Nom\n");
    
    // Parcourir tous les blocs du répertoire
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[part->current_dir_inode].direct_blocks[i];
        if (block_num == -1) continue;
        
        dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int j = 0; j < num_entries; j++) {
            if (dir_entries[j].inode_num == 0) continue;
            
            int file_inode = dir_entries[j].inode_num;
            char type_char = '-';
            
            // Déterminer le type de fichier
            if (part->inodes[file_inode].mode & 040000) type_char = 'd';  // Répertoire
            else if (part->inodes[file_inode].mode & 0120000) type_char = 'l';  // Lien symbolique
            
            // Afficher les permissions
            char perm_str[11];
            perm_str[0] = type_char;
            perm_str[1] = (part->inodes[file_inode].mode & 0400) ? 'r' : '-';
            perm_str[2] = (part->inodes[file_inode].mode & 0200) ? 'w' : '-';
            perm_str[3] = (part->inodes[file_inode].mode & 0100) ? 'x' : '-';
            perm_str[4] = (part->inodes[file_inode].mode & 0040) ? 'r' : '-';
            perm_str[5] = (part->inodes[file_inode].mode & 0020) ? 'w' : '-';
            perm_str[6] = (part->inodes[file_inode].mode & 0010) ? 'x' : '-';
            perm_str[7] = (part->inodes[file_inode].mode & 0004) ? 'r' : '-';
            perm_str[8] = (part->inodes[file_inode].mode & 0002) ? 'w' : '-';
            perm_str[9] = (part->inodes[file_inode].mode & 0001) ? 'x' : '-';
            perm_str[10] = '\0';
            
            // Formater la date
            char date_str[20];
            struct tm *tm_info = localtime(&part->inodes[file_inode].mtime);
            strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M", tm_info);
            
            // Afficher les informations du fichier
            printf("%s  %4d  %4d  %5d  %6d  %s  %s", 
                   perm_str, 
                   part->inodes[file_inode].links_count, 
                   part->inodes[file_inode].uid, 
                   part->inodes[file_inode].gid,
                   part->inodes[file_inode].size,
                   date_str,
                   dir_entries[j].name);
            
            // Si c'est un lien symbolique, afficher la cible
            if (part->inodes[file_inode].mode & 0120000) {
                int data_block = part->inodes[file_inode].direct_blocks[0];
                if (data_block != -1) {
                    printf(" -> %s", part->data + data_block * BLOCK_SIZE);
                }
            }
            
            printf("\n");
        }
    }
// Suite de la fonction list_directory (qui était coupée)
    if (part->inodes[part->current_dir_inode].indirect_block != -1) {
        int *indirect_table = (int *)(part->data + part->inodes[part->current_dir_inode].indirect_block * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            if (indirect_table[i] != -1) {
                dir_entry_t *dir_entries = (dir_entry_t *)(part->data + indirect_table[i] * BLOCK_SIZE);
                int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
                
                for (int j = 0; j < num_entries; j++) {
                    if (dir_entries[j].inode_num == 0) continue;
                    
                    int file_inode = dir_entries[j].inode_num;
                    char type_char = '-';
                    
                    // Déterminer le type de fichier
                    if (part->inodes[file_inode].mode & 040000) type_char = 'd';  // Répertoire
                    else if (part->inodes[file_inode].mode & 0120000) type_char = 'l';  // Lien symbolique
                    
                    // Afficher les permissions
                    char perm_str[11];
                    perm_str[0] = type_char;
                    perm_str[1] = (part->inodes[file_inode].mode & 0400) ? 'r' : '-';
                    perm_str[2] = (part->inodes[file_inode].mode & 0200) ? 'w' : '-';
                    perm_str[3] = (part->inodes[file_inode].mode & 0100) ? 'x' : '-';
                    perm_str[4] = (part->inodes[file_inode].mode & 0040) ? 'r' : '-';
                    perm_str[5] = (part->inodes[file_inode].mode & 0020) ? 'w' : '-';
                    perm_str[6] = (part->inodes[file_inode].mode & 0010) ? 'x' : '-';
                    perm_str[7] = (part->inodes[file_inode].mode & 0004) ? 'r' : '-';
                    perm_str[8] = (part->inodes[file_inode].mode & 0002) ? 'w' : '-';
                    perm_str[9] = (part->inodes[file_inode].mode & 0001) ? 'x' : '-';
                    perm_str[10] = '\0';
                    
                    // Formater la date
                    char date_str[20];
                    struct tm *tm_info = localtime(&part->inodes[file_inode].mtime);
                    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M", tm_info);
                    
                    // Afficher les informations du fichier
                    printf("%s  %4d  %4d  %5d  %6d  %s  %s", 
                        perm_str, 
                        part->inodes[file_inode].links_count, 
                        part->inodes[file_inode].uid, 
                        part->inodes[file_inode].gid,
                        part->inodes[file_inode].size,
                        date_str,
                        dir_entries[j].name);
                    
                    // Si c'est un lien symbolique, afficher la cible
                    if (part->inodes[file_inode].mode & 0120000) {
                        int data_block = part->inodes[file_inode].direct_blocks[0];
                        if (data_block != -1) {
                            printf(" -> %s", part->data + data_block * BLOCK_SIZE);
                        }
                    }
                    
                    printf("\n");
                }
            }
        }
    }
    
    // Mettre à jour le temps d'accès
    part->inodes[part->current_dir_inode].atime = time(NULL);
}

// Fonction pour changer les permissions d'un fichier (chmod)
int chmod_file(partition_t *part, const char *name, int mode) {
    // Trouver le fichier dans le répertoire courant
    int inode_num = find_file_in_dir(part, part->current_dir_inode, name);
    if (inode_num == -1) {
        printf("Erreur: Fichier '%s' non trouvé\n", name);
        return -1;
    }
    
    // Vérifier si l'utilisateur est le propriétaire du fichier ou root
    if (part->current_user.id != 0 && part->current_user.id != part->inodes[inode_num].uid) {
        printf("Erreur: Vous devez être le propriétaire du fichier pour changer ses permissions\n");
        return -1;
    }
    
    // Conserver les bits de type de fichier (les 4 premiers bits)
    int type_bits = part->inodes[inode_num].mode & 0170000;
    
    // Appliquer les nouvelles permissions (les 12 derniers bits)
    part->inodes[inode_num].mode = type_bits | (mode & 07777);
    
    // Mettre à jour le temps de modification
    part->inodes[inode_num].ctime = time(NULL);
    
    printf("Permissions modifiées pour '%s'\n", name);
    return 0;
}

// Fonction pour changer le propriétaire et le groupe d'un fichier (chown)
int chown_file(partition_t *part, const char *name, int uid, int gid) {
    // Seul root peut changer le propriétaire
    if (part->current_user.id != 0) {
        printf("Erreur: Seul root peut changer le propriétaire d'un fichier\n");
        return -1;
    }
    
    // Trouver le fichier dans le répertoire courant
    int inode_num = find_file_in_dir(part, part->current_dir_inode, name);
    if (inode_num == -1) {
        printf("Erreur: Fichier '%s' non trouvé\n", name);
        return -1;
    }
    
    // Changer le propriétaire et le groupe
    part->inodes[inode_num].uid = uid;
    part->inodes[inode_num].gid = gid;
    
    // Mettre à jour le temps de modification
    part->inodes[inode_num].ctime = time(NULL);
    
    printf("Propriétaire et groupe modifiés pour '%s'\n", name);
    return 0;
}

// Fonction pour changer d'utilisateur courant
int switch_user(partition_t *part, int uid, int gid) {
    // En pratique, il faudrait vérifier si l'utilisateur existe dans un fichier passwd
    // et demander un mot de passe, mais pour ce prototype, on le fait simplement
    
    // Mettre à jour l'utilisateur courant
    part->current_user.id = uid;
    sprintf(part->current_user.name, "user%d", uid);  // Nom générique
    part->current_user.group_id = gid;
    
    printf("Utilisateur changé: uid=%d, gid=%d\n", uid, gid);
    return 0;
}

// Fonction pour afficher le chemin courant
void print_current_path(partition_t *part) {
    // Tableau pour stocker les noms de répertoires (du courant à la racine)
    char path_components[MAX_INODES][MAX_NAME_LENGTH];
    int num_components = 0;
    
    int current_inode = part->current_dir_inode;
    
    // Si nous sommes à la racine, afficher simplement "/"
    if (current_inode == 0) {
        printf("/\n");
        return;
    }
    
    // Remonter le chemin jusqu'à la racine
    while (current_inode != 0) {
        // Trouver le parent de ce répertoire
        int parent_inode = -1;
        char current_name[MAX_NAME_LENGTH] = "";
        
        // Parcourir tous les blocs du parent pour trouver ce répertoire
        for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
            int block_num = part->inodes[current_inode].direct_blocks[i];
            if (block_num == -1) continue;
            
            dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            
            for (int j = 0; j < num_entries; j++) {
                if (dir_entries[j].inode_num != 0 && strcmp(dir_entries[j].name, "..") == 0) {
                    parent_inode = dir_entries[j].inode_num;
                    break;
                }
            }
            if (parent_inode != -1) break;
        }
        
        // Si nous ne trouvons pas le parent, arrêter
        if (parent_inode == -1) break;
        
        // Maintenant, dans le répertoire parent, trouver le nom du répertoire courant
        for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
            int block_num = part->inodes[parent_inode].direct_blocks[i];
            if (block_num == -1) continue;
            
            dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            
            for (int j = 0; j < num_entries; j++) {
                if (dir_entries[j].inode_num == current_inode && strcmp(dir_entries[j].name, ".") != 0 && strcmp(dir_entries[j].name, "..") != 0) {
                    strcpy(current_name, dir_entries[j].name);
                    break;
                }
            }
            if (strlen(current_name) > 0) break;
        }
        
        // Ajouter ce nom à notre liste
        if (strlen(current_name) > 0) {
            strcpy(path_components[num_components], current_name);
            num_components++;
        }
        
        // Passer au parent
        current_inode = parent_inode;
    }
    
    // Afficher le chemin complet
    printf("/");
    for (int i = num_components - 1; i >= 0; i--) {
        printf("%s", path_components[i]);
        if (i > 0) printf("/");
    }
    printf("\n");
}


int copy_file(partition_t *part, const char *source, const char *destination) {
    // Trouver le fichier source dans le répertoire courant
    int source_inode = find_file_in_dir(part, part->current_dir_inode, source);
    if (source_inode == -1) {
        printf("Erreur: Fichier source '%s' non trouvé\n", source);
        return -1;
    }
    
    // Vérifier si l'utilisateur a le droit de lire le fichier source
    if (!check_permission(part, source_inode, 4)) { // 4 = permission de lecture
        printf("Erreur: Permission de lecture refusée pour '%s'\n", source);
        return -1;
    }
    
    // Vérifier si le fichier source est un répertoire
    if (part->inodes[source_inode].mode & 040000) {
        printf("Erreur: La copie de répertoires n'est pas supportée\n");
        return -1;
    }
    
    // Vérifier si le fichier destination existe déjà
    int dest_inode = find_file_in_dir(part, part->current_dir_inode, destination);
    if (dest_inode != -1) {
        printf("Erreur: Le fichier destination '%s' existe déjà\n", destination);
        return -1;
    }
    
    // Créer le nouveau fichier avec les mêmes permissions que l'original
    int mode = part->inodes[source_inode].mode & 07777; // Garder seulement les bits de permission
    dest_inode = create_file(part, destination, 0100000 | mode); // 0100000 = fichier régulier
    
    if (dest_inode == -1) {
        printf("Erreur: Impossible de créer le fichier destination '%s'\n", destination);
        return -1;
    }
    
    // Copier les données du fichier source vers le fichier destination
    int source_size = part->inodes[source_inode].size;
    part->inodes[dest_inode].size = source_size;
    
    // Copier les blocs de données directs
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int source_block = part->inodes[source_inode].direct_blocks[i];
        if (source_block == -1) break;
        
        // Allouer un nouveau bloc pour la destination
        int dest_block = allocate_block(part);
        if (dest_block == -1) {
            printf("Erreur: Plus de blocs disponibles\n");
            delete_file(part, destination);
            return -1;
        }
        
        // Copier les données du bloc
        memcpy(part->data + dest_block * BLOCK_SIZE, 
               part->data + source_block * BLOCK_SIZE, 
               BLOCK_SIZE);
        
        // Associer le bloc au fichier destination
        part->inodes[dest_inode].direct_blocks[i] = dest_block;
    }
    
    // Copier le bloc indirect si nécessaire
    if (part->inodes[source_inode].indirect_block != -1) {
        // Allouer un nouveau bloc indirect pour la destination
        int dest_indirect = allocate_block(part);
        if (dest_indirect == -1) {
            printf("Erreur: Plus de blocs disponibles\n");
            delete_file(part, destination);
            return -1;
        }
        
        part->inodes[dest_inode].indirect_block = dest_indirect;
        
        // Copier la table d'indirection
        int *source_table = (int *)(part->data + part->inodes[source_inode].indirect_block * BLOCK_SIZE);
        int *dest_table = (int *)(part->data + dest_indirect * BLOCK_SIZE);
        
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            if (source_table[i] == -1) break;
            
            // Allouer un nouveau bloc pour les données
            int dest_block = allocate_block(part);
            if (dest_block == -1) {
                printf("Erreur: Plus de blocs disponibles\n");
                delete_file(part, destination);
                return -1;
            }
            
            // Copier les données du bloc
            memcpy(part->data + dest_block * BLOCK_SIZE, 
                   part->data + source_table[i] * BLOCK_SIZE, 
                   BLOCK_SIZE);
            
            // Mettre à jour la table d'indirection
            dest_table[i] = dest_block;
        }
    }
    
    // Mettre à jour les propriétaires et les temps
    part->inodes[dest_inode].uid = part->current_user.id;
    part->inodes[dest_inode].gid = part->current_user.group_id;
    part->inodes[dest_inode].atime = time(NULL);
    part->inodes[dest_inode].mtime = time(NULL);
    part->inodes[dest_inode].ctime = time(NULL);
    
    printf("Fichier '%s' copié vers '%s'\n", source, destination);
    return 0;
}


// Structure pour stocker les composants d'un chemin
typedef struct {
    char components[MAX_NAME_LENGTH][MAX_NAME_LENGTH];
    int num_components;
    int is_absolute;  // 1 si le chemin commence par '/', 0 sinon
} path_components_t;

// Fonction pour diviser un chemin en composants
void split_path(const char *path, path_components_t *components) {
    components->num_components = 0;
    components->is_absolute = (path[0] == '/');
    
    // Copier le chemin pour ne pas modifier l'original
    char path_copy[MAX_NAME_LENGTH * MAX_NAME_LENGTH];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    // Diviser le chemin en composants
    char *token = strtok(path_copy, "/");
    while (token != NULL && components->num_components < MAX_NAME_LENGTH) {
        strncpy(components->components[components->num_components], token, MAX_NAME_LENGTH - 1);
        components->components[components->num_components][MAX_NAME_LENGTH - 1] = '\0';
        components->num_components++;
        token = strtok(NULL, "/");
    }
}

// Fonction pour résoudre un chemin relatif ou absolu et obtenir l'inode correspondant
int resolve_path(partition_t *part, const char *path, int *parent_inode) {
    // Diviser le chemin en composants
    path_components_t components;
    split_path(path, &components);
    
    // Définir le répertoire de départ
    int current_inode = components.is_absolute ? 0 : part->current_dir_inode;
    int prev_inode = -1;
    
    // Parcourir les composants du chemin
    for (int i = 0; i < components.num_components; i++) {
        // Gérer les cas spéciaux
        if (strcmp(components.components[i], ".") == 0) {
            continue;  // Rester dans le répertoire courant
        } else if (strcmp(components.components[i], "..") == 0) {
            // Remonter au répertoire parent
            int parent = -1;
            
            // Trouver l'entrée ".." dans le répertoire courant
            for (int j = 0; j < NUM_DIRECT_BLOCKS; j++) {
                int block_num = part->inodes[current_inode].direct_blocks[j];
                if (block_num == -1) continue;
                
                dir_entry_t *dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
                int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
                
                for (int k = 0; k < num_entries; k++) {
                    if (dir_entries[k].inode_num != 0 && strcmp(dir_entries[k].name, "..") == 0) {
                        parent = dir_entries[k].inode_num;
                        break;
                    }
                }
                if (parent != -1) break;
            }
            
            if (parent != -1) {
                prev_inode = current_inode;
                current_inode = parent;
            }
            continue;
        }
        
        // Dernier composant du chemin
        if (i == components.num_components - 1) {
            if (parent_inode != NULL) {
                *parent_inode = current_inode;
            }
            
            // Vérifier si le dernier composant existe
            int file_inode = find_file_in_dir(part, current_inode, components.components[i]);
            return file_inode;  // Retourne -1 si non trouvé
        }
        
        // Trouver le répertoire correspondant au composant actuel
        int next_inode = find_file_in_dir(part, current_inode, components.components[i]);
        if (next_inode == -1) {
            return -1;  // Composant non trouvé
        }
        
        // Vérifier si c'est un répertoire
        if (!(part->inodes[next_inode].mode & 040000)) {
            return -1;  // Pas un répertoire
        }
        
        // Passer au répertoire suivant
        prev_inode = current_inode;
        current_inode = next_inode;
    }
    
    // Si le chemin se termine par '/', on retourne l'inode du répertoire
    if (parent_inode != NULL) {
        *parent_inode = prev_inode != -1 ? prev_inode : 0;
    }
    
    return current_inode;
}

// Fonction pour extraire le nom de fichier d'un chemin
void extract_filename(const char *path, char *filename) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
        strcpy(filename, last_slash + 1);
    } else {
        strcpy(filename, path);
    }
}

// Fonction améliorée pour déplacer un fichier avec support des chemins relatifs
int move_file_with_paths(partition_t *part, const char *source_path, const char *dest_path) {
    // Variables pour stocker les composants du chemin
    int source_parent_inode = -1;
    int dest_parent_inode = -1;
    char source_filename[MAX_NAME_LENGTH];
    char dest_filename[MAX_NAME_LENGTH];
    
    // Extraire le nom du fichier source
    extract_filename(source_path, source_filename);
    
    // Résoudre le chemin source
    int source_inode = resolve_path(part, source_path, &source_parent_inode);
    if (source_inode == -1) {
        printf("Erreur: Fichier source '%s' non trouvé\n", source_path);
        return -1;
    }
    
    // Vérifier si l'utilisateur a les droits nécessaires sur le fichier source
    if (!check_permission(part, source_inode, 2)) { // 2 = permission d'écriture
        printf("Erreur: Permission d'écriture refusée pour '%s'\n", source_path);
        return -1;
    }
    
    // Vérifier si l'utilisateur a les droits d'écriture sur le répertoire parent source
    if (!check_permission(part, source_parent_inode, 2)) {
        printf("Erreur: Permission d'écriture refusée pour le répertoire source\n");
        return -1;
    }
    
    // Extraire le nom du fichier destination
    extract_filename(dest_path, dest_filename);
    
    // Cas où le chemin de destination se termine par '/'
    if (dest_path[strlen(dest_path) - 1] == '/') {
        strcpy(dest_filename, source_filename);
    }
    
    // Résoudre le chemin de destination
    int dest_dir_inode = -1;
    char dest_dir_path[MAX_NAME_LENGTH * MAX_NAME_LENGTH];
    
    // Si le chemin de destination contient '/', on extrait le répertoire
    const char *last_slash = strrchr(dest_path, '/');
    if (last_slash != NULL) {
        int prefix_len = last_slash - dest_path;
        strncpy(dest_dir_path, dest_path, prefix_len);
        dest_dir_path[prefix_len] = '\0';
        
        // Si le chemin est vide, on utilise "/"
        if (prefix_len == 0) {
            strcpy(dest_dir_path, "/");
        }
        
        dest_dir_inode = resolve_path(part, dest_dir_path, &dest_parent_inode);
    } else {
        // Si pas de '/', on utilise le répertoire courant
        dest_dir_inode = part->current_dir_inode;
        dest_parent_inode = -1;  // Non utilisé dans ce cas
    }
    
    if (dest_dir_inode == -1) {
        printf("Erreur: Répertoire de destination non trouvé\n");
        return -1;
    }
    
    // Vérifier si le répertoire de destination est bien un répertoire
    if (!(part->inodes[dest_dir_inode].mode & 040000)) {
        printf("Erreur: La destination n'est pas un répertoire\n");
        return -1;
    }
    
    // Vérifier si l'utilisateur a les droits d'écriture sur le répertoire de destination
    if (!check_permission(part, dest_dir_inode, 2)) {
        printf("Erreur: Permission d'écriture refusée pour le répertoire de destination\n");
        return -1;
    }
    
    // Vérifier si le fichier destination existe déjà
    int dest_file_inode = find_file_in_dir(part, dest_dir_inode, dest_filename);
    if (dest_file_inode != -1) {
        printf("Erreur: Le fichier destination '%s' existe déjà\n", dest_filename);
        return -1;
    }
    
    // Trouver un bloc libre dans le répertoire de destination
    int dir_block = -1;
    int entry_index = -1;
    dir_entry_t *dir_entries = NULL;
    
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[dest_dir_inode].direct_blocks[i];
        if (block_num == -1) {
            // Allouer un nouveau bloc si nécessaire
            block_num = allocate_block(part);
            if (block_num == -1) {
                printf("Erreur: Plus de blocs disponibles\n");
                return -1;
            }
            part->inodes[dest_dir_inode].direct_blocks[i] = block_num;
            
            // Initialiser le nouveau bloc de répertoire
            dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
            memset(dir_entries, 0, BLOCK_SIZE);
            
            dir_block = block_num;
            entry_index = 0;
            break;
        }
        
        // Chercher une entrée libre dans les blocs existants
        dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int j = 0; j < num_entries; j++) {
            if (dir_entries[j].inode_num == 0) {
                dir_block = block_num;
                entry_index = j;
                break;
            }
        }
        
        if (dir_block != -1) break;
    }
    
    if (dir_block == -1) {
        printf("Erreur: Répertoire de destination plein\n");
        return -1;
    }
    
    // Créer l'entrée de répertoire pour le fichier de destination
    dir_entries = (dir_entry_t *)(part->data + dir_block * BLOCK_SIZE);
    dir_entries[entry_index].inode_num = source_inode;
    strncpy(dir_entries[entry_index].name, dest_filename, MAX_NAME_LENGTH - 1);
    dir_entries[entry_index].name[MAX_NAME_LENGTH - 1] = '\0';
    
    // Mettre à jour la taille du répertoire de destination
    part->inodes[dest_dir_inode].size += sizeof(dir_entry_t);
    part->inodes[dest_dir_inode].mtime = time(NULL);
    
    // Supprimer l'entrée de répertoire pour le fichier source
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[source_parent_inode].direct_blocks[i];
        if (block_num == -1) continue;
        
        dir_entries = (dir_entry_t *)(part->data + block_num * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int j = 0; j < num_entries; j++) {
            if (dir_entries[j].inode_num == source_inode && strcmp(dir_entries[j].name, source_filename) == 0) {
                // Effacer cette entrée
                memset(&dir_entries[j], 0, sizeof(dir_entry_t));
                
                // Mettre à jour le temps de modification du répertoire source
                part->inodes[source_parent_inode].mtime = time(NULL);
                
                printf("Fichier '%s' déplacé vers '%s'\n", source_path, dest_path);
                return 0;
            }
        }
    }
    
    printf("Avertissement: Entrée source non trouvée dans le répertoire\n");
    return 0;
}

// Modification de la fonction main pour ajouter la commande mv avec support des chemins
// Ajoutez ce code à la fonction main existante, juste avant le dernier else
/*
        else if (strncmp(command, "mv ", 3) == 0) {
            sscanf(command + 3, "%s %s", param1, param2);
            move_file_with_paths(partition, param1, param2);
        }
*/

// Ajoutez aussi cette ligne dans la section "help" de la fonction main
// printf("  mv src dst    - Déplace un fichier (supporte les chemins relatifs et absolus)\n");


// Interface utilisateur simple (fonction main)
int main() {
    // Créer et initialiser la partition
    partition_t *partition = (partition_t *)malloc(sizeof(partition_t));
    if (!partition) {
        printf("Erreur d'allocation mémoire\n");
        return 1;
    }
    
    init_partition(partition);
    
    char command[256];
    char param1[MAX_NAME_LENGTH];
    char param2[MAX_NAME_LENGTH];
    int running = 1;
    
    printf("Système de fichiers initialisé. Tapez 'help' pour voir les commandes disponibles.\n");
    
    while (running) {
        // Afficher l'invite de commande
        printf("[root@myfs ");
        print_current_path(partition);
        printf("]$ ");
        
        // Lire la commande
        fgets(command, sizeof(command), stdin);
        
        // Supprimer le retour à la ligne
        if (command[strlen(command) - 1] == '\n') {
            command[strlen(command) - 1] = '\0';
        }
        
        // Traiter la commande
        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            running = 0;
        }
        else if (strcmp(command, "help") == 0) {
            printf("Commandes disponibles:\n");
            printf("  help          - Affiche cette aide\n");
            printf("  ls            - Liste le contenu du répertoire courant\n");
            printf("  mkdir nom     - Crée un répertoire\n");
            printf("  touch nom     - Crée un fichier vide\n");
            printf("  cd nom        - Change de répertoire\n");
            printf("  rm nom        - Supprime un fichier ou répertoire\n");
            printf("  ln -s src dst - Crée un lien symbolique\n");
            printf("  chmod mode nom- Change les permissions d'un fichier (mode en octal)\n");
            printf("  chown uid:gid nom - Change le propriétaire d'un fichier\n");
            printf("  su uid gid    - Change d'utilisateur\n");
            printf("  pwd           - Affiche le chemin courant\n");
            printf("  cp src dst    - Copie un fichier\n");
            printf("  mv src dst    - Déplace un fichier (supporte les chemins relatifs et absolus)\n");
            printf("  exit          - Quitte le programme\n");
        }
        else if (strcmp(command, "ls") == 0) {
            list_directory(partition);
        }
        else if (strncmp(command, "mkdir ", 6) == 0) {
            sscanf(command + 6, "%s", param1);
            create_file(partition, param1, 040755);  // drwxr-xr-x
        }
        else if (strncmp(command, "touch ", 6) == 0) {
            sscanf(command + 6, "%s", param1);
            create_file(partition, param1, 0100644);  // -rw-r--r--
        }
        else if (strncmp(command, "cd ", 3) == 0) {
            sscanf(command + 3, "%s", param1);
            change_directory(partition, param1);
        }
        else if (strncmp(command, "rm ", 3) == 0) {
            sscanf(command + 3, "%s", param1);
            delete_file(partition, param1);
        }
        else if (strncmp(command, "ln -s ", 6) == 0) {
            sscanf(command + 6, "%s %s", param1, param2);
            create_symlink(partition, param2, param1);
        }
        else if (strncmp(command, "chmod ", 6) == 0) {
            int mode;
            sscanf(command + 6, "%o %s", &mode, param1);
            chmod_file(partition, param1, mode);
        }
        else if (strncmp(command, "chown ", 6) == 0) {
            int uid, gid;
            sscanf(command + 6, "%d:%d %s", &uid, &gid, param1);
            chown_file(partition, param1, uid, gid);
        }
   else if (strncmp(command, "su ", 3) == 0) {
            int uid, gid;
            sscanf(command + 3, "%d %d", &uid, &gid);
            switch_user(partition, uid, gid);
        }
        else if (strcmp(command, "pwd") == 0) {
            print_current_path(partition);
        }
    else if (strncmp(command, "cp ", 3) == 0) {
            sscanf(command + 3, "%s %s", param1, param2);
            copy_file(partition, param1, param2);
        }
    else if (strncmp(command, "mv ", 3) == 0) {
            sscanf(command + 3, "%s %s", param1, param2);
            move_file_with_paths(partition, param1, param2);
        }
        else {
            printf("Commande inconnue. Tapez 'help' pour voir les commandes disponibles.\n");
        }
    }
    
    // Libérer la mémoire
    free(partition);
    
    return 0;
}