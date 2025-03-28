#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "directory.h"

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
