#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "user.h"

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