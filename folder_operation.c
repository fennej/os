#include "inode.h"


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
            dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + block_num * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
            for (int j = 0; j < num_entries; j++) {
                dir_entries[j].inode_num = 0;
                memset(dir_entries[j].name, 0, MAX_NAME_LENGTH);
            }
        }
        
        // Chercher une entrée libre dans le bloc
        dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET)*BLOCK_SIZE);
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
        int *indirect_table = (int *)(part->space->data + indirect_block * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            indirect_table[i] = -1;
        }
    }
    
    // Utiliser le bloc indirect
    int *indirect_table = (int *)(part->space->data + part->inodes[dir_inode].indirect_block * BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
        if (indirect_table[i] == -1) {
            // Allouer un nouveau bloc pour le répertoire
            int block_num = allocate_block(part);
            if (block_num == -1) return -1;  // Plus de bloc disponible
            
            indirect_table[i] = block_num;
            
            // Initialiser le bloc avec des entrées vides
            dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET) * BLOCK_SIZE);
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
            dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + indirect_table[i] * BLOCK_SIZE);
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
        
        dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET)* BLOCK_SIZE);
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
        int *indirect_table = (int *)(part->space->data + part->inodes[dir_inode].indirect_block * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            if (indirect_table[i] != -1) {
                dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + indirect_table[i] * BLOCK_SIZE);
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


/// Fonction pour changer de répertoire
int change_directory(partition_t *part, const char *path) {
    // Vérifier si le chemin est vide
    if (!path || strlen(path) == 0) {
        return 0;
    }
    
    // Si c'est un chemin absolu (commence par '/')
    if (path[0] == '/') {
        int target_inode = resolve_pathAB(part, path);
        if (target_inode == -1) {
            return -1;  // Erreur déjà affichée par resolve_path
        }
        
        // Vérifier si c'est un répertoire
        if (!(part->inodes[target_inode].mode & 040000)) {
            printf("Erreur: '%s' n'est pas un repertoire\n", path);
            return -1;
        }
        
        // Vérifier les permissions pour entrer dans le répertoire
        if (!check_permission(part, target_inode, 1)) {  // Besoin de permission d'exécution
            printf("Erreur: Permissions insuffisantes pour acceder à '%s'\n", path);
            return -1;
        }
        
        // Tout est bon, changer de répertoire
        part->current_dir_inode = target_inode;
        printf("Changement vers le repertoire '%s' reussi\n", path);
        return 0;
    }
    
    // Si c'est un chemin relatif
    // Faire une copie du chemin pour la tokenisation
    char path_copy[MAX_NAME_LENGTH * 4];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    // Sauvegarder l'inode du répertoire original au cas où
    int original_dir_inode = part->current_dir_inode;
    
    // Cas spéciaux simples
    if (strcmp(path, ".") == 0) {
        return 0;  // Rester dans le même répertoire
    }
    
    if (strcmp(path, "..") == 0) {
        // Trouver l'inode du parent
        int parent_inode = -1;
        
        for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
            int block_num = part->inodes[part->current_dir_inode].direct_blocks[i];
            if (block_num == -1) continue;
            
            // Calculer l'offset réel - ASSUREZ-VOUS QUE LE CALCUL EST CORRECT
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
        if (!check_permission(part, parent_inode, 1)) {
            printf("Erreur: Permissions insuffisantes pour accéder au repertoire parent\n");
            return -1;
        }
        
        part->current_dir_inode = parent_inode;
        printf("Changement vers le repertoire parent reussi\n");
        return 0;
    }
    
    // Pour un chemin relatif complexe comme "foo/bar" ou "../foo"
    char *token = strtok(path_copy, "/");
    
    while (token != NULL) {
        // Cas spécial pour "."
        if (strcmp(token, ".") == 0) {
            // Ne rien faire, rester dans le même répertoire
        }
        // Cas spécial pour ".."
        else if (strcmp(token, "..") == 0) {
            // Trouver l'inode du parent
            int parent_inode = -1;
            
            for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
                int block_num = part->inodes[part->current_dir_inode].direct_blocks[i];
                if (block_num == -1) continue;
                
                // Calculer l'offset réel - ASSUREZ-VOUS QUE C'EST CORRECT
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
                part->current_dir_inode = original_dir_inode;  // Restaurer la position originale
                return -1;
            }
            
            // Vérifier les permissions
            if (!check_permission(part, parent_inode, 1)) {
                printf("Erreur: Permissions insuffisantes pour acceder au repertoire parent\n");
                part->current_dir_inode = original_dir_inode;  // Restaurer la position originale
                return -1;
            }
            
            part->current_dir_inode = parent_inode;
        }
        // Cas général: un nom de fichier/répertoire normal
        else {
            int inode_num = find_file_in_dir(part, part->current_dir_inode, token);
            if (inode_num == -1) {
                printf("Erreur: '%s' non trouve\n", token);
                part->current_dir_inode = original_dir_inode;  // Restaurer la position originale
                return -1;
            }
            
            // Si c'est un lien symbolique, le résoudre
            if (part->inodes[inode_num].mode & 0120000) {
                printf("Suivi du lien symbolique '%s'...\n", token);
                printf("resolution du lin sym avec %d ",inode_num);
                inode_num = resolve_symlink(part, inode_num);
                if (inode_num == -1) {
                    part->current_dir_inode = original_dir_inode;  // Restaurer la position originale
                    return -1;
                }
            }
            
            // Vérifier si c'est un répertoire
            if (!(part->inodes[inode_num].mode & 040000)) {
                printf("Erreur: '%s' n'est pas un répertoire\n", token);
                part->current_dir_inode = original_dir_inode;  // Restaurer la position originale
                return -1;
            }
            
            // Vérifier les permissions
            if (!check_permission(part, inode_num, 1)) {
                printf("Erreur: Permissions insuffisantes pour acceder à '%s'\n", token);
                part->current_dir_inode = original_dir_inode;  // Restaurer la position originale
                return -1;
            }
            
            // Mettre à jour le répertoire courant
            part->current_dir_inode = inode_num;
            
            // Mettre à jour le temps d'accès
            part->inodes[inode_num].atime = time(NULL);
        }
        
        // Passer au composant suivant
        token = strtok(NULL, "/");
    }
    
    printf("Changement vers le repertoire '%s' reussi\n", path);
    return 0;
}


// Fonction pour lister le contenu d'un répertoire
void list_directory(partition_t *part,char* parem) {
        int inode_num = find_file_in_dir(part, part->current_dir_inode, "idir");
   //     printf("After write: mode = %o , num est %d \n", part->inodes[inode_num].mode,inode_num);
    // Vérifier les permissions pour lire le répertoire (bit 4 = r)
    if (!check_permission(part, part->current_dir_inode, 4)) {
        printf("Erreur: Permissions insuffisantes pour lire le contenu de ce repertoire\n");
        return;
    }
        printf("Droits      Liens  Prop  Groupe     Taille    Date         Nom       inode num\n");

    if(parem!=NULL){
    int file_inode = find_file_in_dir(part, part->current_dir_inode, parem);
        if(file_inode==-1){
            printf("le fichier specifie n'existe pas");
        }else{
char type_char = '-';
      //  printf("After write: mode = %o , num est %d \n", part->inodes[inode_num].mode,inode_num);

            //printf(" testtt %o\n",part->inodes[1].mode);
           // printf("%o,le nom %s le num de l'inode %d le file inoden est %d",part->inodes[file_inode].mode,dir_entries[j].name,dir_entries[j].inode_num,file_inode);
            // Déterminer le type de fichier
            if ((part->inodes[file_inode].mode & 0170000) == 040000) type_char = 'd'; // Répertoire
            else if ((part->inodes[file_inode].mode & 0170000) == 0120000) type_char = 'l';  // Lien symbolique
            
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
            printf("%s  %4d  %4d  %5d  %6d  %s  %s   %6d", 
                   perm_str, 
                   part->inodes[file_inode].links_count, 
                   part->inodes[file_inode].uid, 
                   part->inodes[file_inode].gid,
                   part->inodes[file_inode].size,
                   date_str,
                  parem,
                  file_inode);
        }
        return;
    }
    
    // Parcourir tous les blocs du répertoire
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[part->current_dir_inode].direct_blocks[i];
        if (block_num == -1) continue;
        
        dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET) * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int j = 0; j < num_entries; j++) {
            if (dir_entries[j].inode_num == 0) continue;
            
            int file_inode = dir_entries[j].inode_num;
            char type_char = '-';
      //  printf("After write: mode = %o , num est %d \n", part->inodes[inode_num].mode,inode_num);

            //printf(" testtt %o\n",part->inodes[1].mode);
           // printf("%o,le nom %s le num de l'inode %d le file inoden est %d",part->inodes[file_inode].mode,dir_entries[j].name,dir_entries[j].inode_num,file_inode);
            // Déterminer le type de fichier
            if ((part->inodes[file_inode].mode & 0170000) == 040000) type_char = 'd'; // Répertoire
            else if ((part->inodes[file_inode].mode & 0170000) == 0120000) type_char = 'l';  // Lien symbolique
            
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
            printf("%s  %4d  %4d  %5d  %6d  %s  %s   %10d", 
                   perm_str, 
                   part->inodes[file_inode].links_count, 
                   part->inodes[file_inode].uid, 
                   part->inodes[file_inode].gid,
                   part->inodes[file_inode].size,
                   date_str,
                   dir_entries[j].name,
                   file_inode);

            // Si c'est un lien symbolique, afficher la cible
            if ((part->inodes[file_inode].mode & 0170000) == 0120000){
                int data_block = part->inodes[file_inode].direct_blocks[0];
                if (data_block != -1) {
                    printf(" -> %s", part->space->data + (data_block+USERSAPCE_OFSET) * BLOCK_SIZE);
                }
            }
            
            printf("\n");
        }
    }

    // Traitement des blocs indirects
    if (part->inodes[part->current_dir_inode].indirect_block != -1) {
        int *indirect_table = (int *)(part->space->data + part->inodes[part->current_dir_inode].indirect_block * BLOCK_SIZE);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int); i++) {
            if (indirect_table[i] != -1) {
                dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + indirect_table[i] * BLOCK_SIZE);
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
                            printf(" -> %s", part->space->data + data_block * BLOCK_SIZE);
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
            
            dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET) * BLOCK_SIZE);
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
            
            dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET) * BLOCK_SIZE);
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
    int current_inode = components.is_absolute ? 1 : part->current_dir_inode;
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
                
                dir_entry_t *dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET) * BLOCK_SIZE);
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
int move_file_with_paths(partition_t *part, const char *source_path, const char *dest_path,int mode) {
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
        printf("Erreur: Fichier source '%s' non trouve\n", source_path);
        return -1;
    }
    
    // Vérifier si l'utilisateur a les droits nécessaires sur le fichier source
    if (!check_permission(part, source_inode, 2)) { // 2 = permission d'écriture
        printf("Erreur: Permission d'ecriture refusee pour '%s'\n", source_path);
        return -1;
    }
    
    // Vérifier si l'utilisateur a les droits d'écriture sur le répertoire parent source
    if (!check_permission(part, source_parent_inode, 2)) {
        printf("Erreur: Permission d'ecriture refusee pour le repertoire source\n");
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
        printf("Erreur: Repertoire de destination non trouve\n");
        return -1;
    }
    
    // Vérifier si le répertoire de destination est bien un répertoire
    if (!(part->inodes[dest_dir_inode].mode & 040000)) {
        printf("Erreur: La destination n'est pas un repertoire\n");
        return -1;
    }
    
    // Vérifier si l'utilisateur a les droits d'écriture sur le répertoire de destination
    if (!check_permission(part, dest_dir_inode, 2)) {
        printf("Erreur: Permission d'ecriture refusee pour le repertoire de destination\n");
        return -1;
    }
    
    // Vérifier si le fichier destination existe déjà
    int dest_file_inode = find_file_in_dir(part, dest_dir_inode, dest_filename);
    if (dest_file_inode != -1) {
        printf("Erreur: Le fichier destination '%s' existe deja\n", dest_filename);
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
            dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET) * BLOCK_SIZE);
            memset(dir_entries, 0, BLOCK_SIZE);
            
            dir_block = block_num;
            entry_index = 0;
            break;
        }
        
        // Chercher une entrée libre dans les blocs existants
        dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET) * BLOCK_SIZE);
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
        printf("Erreur: Repertoire de destination plein\n");
        return -1;
    }
    
    // Créer l'entrée de répertoire pour le fichier de destination
    dir_entries = (dir_entry_t *)(part->space->data +( dir_block +USERSAPCE_OFSET)* BLOCK_SIZE);
    dir_entries[entry_index].inode_num = source_inode;
    strncpy(dir_entries[entry_index].name, dest_filename, MAX_NAME_LENGTH - 1);
    dir_entries[entry_index].name[MAX_NAME_LENGTH - 1] = '\0';
    
    // Mettre à jour la taille du répertoire de destination
    part->inodes[dest_dir_inode].size += sizeof(dir_entry_t);
    part->inodes[dest_dir_inode].mtime = time(NULL);
    

    if(mode==COPYMODE){
        printf("la copy est realise ");
        return 0;
    }
    // Supprimer l'entrée de répertoire pour le fichier source
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        int block_num = part->inodes[source_parent_inode].direct_blocks[i];
        if (block_num == -1) continue;
        
        dir_entries = (dir_entry_t *)(part->space->data + (block_num +USERSAPCE_OFSET) * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(dir_entry_t);
        
        for (int j = 0; j < num_entries; j++) {
            if (dir_entries[j].inode_num == source_inode && strcmp(dir_entries[j].name, source_filename) == 0) {
                // Effacer cette entrée
                memset(&dir_entries[j], 0, sizeof(dir_entry_t));
                
                // Mettre à jour le temps de modification du répertoire source
                part->inodes[source_parent_inode].mtime = time(NULL);
                
                printf("Fichier '%s' deplace vers '%s'\n", source_path, dest_path);
                return 0;
            }
        }
    }

    
    printf("Avertissement: Entree source non trouvee dans le repertoire\n");
    return 0;
}


int write_to_file(partition_t *part, const char *name, const char *data, int size) {
    
    // Trouver l'inode du fichier par son nom
    int inode_num = find_file_in_dir(part, part->current_dir_inode, name);
    
    // Si le fichier n'existe pas, le créer
    if (inode_num < 0) {
        inode_num = create_file(part, name, 0100644); // -rw-r--r--
        if (inode_num < 0) return -1; // Échec de création
    }
    if (!check_permission(part, inode_num, 2)) return -2; // Pas de permission
    // Calculer combien de blocs sont nécessaires
    int blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    // Libérer les anciens blocs si le fichier existe déjà
    for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
        if (part->inodes[inode_num].direct_blocks[i] != -1) {
            free_block(part, part->inodes[inode_num].direct_blocks[i]);
            part->inodes[inode_num].direct_blocks[i] = -1;
        }
    }
    
    // Écrire les données dans les nouveaux blocs
    int remaining = size;
    int offset = 0;
    
    for (int i = 0; i < blocks_needed && i < NUM_DIRECT_BLOCKS; i++) {
        // Allouer un nouveau bloc
        int block_num = allocate_block(part);
        if (block_num < 0) return -1; // Plus d'espace disponible
        
        part->inodes[inode_num].direct_blocks[i] = block_num;
        
        // Déterminer combien d'octets écrire dans ce bloc
        int write_size = (remaining > BLOCK_SIZE) ? BLOCK_SIZE : remaining;
        
        // Copier les données dans le bloc
        memcpy(part->space->data + (block_num + USERSAPCE_OFSET) * BLOCK_SIZE, data + offset, write_size);
        
        offset += write_size;
        remaining -= write_size;
    }
    
    // Mettre à jour la taille du fichier
    part->inodes[inode_num].size = size;
    part->inodes[inode_num].mtime = time(NULL);
    
    return size;
}