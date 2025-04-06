#include "permission.h"

/**
 * @brief Vérifie si l'utilisateur courant possède une permission spécifique sur un fichier.
 *
 * @param part Pointeur vers la structure de la partition.
 * @param inode_num Numéro de l'inode du fichier.
 * @param perm_bit Bit de permission à vérifier (ex: lecture, écriture, exécution).
 * @return 1 si la permission est accordée, 0 sinon.
 */

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

/**
 * @brief Modifie les permissions (mode) d'un fichier.
 *
 * @param part Pointeur vers la structure de la partition.
 * @param name Nom du fichier.
 * @param mode Nouveau mode de permissions (ex: 0644).
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */

int chmod_file(partition_t *part, const char *name, int mode) {
    // Trouver le fichier dans le répertoire courant
    int inode_num = find_file_in_dir(part, part->current_dir_inode, name);
    if (inode_num == -1) {
        printf("Erreur: Fichier '%s' non trouve\n", name);
        return -1;
    }
    
    // Vérifier si l'utilisateur est le propriétaire du fichier ou root
    if (part->current_user.id != 0 && part->current_user.id != part->inodes[inode_num].uid) {
        printf("Erreur: Vous devez etre le proprietaire du fichier pour changer ses permissions\n");
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



/**
 * @brief Modifie le propriétaire et le groupe d'un fichier.
 *
 * @param part Pointeur vers la structure de la partition.
 * @param name Nom du fichier.
 * @param uid Nouvel identifiant utilisateur.
 * @param gid Nouvel identifiant de groupe.
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */
int chown_file(partition_t *part, const char *name, int uid, int gid) {
    // Seul root peut changer le propriétaire
    if (part->current_user.id != 0) {
        printf("Erreur: Seul root peut changer le proprietaire d'un fichier\n");
        return -1;
    }
    
    // Trouver le fichier dans le répertoire courant
    int inode_num = find_file_in_dir(part, part->current_dir_inode, name);
    if (inode_num == -1) {
        printf("Erreur: Fichier '%s' non trouve\n", name);
        return -1;
    }
    
    // Changer le propriétaire et le groupe
    part->inodes[inode_num].uid = uid;
    part->inodes[inode_num].gid = gid;
    
    // Mettre à jour le temps de modification
    part->inodes[inode_num].ctime = time(NULL);
    
    printf("Proprietaire et groupe modifies pour '%s'\n", name);
    return 0;
}


/**
 * @brief Change l'utilisateur courant.
 *
 * @param part Pointeur vers la structure de la partition.
 * @param uid Nouvel identifiant utilisateur.
 * @param gid Nouvel identifiant de groupe.
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */
int switch_user(partition_t *part, int uid, int gid) {
    // En pratique, il faudrait vérifier si l'utilisateur existe dans un fichier passwd
    // et demander un mot de passe, mais pour ce prototype, on le fait simplement
    
    // Mettre à jour l'utilisateur courant
    part->current_user.id = uid;
    sprintf(part->current_user.name, "user%d", uid);  // Nom générique
    part->current_user.group_id = gid;
    
    printf("Utilisateur change: uid=%d, gid=%d\n", uid, gid);
    return 0;
}
