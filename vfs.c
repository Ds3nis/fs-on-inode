//
// Created by Denis on 05.11.2025.
//

#include <stdio.h>
#include <stdlib.h>
#include "vfs.h"
#include "structures.h"
#include <string.h>
#include "constants.h"
#include "commands.h"
#include "helpers.h"

void initialize_vfs(VFS **vfs, char *vfs_name) {
    *vfs = calloc(1, sizeof(VFS));

    if (!*vfs) {
        printf(MEMORY_ERROR_MSG);
        exit(1);
    }

    (*vfs)->name = strdup(vfs_name);

    FILE *file = fopen(vfs_name, "rb+");
    if (file == NULL) {
        needs_format(vfs);
        return;
    }

    (*vfs)->vfs_file = file;
    (*vfs)->is_formatted = true;


    if (!load_vfs(vfs)) {
        printf(VFS_ERROR);
        fclose(file);
        free(vfs);
        exit(1);
    }

    printf(VFS_LOAD_SUCCESS);
}

void needs_format(VFS **vfs) {
    printf(START_NEEDS_FORMAT_MSG);
    (*vfs)->is_formatted = false;
    (*vfs)->vfs_file = NULL;

    printf(FORMAT_VFS);
    char choice;
    scanf(" %c", &choice);
    if (choice == 'y' || choice == 'Y') {
        printf("Enter filesystem size in bytes: ");
        char *fs_size = get_line();
        remove_nl_inplace(fs_size);
        char *size_args[2] = { fs_size};
        cmd_format_vfs(vfs, size_args);
        free(fs_size);
    }
}

bool load_vfs(VFS **vfs) {

}