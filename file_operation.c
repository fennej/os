/**
 * @file file_operation.c
 * @brief Opérations sur les fichiers et répertoires dans une partition.
 
 * participation: Mestar samy:50% Tighilt idir:50%
 * Ce fichier contient les fonctions permettant de créer des fichiers, de les
 * rechercher dans un répertoire et d'effectuer diverses opérations de gestion
 * des fichiers dans une partition.
 */

#include "file_operation.h"

/**
 * @brief Recherche un fichier dans un répertoire donné.
 * 
 * Cette fonction parcourt les entrées d'un répertoire pour trouver un fichier
 * portant le nom spécifié. Si le fichier est trouvé, elle retourne le numéro
 * d'inode du fichier. Sinon, elle retourne -1.
 * 
 * @param part Pointeur vers la partition contenant le répertoire.
 * @param dir_inode Numéro d'inode du répertoire à rechercher.
 * @param name Nom du fichier à rechercher.
 * @return int Le numéro d'inode du fichier trouvé, ou -1 si le fichier n'est pas trouvé.
 */

 int find_file_in_dir(partition_t *part, int dir_inode, const char *name) {
    if (!(part->inodes[dir_inode].mode & 040000)) {  // Vérifier si c'est un répertoire
        return -1;
    }
    
    // Parcourir tous les blocs du répertoire
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[dir_inode].direct_blocks[i];
        if (block_num == -1) continue;
        

        
        dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET) * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int j = 0; j < num_entries; j++) {
            if (dir_entries[j].inode_num != 0 && strcmp(dir_entries[j].name, name) == 0) {
                return dir_entries[j].inode_num;
            }
        }
    }
    
    // Vérifier le bloc indirect
    if (part->inodes[dir_inode].indirect_block != -1) {
        int *indirect_table = (int *)(part->space->data + part->inodes[dir_inode].indirect_block * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            if (indirect_table[i] != -1) {
                dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + indirect_table[i] * BLOCK_SIZE);
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

/**
 * @brief Crée un fichier dans le répertoire courant.
 * 
 * Cette fonction crée un nouveau fichier dans le répertoire courant en allouant
 * un nouvel inode, en initialisant son contenu et en ajoutant une entrée pour
 * ce fichier dans le répertoire. Elle vérifie également les permissions et
 * l'existence préalable du fichier.
 * 
 * @param part Pointeur vers la partition où créer le fichier.
 * @param name Nom du fichier à créer.
 * @param mode Mode du fichier (permissions et type, par exemple répertoire ou fichier ordinaire).
 * @return int Le numéro d'inode du fichier créé, ou -1 en cas d'erreur.
 */
int create_file(partition_t *part, const char *name, int mode) {
    // Vérifier les permissions du répertoire courant pour l'écriture (bit 2)
    if (!check_permission(part, part->current_dir_inode, 2)) {
        printf("Erreur: Permissions insuffisantes pour creer dans ce repertoire\n");
        return -1;
    }
    
    // Vérifier si le fichier existe déjà
    if (find_file_in_dir(part, part->current_dir_inode, name) != -1) {
        printf("Erreur: Un fichier avec ce nom existe deja\n");
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
        dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num+ USERSAPCE_OFSET) * BLOCK_SIZE);
        
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
        printf("Erreur: Impossible d'ajouter l'entree dans le repertoire\n");
        return -1;
    }
    
    printf("%s '%s' cree avec succes\n", (mode & 040000) ? "Repertoire" : "Fichier", name);
    return inode_num;
}


/**
 * @brief Crée un lien symbolique dans le répertoire courant.
 * 
 * Cette fonction crée un lien symbolique pointant vers un fichier cible. Elle vérifie 
 * les permissions nécessaires et si le lien ou la cible existe déjà. Un nouvel inode 
 * est alloué pour le lien symbolique et le chemin de la cible est stocké dans le 
 * bloc de données du lien.
 * 
 * @param part Pointeur vers la partition où créer le lien symbolique.
 * @param link_name Nom du lien symbolique à créer.
 * @param target_name Nom du fichier cible vers lequel le lien symbolique pointe.
 * @return int Le numéro d'inode du lien symbolique créé, ou -1 en cas d'erreur.
 */
int create_symlink(partition_t *part, const char *link_name, const char *target_name) {
    // Vérifier les permissions du répertoire courant pour l'écriture (bit 2)
    if (!check_permission(part, part->current_dir_inode, 2)) {
        printf("Erreur: Permissions insuffisantes pour creer dans ce repertoire\n");
        return -1;
    }
    
    // Vérifier si le nom du lien existe déjà
    if (find_file_in_dir(part, part->current_dir_inode, link_name) != -1) {
        printf("Erreur: Un fichier avec ce nom existe deja\n");
        return -1;
    }
    
    // Vérifier si la cible existe
    int target_inode = find_file_in_dir(part, part->current_dir_inode, target_name);
    if (target_inode == -1) {
        printf("Erreur: Fichier cible '%s' non trouve\n", target_name);
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
    strcpy(part->space->data + (data_block+USERSAPCE_OFSET) * BLOCK_SIZE, target_name);
    
    // Ajouter l'entrée dans le répertoire courant
    if (add_dir_entry(part, part->current_dir_inode, link_name, symlink_inode) != 0) {
        free_block(part, data_block);
        free_inode(part, symlink_inode);
        printf("Erreur: Impossible d'ajouter l'entree dans le repertoire\n");
        return -1;
    }
    
    printf("Lien symbolique '%s' vers '%s' créé avec succes\n", link_name, target_name);
    return symlink_inode;
}



/**
 * @brief Supprime un fichier ou un répertoire dans le répertoire courant.
 * 
 * Cette fonction supprime un fichier ou un répertoire dans le répertoire courant,
 * après avoir vérifié les permissions nécessaires et que le fichier ou répertoire
 * n'est pas vide (si c'est un répertoire). Elle décrémente également le nombre de 
 * liens de l'inode et libère les ressources associées lorsque le nombre de liens
 * atteint 0.
 * 
 * @param part Pointeur vers la partition contenant le fichier ou répertoire à supprimer.
 * @param name Nom du fichier ou répertoire à supprimer.
 * @return int 0 si la suppression a réussi, -1 en cas d'erreur.
 */
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
        printf("Erreur: Fichier '%s' non trouve\n", name);
        return -1;
    }
    
    // Vérifier si c'est un répertoire et s'il est vide
    if (part->inodes[inode_num].mode & 040000) {
        // Parcourir le contenu du répertoire pour voir s'il est vide
        for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
            int block_num = part->inodes[inode_num].direct_blocks[i];
            if (block_num == -1) continue;
            
            dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET) * BLOCK_SIZE);
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
    
    printf("%s '%s' suppression avec succes\n", (part->inodes[inode_num].mode & 040000) ? "Repertoire" : "Fichier", name);
    return 0;
}

/**
 * @brief Résout un lien symbolique en suivant les redirections jusqu'à ce qu'un fichier ou répertoire réel soit atteint.
 * 
 * Cette fonction limite la profondeur de résolution à 10 pour éviter les boucles infinies.
 * Si un lien symbolique est trouvé, la fonction suit la cible du lien jusqu'à ce qu'un fichier
 * ou répertoire réel soit atteint. Si un lien corrompu est détecté ou si un trop grand nombre
 * de niveaux de liens symboliques est rencontré, la fonction retourne une erreur.
 * 
 * @param part Partition contenant les informations sur les inodes et l'espace de données.
 * @param inode_num Numéro de l'inode du fichier ou répertoire à partir duquel commencer la résolution.
 * 
 * @return L'inode correspondant à la cible du lien symbolique, ou -1 en cas d'erreur.
 */
int resolve_symlink(partition_t *part, int inode_num) {
    int max_depth = 10;  // Limiter la profondeur pour éviter les boucles infinies
    
    for (int depth = 0; depth < max_depth; depth++) {

        if ((part->inodes[inode_num].mode & 0170000)!=0120000) { 
            return inode_num;
        }
        
        int data_block = part->inodes[inode_num].direct_blocks[0];
        if (data_block == -1) {
            return -1;  // Lien corrompu
        }
        
        char target_name[MAX_NAME_LENGTH];
        strncpy(target_name, part->space->data +( data_block+USERSAPCE_OFSET) * BLOCK_SIZE, MAX_NAME_LENGTH - 1);
        target_name[MAX_NAME_LENGTH - 1] = '\0';

        int target_inode = find_file_in_dir(part, part->current_dir_inode, target_name);
        if (target_inode == -1) {
            printf("Erreur: Cible du lien '%s' non trouvee\n", target_name);
            return -1;
        }
        
        inode_num = target_inode;
    }
    
    printf("Erreur: Trop de niveaux de liens symboliques\n");
    return -1;
}

/**
 * @brief Résout un chemin absolu et retourne l'inode correspondant au fichier ou répertoire.
 * 
 * Cette fonction résout un chemin absolu (commençant par '/') en parcourant les répertoires
 * et en suivant les liens symboliques éventuels. Si un lien symbolique est rencontré,
 * il est résolu jusqu'à ce qu'un fichier réel soit trouvé. La fonction vérifie également
 * les permissions nécessaires pour accéder aux répertoires.
 * 
 * @param part Partition contenant les informations sur les inodes et l'espace de données.
 * @param path Le chemin absolu à résoudre.
 * 
 * @return L'inode correspondant au fichier ou répertoire à la fin du chemin, ou -1 en cas d'erreur.
 */

int resolve_pathAB(partition_t *part, const char *path) {
    // Vérifier si le chemin est absolu (commence par '/')
    if (path == NULL || path[0] != '/') {
        printf("Erreur: Chemin non absolu\n");
        return -1;
    }
    
    // Commencer à la racine
    int current_inode = 1;
    
    // Si le chemin est juste "/", retourner l'inode de la racine
    if (strlen(path) == 1) {
        return current_inode;
    }
    
    // Faire une copie du chemin pour la tokenisation
    char path_copy[MAX_NAME_LENGTH * 4];
    strncpy(path_copy, path + 1, sizeof(path_copy) - 1); // +1 pour sauter le '/' initial
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    // Séparer le chemin en composants
    char *token = strtok(path_copy, "/");
    
    while (token != NULL) {
        // Sauter les composants "." (répertoire courant)
        if (strcmp(token, ".") == 0) {
            token = strtok(NULL, "/");
            continue;
        }
        
        // Gérer ".." (répertoire parent)
        if (strcmp(token, "..") == 0) {
            // Trouver l'inode parent en utilisant l'entrée ".."
            int parent_inode = -1;
            
            for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
                int block_num = part->inodes[current_inode].direct_blocks[i];
                if (block_num == -1) continue;
                
                // Calculer l'offset réel dans l'espace de données
                // Assurez-vous que vous utilisez la bonne façon d'accéder aux blocs
                int real_block_offset = (block_num + USERSAPCE_OFSET) * BLOCK_SIZE;
                dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + real_block_offset);
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
                printf("Erreur: Impossible de trouver le repertoire parent\n");
                return -1;
            }
            
            // Vérifier les permissions
            if (!check_permission(part, parent_inode, 1)) {  // Besoin de permission d'exécution
                printf("Erreur: Permissions insuffisantes pour acceder au repertoire parent\n");
                return -1;
            }
            
            current_inode = parent_inode;
        }
        // Gérer un composant normal du chemin
        else {

            printf("hiii %d  %s",current_inode,token);
            // Chercher ce composant dans le répertoire courant
            int inode_num = find_file_in_dir(part, current_inode, token);
            if (inode_num == -1) {
                printf("Erreur: '%s' non trouve\n", token);
                return -1;
            }
            
            // Si c'est un lien symbolique, le résoudre
            if (part->inodes[inode_num].mode & 0120000) {
                printf("Suivi du lien symbolique '%s'...\n", token);
                inode_num = resolve_symlink(part, inode_num);
                if (inode_num == -1) {
                    return -1;
                }
            }
            
            // Vérifier les permissions (sauf pour le dernier composant du chemin)
            char *next_token = strtok(NULL, "/");
            if (next_token != NULL) {
                // Ce n'est pas le dernier composant, nous avons besoin de permission d'exécution
                // Vérifier si c'est un répertoire
                if (!(part->inodes[inode_num].mode & 040000)) {
                    printf("Erreur: '%s' n'est pas un repertoire\n", token);
                    return -1;
                }
                
                if (!check_permission(part, inode_num, 1)) {  // Besoin de permission d'exécution
                    printf("Erreur: Permissions insuffisantes pour acceder à '%s'\n", token);
                    return -1;
                }
            }
            // Si c'est le dernier composant, ne pas vérifier s'il s'agit d'un répertoire
            
            // Mettre à jour l'inode courant
            current_inode = inode_num;
            
            // Remettre le token pour le prochain tour
            token = next_token;
            continue;
        }
        
        // Passer au composant suivant
        token = strtok(NULL, "/");
    }
    
    // Mettre à jour le temps d'accès
    part->inodes[current_inode].atime = time(NULL);
    
    return current_inode;
}


/**
 * @brief Résout un lien symbolique en suivant sa cible.
 * 
 * Cette fonction vérifie si un inode correspond à un lien symbolique. Si oui, elle résout
 * le lien symbolique et retourne l'inode du fichier cible. Sinon, elle retourne l'inode
 * d'origine.
 * 
 * @param part Partition contenant les informations sur les inodes et l'espace de données.
 * @param symlink_inode Numéro de l'inode du lien symbolique à résoudre.
 * 
 * @return L'inode de la cible du lien symbolique ou l'inode d'origine si ce n'est pas un lien symbolique, ou -1 en cas d'erreur.
 */

int resolve_symlink2(partition_t *part, int symlink_inode) {
    // Check if it's a symlink
    if (!(part->inodes[symlink_inode].mode & 0120000)) {
        return symlink_inode; // Not a symlink, return as is
    }
    
    // Read the target path
    int data_block = part->inodes[symlink_inode].direct_blocks[0];
    if (data_block == -1) return -1;
    
    char target_path[MAX_NAME_LENGTH];
    strcpy(target_path, part->space->data + (data_block + USERSAPCE_OFSET) * BLOCK_SIZE);
    
    // Find the target file
    return find_file_in_dir(part, part->current_dir_inode, target_path);
}

/**
 * @brief Lit le contenu d'un fichier et le copie dans un buffer.
 * 
 * Cette fonction lit un fichier à partir du système de fichiers et le copie dans un tampon. 
 * Si le fichier est un lien symbolique, il est d'abord résolu. Elle vérifie également si 
 * l'utilisateur dispose des permissions nécessaires pour lire le fichier.
 * 
 * @param part Partition contenant les informations sur les inodes et l'espace de données.
 * @param name Le nom du fichier à lire.
 * @param buffer Le tampon où les données lues seront copiées.
 * @param max_size La taille maximale à lire.
 * 
 * @return Le nombre d'octets effectivement lus, ou -1 si le fichier n'a pas été trouvé, ou -2 si les permissions sont insuffisantes.
 */

int read_from_file(partition_t *part, const char *name, char *buffer, int max_size) {

    // Trouver l'inode du fichier
    int inode_num = find_file_in_dir(part, part->current_dir_inode, name);
    if (inode_num < 0) return -1; // Fichier non trouvé
    
    if ((part->inodes[inode_num].mode & 0170000) == 0120000) {
        inode_num = resolve_symlink2(part, inode_num);
        if (inode_num < 0) return -1; // Failed to resolve symlink
    }

    // Vérifier les permissions de lecture
    if (!check_permission(part, inode_num, 4)) return -2; // Pas de permission
    
    // Déterminer la taille à lire
    int size_to_read = (part->inodes[inode_num].size < max_size) ? 
                        part->inodes[inode_num].size : max_size;
    
    int remaining = size_to_read;
    int offset = 0;
    
    // Lire les données depuis les blocs directs
    for (int i = 0; i < NUM_DIRECT_BLOCKS && remaining > 0; i++) {
        int block_num = part->inodes[inode_num].direct_blocks[i];
        if (block_num == -1) break;
        
        int read_size = (remaining > BLOCK_SIZE) ? BLOCK_SIZE : remaining;
        
        // Copier les données du bloc vers le buffer
        memcpy(buffer + offset, part->space->data + (block_num + USERSAPCE_OFSET) * BLOCK_SIZE, read_size);
        
        offset += read_size;
        remaining -= read_size;
    }
    
    // Mettre à jour le temps d'accès
    part->inodes[inode_num].atime = time(NULL);
    
    return size_to_read - remaining;
}

/**
 * @brief Affiche le contenu d'un fichier dans la sortie standard (simule la commande 'cat').
 * 
 * Cette fonction utilise `read_from_file` pour lire le contenu d'un fichier et l'afficher à l'écran.
 * Si une erreur se produit, un message d'erreur est affiché.
 * 
 * @param part Partition contenant les informations sur les inodes et l'espace de données.
 * @param name Le nom du fichier à afficher.
 */

void cat_command(partition_t *part, const char *name) {
    // Tampon pour stocker le contenu du fichier
    char buffer[10240]; // Maximum de 10Ko pour cet exemple
    
    int bytes_read = read_from_file(part, name, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; // Ajouter un terminateur de chaîne
        printf("%s\n", buffer);
    } else if (bytes_read == -1) {
        printf("Erreur: fichier '%s' non trouve.\n", name);
    } else if (bytes_read == -2) {
        printf("Erreur: permission refusee pour '%s'.\n", name);
    }


}


/**
 * @brief Écrit du contenu dans un fichier, simule la commande 'cat >'.
 * 
 * Cette fonction utilise une fonction d'écriture pour placer le contenu dans un fichier donné.
 * Si une erreur se produit lors de l'écriture ou si les permissions sont insuffisantes,
 * un message d'erreur est affiché.
 * 
 * @param part Partition contenant les informations sur les inodes et l'espace de données.
 * @param name Le nom du fichier où le contenu doit être écrit.
 * @param content Le contenu à écrire dans le fichier.
 * 
 * @return 0 si l'écriture a réussi, ou 1 si une erreur est survenue.
 */

int cat_write_command(partition_t *part, const char *name, const char *content) {
    int bytes_written = write_to_file(part, name, content, strlen(content));
    if(bytes_written<0)
    if (bytes_written == -1) {
        printf("Erreur lors de l'écriture dans '%s'.\n", name);
        return 1;
    } else if (bytes_written==-2){
        printf("Erreur: permission refusée pour '%s'.\n", name);
        return 1;
    } else {
        printf("%d octets écrits dans '%s'.\n", bytes_written, name);
    }
    return 0;
}


/**
 * @brief Crée un lien dur vers un fichier existant.
 * 
 * Cette fonction crée un lien dur dans un répertoire donné pointant vers un fichier existant. 
 * Un lien dur est un autre nom pour un fichier sur le disque, qui fait référence au même inode.
 * 
 * @param part Partition où le fichier et le répertoire sont situés.
 * @param target_path Chemin vers le fichier cible vers lequel créer le lien dur.
 * @param link_path Chemin du nouveau lien dur à créer.
 * @return int Retourne 0 si la création du lien dur est réussie, -1 en cas d'erreur.
 */
int create_hard_link(partition_t *part, const char *target_path, const char *link_path) {
    int target_parent_inode = -1;
    int link_parent_inode = -1;
    
    // Résoudre le chemin de la cible
    int target_inode = resolve_path(part, target_path, &target_parent_inode);
    if (target_inode == -1) {
        printf("Erreur: Fichier cible '%s' non trouvé\n", target_path);
        return -1;
    }
    
    // Vérifier si la cible est un répertoire
    if (part->inodes[target_inode].mode & 040000) {
        printf("Erreur: Impossible de créer un lien dur vers un répertoire\n");
        return -1;
    }
    
    // Résoudre le chemin parent du lien
    char link_parent_path[MAX_NAME_LENGTH * 4];
    char link_name[MAX_NAME_LENGTH];
    
    // Extraire le nom du lien et le chemin de son parent
    const char *last_slash = strrchr(link_path, '/');
    if (last_slash == NULL) {
        // Chemin relatif sans slash, utiliser le répertoire courant
        strcpy(link_parent_path, ".");
        strcpy(link_name, link_path);
    } else if (last_slash == link_path) {
        // Lien dans la racine
        strcpy(link_parent_path, "/");
        strcpy(link_name, last_slash + 1);
    } else {
        // Extraire le chemin du parent
        int parent_len = last_slash - link_path;
        strncpy(link_parent_path, link_path, parent_len);
        link_parent_path[parent_len] = '\0';
        strcpy(link_name, last_slash + 1);
    }
    
    // Résoudre le répertoire parent du lien
    int link_dir_inode = resolve_path(part, link_parent_path, &link_parent_inode);
    if (link_dir_inode == -1) {
        printf("Erreur: Répertoire parent du lien '%s' non trouvé\n", link_parent_path);
        return -1;
    }
    
    // Vérifier si le répertoire parent du lien est bien un répertoire
    if (!(part->inodes[link_dir_inode].mode & 040000)) {
        printf("Erreur: '%s' n'est pas un répertoire\n", link_parent_path);
        return -1;
    }
    
    // Vérifier si l'utilisateur a les droits d'écriture sur le répertoire parent du lien
    if (!check_permission(part, link_dir_inode, 2)) {
        printf("Erreur: Permission d'écriture refusée pour '%s'\n", link_parent_path);
        return -1;
    }
    
    // Vérifier si un fichier avec le même nom existe déjà dans le répertoire parent du lien
    if (find_file_in_dir(part, link_dir_inode, link_name) != -1) {
        printf("Erreur: '%s' existe déjà\n", link_name);
        return -1;
    }
    
    // Trouver un bloc libre pour ajouter l'entrée de répertoire
    int dir_block = -1;
    int entry_index = -1;
    
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[link_dir_inode].direct_blocks[i];
        if (block_num == -1) {
            // Allouer un nouveau bloc
            block_num = allocate_block(part);
            if (block_num == -1) {
                printf("Erreur: Plus de blocs disponibles\n");
                return -1;
            }
            part->inodes[link_dir_inode].direct_blocks[i] = block_num;
            
            // Initialiser le nouveau bloc
            memset(part->space->data + (block_num + USERSAPCE_OFSET) * BLOCK_SIZE, 0, BLOCK_SIZE);
            
            dir_block = block_num;
            entry_index = 0;
            break;
        }
        
        // Chercher une entrée libre dans le bloc existant
        dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num + USERSAPCE_OFSET) * BLOCK_SIZE);
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
    
    // Si aucun bloc direct n'a d'espace, essayer le bloc indirect
    if (dir_block == -1) {
        // Si pas de bloc indirect, en allouer un
        if (part->inodes[link_dir_inode].indirect_block == -1) {
            int indirect_block = allocate_block(part);
            if (indirect_block == -1) {
                printf("Erreur: Plus de blocs disponibles\n");
                return -1;
            }
            part->inodes[link_dir_inode].indirect_block = indirect_block;
            
            // Initialiser la table indirecte
            int *indirect_table = (int *)(part->space->data + (indirect_block + USERSAPCE_OFSET) * BLOCK_SIZE);
            for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
                indirect_table[i] = -1;
            }
        }
        
        // Parcourir la table indirecte
        int *indirect_table = (int *)(part->space->data + (part->inodes[link_dir_inode].indirect_block + USERSAPCE_OFSET) * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            if (indirect_table[i] == -1) {
                // Allouer un nouveau bloc
                int block_num = allocate_block(part);
                if (block_num == -1) {
                    printf("Erreur: Plus de blocs disponibles\n");
                    return -1;
                }
                indirect_table[i] = block_num;
                
                // Initialiser le nouveau bloc
                memset(part->space->data + (block_num + USERSAPCE_OFSET) * BLOCK_SIZE, 0, BLOCK_SIZE);
                
                dir_block = block_num;
                entry_index = 0;
                break;
            }
            
            // Chercher une entrée libre dans le bloc existant
            dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (indirect_table[i] + USERSAPCE_OFSET) * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            
            for (int j = 0; j < num_entries; j++) {
                if (dir_entries[j].inode_num == 0) {
                    dir_block = indirect_table[i];
                    entry_index = j;
                    break;
                }
            }
            
            if (dir_block != -1) break;
        }
    }
    
    if (dir_block == -1) {
        printf("Erreur: Répertoire plein, impossible d'ajouter une nouvelle entrée\n");
        return -1;
    }
    
    // Ajouter l'entrée de répertoire pour le nouveau lien dur
    dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (dir_block + USERSAPCE_OFSET) * BLOCK_SIZE);
    dir_entries[entry_index].inode_num = target_inode;
    strncpy(dir_entries[entry_index].name, link_name, MAX_NAME_LENGTH - 1);
    dir_entries[entry_index].name[MAX_NAME_LENGTH - 1] = '\0';
    
    // Incrémenter le nombre de liens dans l'inode cible
    part->inodes[target_inode].links_count++;
    
    // Mettre à jour le temps de modification du répertoire parent
    part->inodes[link_dir_inode].mtime = time(NULL);
    
    printf("Lien dur '%s' créé vers '%s'\n", link_path, target_path);
    return 0;
}


/**
 * @brief Supprime un fichier ou un répertoire de manière récursive.
 * 
 * Cette fonction supprime un fichier ou un répertoire et son contenu, si c'est un répertoire, de manière récursive. 
 * Pour un répertoire, cette fonction va d'abord supprimer les fichiers et sous-répertoires avant de supprimer le répertoire lui-même.
 * 
 * @param part Partition contenant les fichiers et répertoires à supprimer.
 * @param path Chemin du fichier ou répertoire à supprimer.
 * @return int Retourne 0 si la suppression est réussie, -1 en cas d'erreur.
 */

int delete_recursive(partition_t *part, const char *path) {
    int parent_inode = -1;
    int target_inode = resolve_path(part, path, &parent_inode);
    
    if (target_inode == -1) {
        printf("Erreur: '%s' n'existe pas\n", path);
        return -1;
    }
    
    // Vérifier les permissions sur le parent
    if (!check_permission(part, parent_inode, 2)) { // 2 = écriture
        printf("Erreur: Permissions insuffisantes pour supprimer '%s'\n", path);
        return -1;
    }
    
    // Extraire le nom du fichier/répertoire à supprimer
    char target_name[MAX_NAME_LENGTH];
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        strcpy(target_name, path);
    } else {
        strcpy(target_name, last_slash + 1);
    }
    
    // Vérifier si c'est un répertoire
    if (part->inodes[target_inode].mode & 040000) {
        // C'est un répertoire, vérifier s'il est vide
        for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
            int block_num = part->inodes[target_inode].direct_blocks[i];
            if (block_num == -1) continue;
            
            dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num + USERSAPCE_OFSET) * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            
            for (int j = 0; j < num_entries; j++) {
                if (dir_entries[j].inode_num != 0 && 
                    strcmp(dir_entries[j].name, ".") != 0 && 
                    strcmp(dir_entries[j].name, "..") != 0) {
                    // Construire le chemin complet pour l'entrée
                    char subpath[MAX_NAME_LENGTH * 2];
                    if (strcmp(path, "/") == 0) {
                        snprintf(subpath, sizeof(subpath), "/%s", dir_entries[j].name);
                    } else {
                        snprintf(subpath, sizeof(subpath), "%s/%s", path, dir_entries[j].name);
                    }
                    
                    // Supprimer récursivement
                    if (delete_recursive(part, subpath) != 0) {
                        return -1;
                    }
                }
            }
        }
        
        // Vérifier également le bloc indirect pour les entrées de répertoire
        if (part->inodes[target_inode].indirect_block != -1) {
            int *indirect_table = (int *)(part->space->data + (part->inodes[target_inode].indirect_block + USERSAPCE_OFSET) * BLOCK_SIZE);
            for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
                if (indirect_table[i] == -1) continue;
                
                dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (indirect_table[i] + USERSAPCE_OFSET) * BLOCK_SIZE);
                int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
                
                for (int j = 0; j < num_entries; j++) {
                    if (dir_entries[j].inode_num != 0 && 
                        strcmp(dir_entries[j].name, ".") != 0 && 
                        strcmp(dir_entries[j].name, "..") != 0) {
                        // Construire le chemin complet pour l'entrée
                        char subpath[MAX_NAME_LENGTH * 2];
                        if (strcmp(path, "/") == 0) {
                            snprintf(subpath, sizeof(subpath), "/%s", dir_entries[j].name);
                        } else {
                            snprintf(subpath, sizeof(subpath), "%s/%s", path, dir_entries[j].name);
                        }
                        
                        // Supprimer récursivement
                        if (delete_recursive(part, subpath) != 0) {
                            return -1;
                        }
                    }
                }
            }
        }
    }
    
    // Diminuer le nombre de liens
    part->inodes[target_inode].links_count--;
    
    // Si le nombre de liens est 0, libérer l'inode et les blocs associés
    if (part->inodes[target_inode].links_count <= 0) {
        free_inode(part, target_inode);
    }
    
    // Supprimer l'entrée du répertoire parent
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[parent_inode].direct_blocks[i];
        if (block_num == -1) continue;
        
        dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num + USERSAPCE_OFSET) * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int j = 0; j < num_entries; j++) {
            if (dir_entries[j].inode_num == target_inode && strcmp(dir_entries[j].name, target_name) == 0) {
                // Effacer cette entrée
                memset(&dir_entries[j], 0, sizeof(dir_entry_t));
                
                // Mettre à jour le temps de modification du répertoire parent
                part->inodes[parent_inode].mtime = time(NULL);
                
                printf("'%s' supprimé avec succès\n", path);
                return 0;
            }
        }
    }
    
    // Vérifier également le bloc indirect du parent
    if (part->inodes[parent_inode].indirect_block != -1) {
        int *indirect_table = (int *)(part->space->data + (part->inodes[parent_inode].indirect_block + USERSAPCE_OFSET) * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            if (indirect_table[i] == -1) continue;
            
            dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (indirect_table[i] + USERSAPCE_OFSET) * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            
            for (int j = 0; j < num_entries; j++) {
                if (dir_entries[j].inode_num == target_inode && strcmp(dir_entries[j].name, target_name) == 0) {
                    // Effacer cette entrée
                    memset(&dir_entries[j], 0, sizeof(dir_entry_t));
                    
                    // Mettre à jour le temps de modification du répertoire parent
                    part->inodes[parent_inode].mtime = time(NULL);
                    
                    printf("'%s' supprimé avec succès\n", path);
                    return 0;
                }
            }
        }
    }
    
    printf("Erreur: Entrée non trouvée dans le répertoire parent\n");
    return -1;
}
