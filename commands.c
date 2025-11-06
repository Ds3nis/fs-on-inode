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
    int vfs_size = atoi(args[0]);
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

    superblock *sb = calloc(1, sizeof(superblock));
    strcpy(sb->signature, SUPERBLOCK_SIGNATURE);
    sb->disk_size = vfs_size;
    sb->cluster_size = CLUSTER_SIZE;
    sb->cluster_count = vfs_size / CLUSTER_SIZE;

    // 10% для inodes, 5% для bitmap, решта для даних
    sb->bitmap_cluster_count = sb->cluster_count * 0.05;
    sb->inode_cluster_count = sb->cluster_count * 0.10;
    sb->data_cluster_count = sb->cluster_count - sb->bitmap_cluster_count - sb->inode_cluster_count;

    sb->bitmap_start_address = sizeof(superblock);
    sb->inode_start_address = sb->bitmap_start_address + sb->bitmap_cluster_count * CLUSTER_SIZE;
    sb->data_start_address = sb->inode_start_address + sb->inode_cluster_count * CLUSTER_SIZE;

    (*vfs)->superblock = sb;

    (*vfs)->data_bitmap = calloc(sb->cluster_count, sizeof(int8_t));
    if (!(*vfs)->data_bitmap) {
        printf(MEMORY_ERROR_MSG);
        fclose(file);
        return;
    }

    (*vfs)->inodes = calloc(sb->inode_cluster_count * (CLUSTER_SIZE / sizeof(inode)), sizeof(inode));
    if (!(*vfs)->inodes) {
        printf(MEMORY_ERROR_MSG);
        fclose(file);
        return;
    }

    // Всі inode вільні
    for (int i = 0; i < sb->inode_cluster_count * (CLUSTER_SIZE / sizeof(inode)); i++) {
        (*vfs)->inodes[i].nodeid = ID_ITEM_FREE;
    }

    inode *root = &((*vfs)->inodes[0]);
    root->nodeid = 0;
    root->isDirectory = true;
    root->references = 1;
    root->file_size = 0;

    rewind(file);
    fwrite(sb, sizeof(superblock), 1, file);

    fseek(file, sb->bitmap_start_address, SEEK_SET);
    fwrite((*vfs)->data_bitmap, sizeof(int8_t), sb->cluster_count, file);

    fseek(file, sb->inode_start_address, SEEK_SET);
    fwrite((*vfs)->inodes, sizeof(inode), sb->inode_cluster_count * (CLUSTER_SIZE / sizeof(inode)), file);

    fflush(file);

    (*vfs)->is_formatted = true;
    printf(FORMAT_SUCCESS_MSG);
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

