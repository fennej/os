# Système de fichiers virtuel

Ce projet implémente un système de fichiers virtuel permettant de gérer des fichiers et des répertoires avec une série de commandes. inspiré des systèmes de type UNIX. Le système implémente les opérations de base sur les fichiers et répertoires, ainsi qu'une gestion des droits d'accès et des liens.


## Commandes disponibles

### `help`
Affiche l'aide et la liste des commandes disponibles.

### `ls`
Liste le contenu du répertoire courant et affichier des information sur un fichier.

**Exemple :**
```bash
> ls
> ls fichier.txt
```
### `mkdir nom`
Crée un répertoire avec le nom spécifié.

**Exemple :**
```bash
> mkdir dossier
```
### `touch nom`
Crée un fichier vide avec le nom spécifié.

**Exemple :**
```bash
> touch fichier.txt
```
### `cd nom`
Change le répertoire courant pour celui spécifié.

**Exemple :**
```bash
> cd dossier
```
### `rm nom`
Supprime un fichier ou un répertoire.

**Exemple :**
```bash
> rm fichier.txt
```
### `ln -s src dst`
Crée un lien symbolique entre le fichier source (`src`) et le fichier de destination (`dst`).

**Exemple :**
```bash
> ln -s fichier.txt fichierS.txt
```

### `chmod mode nom`
Change les permissions d'un fichier ou répertoire. Le mode doit être spécifié en octal.

**Exemple :**
```bash
> chmod 777 fichier.txt
```
### `chown uid:gid nom`
Change le propriétaire d'un fichier en spécifiant l'identifiant utilisateur (`uid`) et l'identifiant de groupe (`gid`).

**Exemple :**
```bash
> chown 1:1 fichier.txt
```
### `su uid gid`
Change l'utilisateur actuel pour l'utilisateur spécifié par `uid` et `gid`.

**Exemple :**
```bash
> su 1 1
```
### `pwd`
Affiche le chemin complet du répertoire courant.

**Exemple :**
```bash
> pwd
```
### `cp src dst`
Copie un fichier de `src` vers `dst`.

**Exemple :**
```bash
> cp fichier.txt dossier/
> cp fichier.txt fichier2.txt
```
### `mv src dst`
Déplace un fichier de `src` vers `dst`. Cette commande prend en charge les chemins relatifs et absolus.

**Exemple :**
```bash
> mv fichier.txt dossier/
> mv fichier.txt fichier2.txt
```

### `save fichier`
Sauvegarde l'état actuel du système de fichiers dans le fichier spécifié.

**Exemple :**
```bash
> save fichier_sauvegarde.data
```
### `load fichier`
Charge un système de fichiers à partir du fichier spécifié.

**Exemple :**
```bash
> load fichier_sauvegarde.data
```
### `exit`
Quitte le programme.

**Exemple :**
```bash
> exit
```
## Installation

executer simplement make
```bash
make
```
vous n'aurais alors que a executer le code avec ou sans un fichier de test , contenant des commandes que vous pourrez definir
**Exemple :**
```bash
> ./main
> ./main programme_de_test.txt
``` 