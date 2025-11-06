CC=gcc
CFLAGS=-Wall -lpthread -lm

SOURCES=main.c commands.c vfs.c
OBJECTS=$(SOURCES:.c=.o)

all: clean comp

comp: $(OBJECTS)
	${CC} $(OBJECTS) -o fs-on-inode $(CFLAGS)

# Правило для компіляції кожного .c файлу в .o
%.o: %.c
	${CC} -c $< -o $@

clean:
	rm -f fs-on-inode
	rm -f *.o
	rm -f *.*~
