#ifndef STRUCTURE_H
#define STRUCTURE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>


#define BLOCK_SIZE 512
#define MAX_NAME_LENGTH 32
#define MAX_BLOCKS 1024
#define MAX_INODES 100
#define NUM_DIRECT_BLOCKS 12  // Nombre de blocs directs par inode (comme dans Unix)
#define INDIRECT_BLOCKS 1     // Nombre de blocs indirects par inode
#define PARTITION_SIZE (BLOCK_SIZE * MAX_BLOCKS)
#define SYSBLOCK 20

#define PARTITION_OFSET 0
#define INODE_OFSET 5
#define USERT_OFSET 6
#define BLOCKB_OFSET 8
#define INODEB_OFSET 9
#define SUPERBLOCK_OFSET 10

#define USERSAPCE_OFSET 15 



// Types d'utilisateurs
#define USER_OWNER 0
#define USER_GROUP 1
#define USER_OTHER 2


#define COPYMODE 0
#define MOVMODE 1

// Structure pour la carte des blocs (bitmap)
typedef struct {
    unsigned char bitmap[MAX_BLOCKS-USERSAPCE_OFSET / 8 + 1];  // Chaque bit représente un bloc
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

typedef struct espace_utilisable_t {
    char data[PARTITION_SIZE];  // Données stockées sur la partition
} espace_utilisable_t; 


// Structure pour représenter la partition
typedef struct {
    superblock_t *superblock;         // Pointeur vers le superbloc
    block_bitmap_t *block_bitmap;     // Pointeur vers le bitmap des blocs
    inode_bitmap_t *inode_bitmap;     // Pointeur vers le bitmap des inodes
    inode_t *inodes;                  // Pointeur vers la table d'inodes
    espace_utilisable_t *space;       // Pointeur vers les données stockées
    int current_dir_inode;            // Inode du répertoire courant
    user_t current_user;              // Utilisateur courant
} partition_t;

//partition globale qui vas servire a la gertion des sig



// Structure pour stocker les composants d'un chemin
typedef struct {
    char components[MAX_NAME_LENGTH][MAX_NAME_LENGTH];
    int num_components;
    int is_absolute;  // 1 si le chemin commence par '/', 0 sinon
} path_components_t;

#endif