CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lc
OBJ = main.o inode.o block.o file_operation.o folder_operation.o init.o load.o permission.o

all: main

main: $(OBJ)
	$(CC) $(CFLAGS) -o main $(OBJ) $(LDFLAGS)

main.o: main.c structure.h inode.h block.h file_operation.h folder_operation.h init.h load.h permission.h
	$(CC) $(CFLAGS) -c main.c

inode.o: inode.c inode.h
	$(CC) $(CFLAGS) -c inode.c


block.o: block.c block.h 
	$(CC) $(CFLAGS) -c block.c

file_operation.o: file_operation.c file_operation.h 
	$(CC) $(CFLAGS) -c file_operation.c

folder_operation.o: folder_operation.c folder_operation.h  block.h
	$(CC) $(CFLAGS) -c folder_operation.c

init.o: init.c init.h 
	$(CC) $(CFLAGS) -c init.c

load.o: load.c load.h 
	$(CC) $(CFLAGS) -c load.c

permission.o: permission.c permission.h 
	$(CC) $(CFLAGS) -c permission.c


clean:
	rm -f *.o main
