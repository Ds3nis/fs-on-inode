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
static const char *ERR_DIRNAME[]  = {DIRNAME_NOT_DEFINED_MSG};

Command commands[] = {
    {MKDIR_COMMAND, true,  1, ERR_DIRNAME,  cmd_mkdir, "Create new directory"},
    {LS_COMMAND, true, 0, {}, cmd_ls, "Lists the content of directory"},
    {"incp",  true,  2, ERR_SRC_DEST, cmd_incp,  "Copy file from host to VFS"},
    {"outcp", true,  2, ERR_SRC_DEST, cmd_outcp, "Copy file from VFS to host"},
    {"cp",    true,  2, ERR_SRC_DEST, cmd_cp,    "Copy file inside VFS"},
    {"format",false, 1, NULL,         cmd_format_vfs,"Format the virtual disk"},
    {"help",  false, 0, NULL,         cmd_help,  "Show available commands"},
    {"exit",  false, 0, NULL,         cmd_exit,  "Exit the program"},
};


const int command_count = sizeof(commands) / sizeof(Command);


bool validate_and_execute_command(VFS **vfs, Command *cmd, char *input) {
    if (cmd->requires_format && !(*vfs)->is_formatted) {
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

    rewind_vfs(vfs);
    vfs_write_superblock_to_file(vfs);

    vfs_write_bitmaps_to_file(vfs);

    vfs_write_inodes_to_file(vfs);

    flush_vfs(vfs);

    (*vfs)->is_formatted = true;


    printf(FORMAT_SUCCESS_MSG);
    check_sb_info(vfs);
}


void cmd_mkdir(VFS **vfs, char **args) {
    if (!vfs || !*vfs || !(*vfs)->is_formatted) {
        printf(VFS_NOT_INITIALIZED_MSG);
        return;
    }

    char *dirname = args[0];
    if (str_empty(dirname)) {
        printf(SRC_NOT_DEFINED_MSG);
        return;
    }

    directory *dir = NULL;
    char *name = NULL;

    if (parse_path(vfs, dirname, &name, &dir) == ERROR_CODE) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    printf("Directory where need to create a subdir Node id %d\n", dir->current->inode);
    printf("Item name %s \n", dir->current->item_name);
    printf("Node id %d \n", (*vfs)->inodes[dir->current->inode].nodeid);


    printf("%s", name);

    if (dir == NULL) dir = (*vfs)->current_dir;

    if (check_if_exists(dir, name)) {
        printf(FILE_EXISTS_MSG);
        return;
    }

    int free_inode = vfs_find_free_inode(vfs);
    if (free_inode == -1) {
        printf(NO_FREE_INODE);
        return;
    }

    int32_t *data_block = find_free_data_blocks(vfs, 1);
    if (!data_block) {
        printf(NOT_ENOUGH_BLOCKS_MSG);
        return;
    }


    inode *new_inode = &(*vfs)->inodes[free_inode];
    memset(new_inode, 0, sizeof(inode));
    new_inode->nodeid = free_inode;
    new_inode->isDirectory = true;
    new_inode->references = 1;
    new_inode->file_size = 0;
    new_inode->direct1 = data_block[0];
    new_inode->direct2 = new_inode->direct3 = new_inode->direct4 =
    new_inode->direct5 = new_inode->indirect1 = new_inode->indirect2 = ID_ITEM_FREE;

    dir_item *new_item = create_directory_item(free_inode, name);
    if (!new_item) {
        free(data_block);
        printf(MEMORY_ERROR_MSG);
        return;
    }

    directory *new_dir = calloc(1, sizeof(directory));
    if (!new_dir) {
        free(new_item);
        free(data_block);
        printf(MEMORY_ERROR_MSG);
        return;
    }


    new_dir->current = new_item;
    new_dir->parent = dir;
    new_dir->subdir = NULL;
    new_dir->file = NULL;
    (*vfs)->all_dirs[free_inode] = new_dir;

    (*vfs)->data_bitmap[data_block[0]] = 1;

    dir_item **temp = &(dir->subdir);
    while (*temp) temp = &((*temp)->next);
    *temp = new_item;

    if (update_directory_in_file(vfs, dir, new_item, true) == ERROR_CODE) {
        printf("Error writing directory structure to VFS.\n");
        free(data_block);
        return;
    }

    update_bitmap_in_file(vfs, new_item, 1, data_block, 1);

    write_inode_to_vfs(vfs, free_inode);
    flush_vfs(vfs);

    printf("Directory '%s' created successfully (inode %d)\n", name, free_inode);
    free(data_block);
    check_sb_info(vfs);
}

void cmd_ls(VFS **vfs, char **args) {

    directory *dir = NULL;
    char *name = NULL;

    if (!args || !args[0] || str_empty(args[0])) {
        dir = (*vfs)->current_dir;
    }
    else {
        if (parse_path(vfs, args[0], &name, &dir) == ERROR_CODE) {
            printf(PATH_NOT_FOUND_MSG);
            return;
        }

        if (!str_empty(name)) {
            dir_item *sub = dir->subdir;
            bool found = false;

            while (sub) {
                if (streq(sub->item_name, name)) {
                    dir = (*vfs)->all_dirs[sub->inode];
                    found = true;
                    break;
                }
                sub = sub->next;
            }

            if (!found) {
                printf(PATH_NOT_FOUND_MSG);
                return;
            }
        }
    }

    if (!dir) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    printf("Directories:\n");
    dir_item *sub = dir->subdir;
    if (!sub) printf("  <none>\n");

    while (sub) {
        printf("DIR: %s\n", sub->item_name);
        sub = sub->next;
    }

    printf("\nFiles:\n");
    dir_item *file = dir->file;
    if (!file) printf("  <none>\n");

    while (file) {
        printf("FILE: %s\n", file->item_name);
        file = file->next;
    }

    printf("\n");
}


void cmd_incp() {

}


void cmd_outcp() {

}

void cmd_cp(){

}



void cmd_format(){
}


void cmd_exit() {

}

