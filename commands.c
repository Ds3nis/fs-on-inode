//
// Created by Denis on 03.11.2025.
//

#include "commands.h"
#include "constants.h"
#include "vfs.h"
#include "helpers.h"
#include <string.h>
#include <stdlib.h>




static const char *ERR_DIRNAME[]  = {DIRNAME_NOT_DEFINED_MSG};
static const char *ERR_FS_SIZE[] = {FS_SIZE_NOT_DEFINED_MSG};
static const char *ERR_SRC_DEST[] = {DEST_NOT_DEFINED_MSG};
static const char *ERR_FILE_NAME[] = {FILE_OR_DIRECTORY_NOT_DEFINED};

Command commands[] = {
    {HELP_COMMAND,  false, 0, NULL, cmd_help,  "help --  Show available commands \n"},
    {FORMAT_COMMAND,false, 1, ERR_FS_SIZE, cmd_format_vfs,"format 600M  --  Formats the virtual file system (VFS)\n"},
    {MKDIR_COMMAND, true,  1, ERR_DIRNAME,  cmd_mkdir, "mkdir a1  --  Creates new directory a1\n"},
    {LS_COMMAND, true, 0, {}, cmd_ls, "ls a1  --  Lists the contents of the directory a1\n"},
    {RM_COMMAND, true, 1, ERR_DIRNAME, cmd_rmdir, "rmdir a1  --  Deletes the directory a1\n"},
    {PWD_COMMAND, true, 0, {}, cmd_pwd, "pwd  --  Lists the path to the current folder\n"},
    {CD_COMMAND, true, 1, ERR_SRC_DEST, cmd_cd, "cd a1  --  Changes the current folder to the directory at address a1\n"},
    {INFO_COMMAND, true, 1, ERR_FILE_NAME, cmd_info, "info a1/s1  --  Lists information about the given file/folder\n"},
    {EXIT_COMMAND, false, 0, {}, cmd_exit, "exit -- Exit filesystem \n"}
};


const int command_count = sizeof(commands) / sizeof(Command);


bool validate_and_execute_command(VFS **vfs, Command *cmd, char *input) {
    if (cmd->requires_format && (!vfs || !*vfs || !(*vfs)->is_formatted)) {
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
        // printf("Compare: '%s' and '%s'\n", command_name, commands[i].name);
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
    for (int i = 0; i < command_count; i++) {
        printf("%s",commands[i].help);
    }

    // printf("cp s1 s2  --  Copies the file from path s1 to path s2\n");
    // printf("mv s1 s2  --  Moves the file from path s1 to path s2\n");
    // printf("rm s1  --  Deletes file s1\n");
    // printf("mkdir a1  --  Creates directory a1\n");
    // printf("rmdir a1  --  Deletes the directory a1\n");
    // printf("ls a1  --  Lists the contents of the directory a1\n");
    // printf("cat s1  --  Lists the contents of the file s1\n");
    // printf("cd a1  --  Changes the current folder to the directory at address a1\n");
    // printf("pwd  --  Lists the path to the current folder\n");
    // printf("info a1/s1  --  Lists information about the given file/folder\n");
    // printf("incp s1 s2  --  Moves a file from the real file system to the virtual one\n");
    // printf("outcp s1 s2  --  Moves a file from the virtual file system to the real one\n");
    // printf("load s1  --  Reads commands line by line from file s1 from the real file system\n");
    // printf("format 600M  --  Formats the virtual file system (VFS)\n");
    // printf("check  --  Performs a consistency check of the file system\n");
    // printf("size inode_id 600M  --  Changes the size of the i-node with the given ID to the size specified by the parameter\n");
}




void cmd_format_vfs(VFS **vfs, char **args) {

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
    char *dirname = args[0];

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
    }else {
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

    print_directory_content(dir);
    printf("\n");
}

void cmd_rmdir(VFS **vfs, char **args) {
    char *dirname = args[0];

    directory *dir = NULL;
    char *name = NULL;


    if (parse_path(vfs, dirname, &name, &dir) == ERROR_CODE) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    dir_item *finding_item = find_diritem(dir->subdir, name);
    if (!finding_item) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    inode *nd = &(*vfs)->inodes[finding_item->inode];
    if (!nd->isDirectory) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    directory *finding_dir = (*vfs)->all_dirs[finding_item->inode];
    if (!finding_dir) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }
    if (finding_dir->file != NULL || finding_dir->subdir != NULL) {
        printf(DIR_NOT_EMPTY_MSG);
        return;
    }

    if (update_directory_in_file(vfs, dir, finding_item, false) == ERROR_CODE) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    update_bitmap_in_file(vfs, finding_item, 0, NULL, 0);

    nd->nodeid      = ID_ITEM_FREE;
    nd->isDirectory = 0;
    nd->references  = 0;
    nd->file_size   = 0;
    nd->direct1 = nd->direct2 = nd->direct3 = nd->direct4 = nd->direct5 = ID_ITEM_FREE;
    nd->indirect1 = nd->indirect2 = ID_ITEM_FREE;
    write_inode_to_vfs(vfs, finding_item->inode);

    dir_item *detached = remove_diritem(&dir->subdir, name);
    if ((*vfs)->all_dirs[finding_item->inode]) {
        free((*vfs)->all_dirs[finding_item->inode]);
        (*vfs)->all_dirs[finding_item->inode] = NULL;
    }
    free(detached);

    printf(OK_MSG);
}

void cmd_pwd(VFS **vfs, char **args) {
    directory *cur = (*vfs)->current_dir;
    char path[1024] = {0};
    char temp[256];

    if (cur->current->inode == 0) {
        printf("/\n");
        return;
    }

    while (cur != NULL) {
        if (cur->current->inode != 0) {
            snprintf(temp, sizeof(temp), "/%s", cur->current->item_name);

            char new_path[1024] = {0};
            strcpy(new_path, temp);
            strcat(new_path, path);
            strcpy(path, new_path);
        }

        if (cur == cur->parent) break;
        cur = cur->parent;
    }

    if (strlen(path) == 0) {
        strcpy(path, "/");
    }

    printf("%s\n", path);
}

void cmd_cd(VFS **vfs, char **args) {
    char *path = args[0];
    directory *dir = find_directory(vfs, path);
    if (dir == NULL) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    (*vfs)->current_dir = dir;
    printf(OK_MSG);
}

void cmd_info(VFS **vfs, char **args) {
    char *path = args[0];
    dir_item *item;
    directory *dir = NULL;
    char *name = NULL;

    if (streq(path, ".")) {
        print_dir_item_info(vfs, (*vfs)->current_dir->current);
        return;
    }

    if (parse_path(vfs, path, &name, &dir) == ERROR_CODE || !dir) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    if (dir == (*vfs)->all_dirs[0] && strlen(name) == 0) {
        print_dir_item_info(vfs, (*vfs)->all_dirs[0]->current);
        return;
    }

    item = find_item_by_name(dir->file, name);
    if (item != NULL) {
        print_dir_item_info(vfs, item);
        return;
    }

    item = find_item_by_name(dir->subdir, name);
    if (item != NULL) {
        print_dir_item_info(vfs, item);
        return;
    }

    printf(FILE_NOT_FOUND_MSG);
}

void cmd_exit(VFS **vfs, char **args) {
    printf("/--------------------\\\n");
    printf("|   END OF PROGRAM   |\n");
    printf("\\--------------------/\n\n");
}

void cmd_incp() {

}


void cmd_outcp() {

}

void cmd_cp(){

}



