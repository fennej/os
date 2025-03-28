#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "user.h"
#include "inode.h"
#include "filesystem.h"
#include "file_function.h"

// Prototypes de fonctions














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