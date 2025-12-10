COMPILER       = gcc
CFLAGS         = -Wall -Wextra -pedantic -std=c17 -g
LDFLAGS        = -lm

EXECUTABLE     = vfs
SOURCES        = main.c vfs.c commands.c helpers.c
OBJECTS        = $(SOURCES:.c=.o)

RUN_ARGUMENT   = myvfs

.PHONY: all run clean valgrind

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(COMPILER) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

run: $(EXECUTABLE)
	./$(EXECUTABLE) "$(RUN_ARGUMENT)"

valgrind: $(EXECUTABLE)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(EXECUTABLE) "$(RUN_ARGUMENT)"

clean:
	rm -f $(EXECUTABLE) $(OBJECTS)