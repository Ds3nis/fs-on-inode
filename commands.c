//
// Created by Denis on 03.11.2025.
//

#include "commands.h"
#include "constants.h"
#include "vfs.h"
#include "helpers.h"
#include <string.h>
#include <stdlib.h>




static const char *ERR_SRC_DEST[] = {SRC_NOT_DEFINED_MSG, DEST_NOT_DEFINED_MSG};
static const char *ERR_DIRNAME[]  = {SRC_NOT_DEFINED_MSG};

Command commands[] = {
    {"incp",  true,  2, ERR_SRC_DEST, cmd_incp,  "Copy file from host to VFS"},
    {"outcp", true,  2, ERR_SRC_DEST, cmd_outcp, "Copy file from VFS to host"},
    {"cp",    true,  2, ERR_SRC_DEST, cmd_cp,    "Copy file inside VFS"},
    {"mkdir", true,  1, ERR_DIRNAME,  cmd_mkdir, "Create new directory"},
    {"format",false, 1, NULL,         cmd_format_vfs,"Format the virtual disk"},
    {"help",  false, 0, NULL,         cmd_help,  "Show available commands"},
    {"exit",  false, 0, NULL,         cmd_exit,  "Exit the program"},
};


const int command_count = sizeof(commands) / sizeof(Command);


bool validate_and_execute_command(VFS **vfs, Command *cmd, char *input) {
    if (cmd->requires_format) {
        printf(VFS_NOT_INITIALIZED_MSG);
        return false;
    }

    char *args[10] = {0};
    for (int i = 0; i < cmd->expected_args; i++) {
        args[i] = strtok(NULL, " ");
        if (str_empty(args[i])) {
            if (cmd->arg_error_msgs && cmd->arg_error_msgs[i]) {
                printf("%s", cmd->arg_error_msgs[i]);
            } else {
                printf("Missing argument #%d for command '%s'\n", i + 1, cmd->name);
            }
            return false;
        }
    }

    cmd->handler(vfs, args);
    return false;
}


/*
 * Main command processor — looks up command in the table and runs handler
 */
int process_command_line(VFS **vfs, char *input) {
    char *command_name = strtok(input, " ");

    if (!command_name) return false;
    for (int i = 0; i < command_count; i++) {
        printf("Compare: '%s' and '%s'\n", command_name, commands[i].name);

        if (streq(command_name, commands[i].name)) {

            bool should_exit = false;
            if (strcmp(command_name, "exit") == 0) should_exit = true;

            bool result = validate_and_execute_command(vfs, &commands[i], input);
            return should_exit ? 1 : result;
        }
    }

    printf(UNKNOWN_COMMAND_MSG, command_name);
    return 0;
}

/*
 * Shows how to use the program
 */
void cmd_help() {
    printf("/----------\\\n");
    printf("|   HELP   |\n");
    printf("\\----------/\n");
    printf("\n");
    printf("Starting the program: \n\n");
    printf("./vfs [filesystem_name]\n\n");
    printf("Available commands: \n\n");
    printf("cp s1 s2  --  Copies the file from path s1 to path s2\n");
    printf("mv s1 s2  --  Moves the file from path s1 to path s2\n");
    printf("rm s1  --  Deletes file s1\n");
    printf("mkdir a1  --  Creates directory a1\n");
    printf("rmdir a1  --  Deletes the directory a1\n");
    printf("ls a1  --  Lists the contents of the directory a1\n");
    printf("cat s1  --  Lists the contents of the file s1\n");
    printf("cd a1  --  Changes the current folder to the directory at address a1\n");
    printf("pwd  --  Lists the path to the current folder\n");
    printf("info a1/s1  --  Lists information about the given file/folder\n");
    printf("incp s1 s2  --  Moves a file from the real file system to the virtual one\n");
    printf("outcp s1 s2  --  Moves a file from the virtual file system to the real one\n");
    printf("load s1  --  Reads commands line by line from file s1 from the real file system\n");
    printf("format 600M  --  Formats the virtual file system (VFS)\n");
    printf("check  --  Performs a consistency check of the file system\n");
    printf("size inode_id 600M  --  Changes the size of the i-node with the given ID to the size specified by the parameter\n");
}




void cmd_format_vfs(VFS **vfs, char **args) {
    if (!args[0]) { printf("Missing argument!\n"); return; }

    int32_t vfs_size = atoi(args[0]);
    if (vfs_size < MIN_FS) {
        printf(FORMAT_ERROR_SIZE_MSG);
        return;
    }

    FILE *file = fopen((*vfs)->name, "wb+");
    if (!file) {
        printf(OPEN_FILE_ERR_MSG);
        return;
    }
    (*vfs)->vfs_file = file;

    if (!vfs_init_memory_structures(vfs, vfs_size)) {
        fclose(file);
        printf(MEMORY_ERROR_MSG);
        return;
    }

    char buffer[CLUSTER_SIZE];
    memset(buffer, 0, CLUSTER_SIZE);
    for (int i = 0; i < (*vfs)->superblock->cluster_count; i++) {
        write_vfs(vfs, buffer, CLUSTER_SIZE, 1);
    }


    vfs_write_superblock_to_file(vfs);

    vfs_write_bitmaps_to_file(vfs);

    vfs_write_inodes_to_file(vfs);

    flush_vfs(vfs);

    (*vfs)->is_formatted = true;

    printf(FORMAT_SUCCESS_MSG);
    check_sb_info(vfs);
}


void cmd_incp() {

}


void cmd_outcp() {

}

void cmd_cp(){

}

void cmd_mkdir() {

}

void cmd_format(){
}


void cmd_exit() {

}

