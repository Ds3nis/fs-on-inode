#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "helpers.h"
#include <stdio.h>


/*
 * Returns if string str1 equals str2
 */
bool streq(char *str1, char *str2) {
    if (strcmp(str1, str2) == 0) {
        return true;
    } else {
        return false;
    }
}


/*
 * Returns true if string is empty
 */
bool str_empty(char *str) {
    if (str == NULL || streq(str, "")) {
        return true;
    }

    return false;
}


char * get_line() {
    char * line = calloc(1, 100), * linep = line;
    size_t lenmax = 100, len = lenmax;
    int c;

    if(line == NULL) {
        return NULL;
    }

    while(true) {
        c = fgetc(stdin);
        if(c == EOF)
            break;

        if(--len == 0) {
            len = lenmax;
            char * linen = realloc(linep, lenmax *= 2);

            if(linen == NULL) {
                free(linep);
                return NULL;
            }
            line = linen + (line - linep);
            linep = linen;
        }

        if((*line++ = c) == '\n')
            break;
    }
    *line = '\0';
    return linep;
}

/*
 * Removes new line characters from string
 */
void remove_nl_inplace(char *message) {
    int len = strlen(message);
    int index = 0;
    for (int i = 0; i < len; i++) {
        if (message[i] != 10 && message[i] != 13) {
            message[index++] = message[i];
        }
    }
    message[index] = '\0';
}