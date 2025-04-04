#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "folder_operation.h"
#include "block.h"
#include "file_operation.h"
#include "init.h"
#include "inode.h"
#include "load.h"
#include "permission.h"




void setup_signal_handler(partition_t *part) {
    // Stocker la référence à la partition pour pouvoir y accéder dans le gestionnaire de signal
     global_partition = part;
    
    // Installer le gestionnaire de signal pour SIGINT (Ctrl+C)
     struct sigaction sa;
     sa.sa_handler = sigint_handler;
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = 0;
    
     if (sigaction(SIGINT, &sa, NULL) == -1) {
         perror("Erreur lors de l'installation du gestionnaire de signal");
     }
}


int main(int argc, char *argv[]) {

    

    espace_utilisable_t *space = (espace_utilisable_t *)malloc(sizeof(espace_utilisable_t));
    if (!space) {
        printf("Erreur d'allocation memoire\n");
        return 1;
    }
    
    // Allouer de la mémoire pour la structure partition séparément
    partition_t *partition = (partition_t *)malloc(sizeof(partition_t));
    if (!partition) {
        printf("Erreur d'allocation memoire pour partition\n");
        free(space);
        return 1;
    }

    
    // Définir la relation entre partition et space
    partition->space = space;
    
    // Initialiser les pointeurs vers les zones dans part->space->data
    partition->superblock = (superblock_t*)(partition->space->data + SUPERBLOCK_OFSET * BLOCK_SIZE);

    partition->superblock->block_size=5;
    
    // Initialiser le bitmap des blocs
    block_bitmap_t* test = (block_bitmap_t*)(partition->space->data + BLOCKB_OFSET* BLOCK_SIZE );
    


    partition->block_bitmap=test;


    partition->block_bitmap->bitmap[4]=0xFF;

    // Initialiser la partition
    init_partition(partition);

    setup_signal_handler(partition);
    
    char command[256];
    char param1[MAX_NAME_LENGTH];
    char param2[MAX_NAME_LENGTH];
    int running = 1;
    
    printf("Systeme de fichiers initialise. Tapez 'help' pour voir les commandes disponibles.\n");
    
    create_file(partition,"root",040777);
    change_directory(partition,"root");
    FILE *input = stdin;
    if (argc > 1) {
        input = fopen(argv[1], "r");
        if (!input) {
            perror("Erreur lors de l'ouverture du fichier de commandes");
            free(space);
            free(partition);
            return 1;
        }
    }
    int done=0;
    while (running) {
        // Afficher l'invite de commande
        if (done==0){
            printf("[user@myfs ");
            print_current_path(partition);
            printf("]$ ");}
        else{
            done=0;
        }
        // Lire la commande
        if (!fgets(command, sizeof(command), input)) {
            if (input != stdin) {
                fclose(input);  // Fermer le fichier
                input = stdin;  // Passer en mode interactif
                done=1;
                continue;  // Reprendre la boucle pour éviter d'exécuter une commande vide
        }
            break;
        }
        
        
        
        
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
            printf("  mkdir nom     - Cree un répertoire\n");
            printf("  touch nom     - Cree un fichier vide\n");
            printf("  cd nom        - Change de repertoire\n");
            printf("  rm nom        - Supprime un fichier ou repertoire\n");
            printf("  ln -s src dst - Cree un lien symbolique\n");
            printf("  chmod mode nom- Change les permissions d'un fichier (mode en octal)\n");
            printf("  chown uid:gid nom - Change le proprietaire d'un fichier\n");
            printf("  su uid gid    - Change d'utilisateur\n");
            printf("  pwd           - Affiche le chemin courant\n");
            printf("  cp src dst    - Copie un fichier\n");
            printf("  mv src dst    - Deplace un fichier (supporte les chemins relatifs et absolus)\n");
            printf("  save fichier  - sauvegarde la partition dans un fichier\n");
            printf("  load fichier  - load la partition a partir d'un fichier\n");
            printf("  exit          - Quitte le programme\n");
        } else if (strncmp(command, "ls", 2) == 0) {
            if (sscanf(command + 2, "%s", param1) == 1) {
                list_directory(partition, param1); // Exécuter avec l'argument s'il existe
            } else {
                list_directory(partition, NULL); // Sinon, exécuter sans argument
            }
            printf("\n");
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
        }else if(strncmp(command, "rm -r  ", 5) == 0){
            sscanf(command + 5, "%s", param1);
            delete_recursive(partition,param1);
        }else
         if (strncmp(command, "rm ", 3) == 0) {
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
            printf("\n");
        }
        else if (strncmp(command, "cp ", 3) == 0) {
            sscanf(command + 3, "%s %s", param1, param2);
            move_file_with_paths(partition, param1, param2,COPYMODE);
        }
        else if (strncmp(command, "mv ", 3) == 0) {
            sscanf(command + 3, "%s %s", param1, param2);
            move_file_with_paths(partition, param1, param2,MOVMODE);
 
        }else if(strncmp(command, "cat > ",6 ) == 0){
    // Extraire le nom du fichier en préservant les espaces éventuels
    char filename[MAX_NAME_LENGTH];
    int i = 6;  // Position après "cat > "
    int j = 0;
    
    // Ignorer les espaces initiaux
    while(command[i] == ' ') i++;
    
    // Copier le nom du fichier jusqu'à la fin de la commande
    while(command[i] != '\0' && command[i] != '\n' && j < MAX_NAME_LENGTH - 1) {
        filename[j++] = command[i++];
    }
    filename[j] = '\0';
    
    if(strlen(filename) == 0) {
        printf("Erreur: Nom de fichier requis\n");
    } else {
        // Allouer un buffer pour stocker le contenu
        char *content = malloc(BLOCK_SIZE * NUM_DIRECT_BLOCKS);  // Taille maximale possible
        if(!content) {
            printf("Erreur: Memoire insuffisante\n");
        } else {
            content[0] = '\0';  // Initialiser la chaîne vide
            
            printf("Entrez le contenu du fichier (terminez par un . sur une ligne vide):\n");
            
            char buffer[1024];
            while(fgets(buffer, sizeof(buffer), input) != NULL) {
                // Vérifier si la saisie est terminée par un signal spécial
                if(strcmp(buffer, ".\n") == 0) break;  // Une ligne contenant seulement un point termine la saisie
                
                // Ajouter la ligne au contenu
                if(strlen(content) + strlen(buffer) < BLOCK_SIZE * NUM_DIRECT_BLOCKS) {
                    strcat(content, buffer);
                } else {
                    printf("Avertissement: Taille maximale du fichier atteinte\n");
                    break;
                }
            }
            
            // Écrire le contenu dans le fichier
            if(cat_write_command(partition, filename, content) != 0) {
                printf("Erreur: Échec d'écriture dans le fichier '%s'\n", filename); 
            }
            
            free(content);
        }
    }

            
        }else if(strncmp(command, "cat ",4 ) == 0){
            sscanf(command +4,"%s %s", param1);
            cat_command(partition,param1);
        }else if (strncmp(command, "save ", 5) == 0) {
    		sscanf(command + 5, "%s", param1);
  		  	save_partition(partition, param1);
		}else if (strncmp(command, "load ", 5) == 0) {
 		   	sscanf(command + 5, "%s", param1);
  			load_partition(partition, param1);
		}else if(strncmp(command, "ln ", 3) == 0){
            sscanf(command + 3, "%s", param1);
            delete_recursive(partition,param1);
            }
        else {
            printf("Commande inconnue. Tapez 'help' pour voir les commandes disponibles.\n");
        }
    }
    
    // Libérer la mémoire
    free(space);  // Libérer l'espace utilisable plutôt que partition
    if (input != stdin) {
        fclose(input);
    }
    return 0;
}