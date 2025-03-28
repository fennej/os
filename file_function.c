#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "file_function.h"

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