//
// Created by Denis on 03.11.2025.
//

#include <stdio.h>
#include <stdlib.h>
#include "structures.h"
#include "commands.h"
#include "vfs.h"
#include <string.h>
#include "helpers.h"
#include "constants.h"

VFS *current_vfs = NULL;


/*
 * Main command loop — handles user input and dispatches commands
 */
void run_shell() {
    while (1) {
        printf("vfs> ");
        char *input = get_line();
        remove_nl_inplace(input);
        if (!input || strlen(input) == 0) {
            continue;
        }

        // process command — if returns true, user entered 'exit'
        if (process_command_line(&current_vfs, input)) {
            printf("\nExiting Virtual File System...\n");
            break;
        }

        free(input);
    }
}

/*
 * Shows program banner
 */
void show_banner() {
    printf("\n");
    printf("=====================================\n");
    printf("     Virtual File System (VFS)    \n");
    printf("=====================================\n");
    printf("   Author: Khuda Denys\n");
    printf("   Language: C\n");
    printf("-------------------------------------\n");
    printf("Type 'help' to see available commands\n");
    printf("Type 'exit' to quit\n");
    printf("=====================================\n\n");
}

/*
 * Program entry point
 */
int main(int argc, char *argv[]) {
    show_banner();

    if (argc == 2) {
        char *filename = argv[1];
        printf("Loading virtual filesystem: %s\n", filename);

        initialize_vfs(&current_vfs, filename);
        run_shell();
    } else {
        printf("Usage: %s <vfs_file>\n", argv[0]);
        printf("Example: %s mydisk.vfs\n", argv[0]);
    }

    return 0;
}
