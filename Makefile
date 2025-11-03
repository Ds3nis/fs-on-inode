CC=gcc

all:	clean comp

comp:
	${CC} main.c -o fs-on-inode -lpthread -lm -Wall


clean:
	rm -f fs-on-inode
	rm -f *.*~
