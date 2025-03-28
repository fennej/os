CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lc
OBJ = main.o filesystem.o inode.o block.o user.o directory.o file_function.o block_function.o

all: main

main: $(OBJ)
	$(CC) $(CFLAGS) -o main $(OBJ) $(LDFLAGS)

block_function.o: block_function.c block_function.h block.h
	$(CC) $(CFLAGS) -c block_function.c

block.o: block.c block.h
	$(CC) $(CFLAGS) -c block.c

main.o: main.c user.h block.h file_function.h inode.h directory.h
	$(CC) $(CFLAGS) -c main.c

filesystem.o: filesystem.c block.h block_function.h
	$(CC) $(CFLAGS) -c filesystem.c

file_function.o: file_function.c file_function.h inode.h block.h user.h directory.h
	$(CC) $(CFLAGS) -c file_function.c

directory.o: directory.c user.h block.h file_function.h
	$(CC) $(CFLAGS) -c directory.c

inode.o: inode.c inode.h block.h block_function.h
	$(CC) $(CFLAGS) -c inode.c


user.o: user.c block.h 
	$(CC) $(CFLAGS) -c user.c

clean:
	rm -f *.o main