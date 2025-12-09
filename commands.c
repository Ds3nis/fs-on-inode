//
// Created by Denis on 03.11.2025.
//

#include "commands.h"
#include "constants.h"
#include "vfs.h"
#include "helpers.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>


/* Error messages for missing arguments */
static const char *ERR_DIRNAME[]  = {DIRNAME_NOT_DEFINED_MSG};
static const char *ERR_FS_SIZE[] = {FS_SIZE_NOT_DEFINED_MSG};
static const char *ERR_SRC_DEST[] = {DEST_NOT_DEFINED_MSG};
static const char *ERR_FILE_NAME[] = {FILE_OR_DIRECTORY_NOT_DEFINED};
static const char *ERR_SRC_DEST_ARGS[] = {SRC_NOT_DEFINED_MSG,DEST_NOT_DEFINED_MSG};
static const char *ERR_FILE_NOT_FOUND[] = {FILE_NOT_FOUND_MSG};

// Maximum allowed nested load depth to prevent infinite recursion
static int load_depth = 0;

/* Command table: maps command name to handler, arg count, and help message */
Command commands[] = {
    {HELP_COMMAND,  false, 0, NULL, cmd_help,  "help --  Show available commands \n"},
    {FORMAT_COMMAND,false, 1, ERR_FS_SIZE, cmd_format_vfs,"format 600M  --  Formats the virtual file system (VFS)\n"},
    {MKDIR_COMMAND, true,  1, ERR_DIRNAME,  cmd_mkdir, "mkdir a1  --  Creates new directory a1\n"},
    {LS_COMMAND, true, 0, NULL, cmd_ls, "ls a1  --  Lists the contents of the directory a1\n"},
    {RMDIR_COMMAND, true, 1, ERR_DIRNAME, cmd_rmdir, "rmdir a1  --  Deletes the directory a1\n"},
    {PWD_COMMAND, true, 0, NULL, cmd_pwd, "pwd  --  Lists the path to the current folder\n"},
    {CD_COMMAND, true, 1, ERR_SRC_DEST, cmd_cd, "cd a1  --  Changes the current folder to the directory at address a1\n"},
    {INFO_COMMAND, true, 1, ERR_FILE_NAME, cmd_info, "info a1/s1  --  Lists information about the given file/folder\n"},
    {INCP_COMMAND, true, 2, ERR_SRC_DEST_ARGS, cmd_incp, "incp s1 s2  --  Moves a file from the real file system to the virtual one\n"},
    {OUTCP_COMMAND, true, 2, ERR_SRC_DEST_ARGS, cmd_outcp, "outcp s1 s2  --  Moves a file from the virtual file system to the real one\n"},
    {STATFS_COMMAND, true, 0, NULL, cmd_statfs, "statfs  --  displays file system statistics such as size, number ofoccupied and free blocks, number of occupied and free i-nodes, number of directories \n"},
    {LOAD_COMMAND, false, 1, ERR_FILE_NOT_FOUND, cmd_load, "load s1  --  Reads commands line by line from file s1 from the real file system\n"},
    {CAT_COMMAND, true, 1, ERR_FILE_NOT_FOUND, cmd_cat, "cat s1  --  Lists the contents of the file s1\n"},
    {CP_COMMAND, true, 2, ERR_SRC_DEST_ARGS, cmd_cp, "cp s1 s2  --  Copies the file from path s1 to path s2\n"},
    {MV_COMMAND, true, 2, ERR_SRC_DEST_ARGS, cmd_mv, "mv s1 s2  --  Přesune soubor na cestě s1 na cestu s2\n"},
    {RM_COMMAND, true, 1, ERR_FILE_NOT_FOUND, cmd_rm, "rm s1  --  Odstraní soubor s1\n"},
    {EXIT_COMMAND, false, 0, NULL, cmd_exit, "exit -- Exit filesystem \n"}
};

/* Number of commands in table */
const int command_count = sizeof(commands) / sizeof(Command);


/*
 * Validates argument count and executes command handler.
 */
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
 * Parses input and dispatches correct command.
 * Returns 1 if "exit" was executed.
 */
int process_command_line(VFS **vfs, char *input) {
    char *command_name = strtok(input, " ");

    if (!command_name) return false;
    for (int i = 0; i < command_count; i++) {
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
 * Prints all supported commands and their descriptions.
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
}



/*
 * Formats the virtual filesystem to a new size.
 * Creates a fresh superblock, bitmaps, inode table and root directory.
 * Overwrites the entire VFS file with zeros and writes all metadata structures.
 */
void cmd_format_vfs(VFS **vfs, char **args) {
    int32_t vfs_size = atoi(args[0]);
    if (vfs_size < MIN_FS) {
        printf(FORMAT_ERROR_SIZE_MSG);
        return;
    }

    // Open or recreate VFS file for writing
    FILE *file = fopen((*vfs)->name, "wb+");
    if (!file) {
        printf(OPEN_FILE_ERR_MSG);
        return;
    }
    (*vfs)->vfs_file = file;

    // Allocate internal structures: superblock, bitmaps, inode table, root
    if (!vfs_init_memory_structures(vfs, vfs_size)) {
        fclose(file);
        printf(MEMORY_ERROR_MSG);
        return;
    }

    // Clear the whole file with zeros cluster-by-cluster
    char buffer[CLUSTER_SIZE];
    memset(buffer, 0, CLUSTER_SIZE);
    for (int i = 0; i < (*vfs)->superblock->cluster_count; i++) {
        write_vfs(vfs, buffer, CLUSTER_SIZE, 1);
    }

    // Write metadata back to VFS file
    rewind_vfs(vfs);
    vfs_write_superblock_to_file(vfs);
    vfs_write_bitmaps_to_file(vfs);
    vfs_write_inodes_to_file(vfs);

    flush_vfs(vfs);

    (*vfs)->is_formatted = true;
    printf(FORMAT_SUCCESS_MSG);
    check_sb_info(vfs);
}


/*
 * Creates a new directory inside the virtual filesystem.
 * Resolves the target path, validates that the directory does not already exist,
 * allocates a free inode and one free data block, initializes directory metadata,
 * links the new directory into the parent directory structure, and writes
 * updated inode, bitmap, and directory contents back to the VFS file.
 */
void cmd_mkdir(VFS **vfs, char **args) {
    char *dirname = args[0];

    directory *dir = NULL;
    char *name = NULL;

    // Parse the path: split into parent directory and directory name
    if (parse_path(vfs, dirname, &name, &dir) == ERROR_CODE) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    if (dir == NULL) dir = (*vfs)->current_dir;

    // Prevent creation if file/folder already exists
    if (check_if_exists(dir, name)) {
        printf(FILE_EXISTS_MSG);
        return;
    }

    // Validate name length
    if (!validate_new_item_name(name))
        return;

    int inode_id;
    int32_t data_block;

    // Allocate inode + 1 data block
    if (!allocate_new_inode_and_block(vfs, &inode_id, &data_block)) {
        return;
    }

    init_directory_inode(&(*vfs)->inodes[inode_id], inode_id, data_block);


    // Allocate a free inode for the new directory
    int free_inode = vfs_find_free_inode(vfs);
    if (free_inode == -1) {
        printf(NO_FREE_INODE);
        return;
    }

    // Create directory structures in memory
    directory *new_dir = NULL;
    dir_item *new_item = NULL;

    if (!create_directory_structure(vfs, dir, name, inode_id, &new_dir, &new_item)) {
        return;
    }

    // Link directory item at end of subdir list
    add_item_to_list(&dir->subdir, new_item);
    (*vfs)->data_bitmap[data_block] = 1;
    if (sync_to_disk(vfs, dir, new_item, inode_id, &data_block, true) == false) {
        return;
    }

    printf(OK_MSG);
    check_sb_info(vfs);
}


/*
 * Lists the contents of a directory.
 * If no path is provided, lists the current directory.
 * If a path is provided, resolves it using parse_path(), and if the last
 * component is a subdirectory name, navigates into it before printing contents.
 */
void cmd_ls(VFS **vfs, char **args) {

    directory *dir = NULL;
    char *name = NULL;
    char *path = args[0];

    // If no argument → print current directory
    if (!path || str_empty(path)) {
        dir = (*vfs)->current_dir;
    } else {
        // Resolve path into (directory, final name segment)
        if (parse_path(vfs, path, &name, &dir) == ERROR_CODE) {
            printf(PATH_NOT_FOUND_MSG);
            return;
        }

        // If name is not empty → user probably pointed to a subdirectory
        if (!str_empty(name)) {

            // Find subdirectory with given name
            dir_item *found_item = find_directory_by_name(dir->subdir, name);
            if (!found_item) {
                printf(PATH_NOT_FOUND_MSG);
                return;
            }

            // Move into subdirectory (retrieve directory struct from all_dirs)
            dir = (*vfs)->all_dirs[found_item->inode];
        }
    }

    if (!dir) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    print_directory_content(dir);
    printf("\n");

}


/*
 * Removes an empty subdirectory:
 *  - Resolves the path and finds the directory entry
 *  - Ensures the target is a directory and it is empty
 *  - Removes dir_item from parent directory (memory + VFS file)
 *  - Frees inode and associated data blocks
 *  - Updates bitmap and saves changes to the VFS
 */
void cmd_rmdir(VFS **vfs, char **args) {
    char *dirname = args[0];

    directory *parent = NULL;
    char *name = NULL;

    // Resolve path: get parent folder and target folder name
    if (parse_path(vfs, dirname, &name, &parent) == ERROR_CODE) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    // Locate directory entry in parent
    dir_item *dir_item_to_remove = find_directory_by_name(parent->subdir, name);
    if (!dir_item_to_remove) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    // Validate that entry is a directory
    inode *target_inode = &(*vfs)->inodes[dir_item_to_remove->inode];
    if (!target_inode->isDirectory) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    // Retrieve full directory structure
    directory *target_dir = (*vfs)->all_dirs[dir_item_to_remove->inode];
    if (!target_dir) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    // Cannot delete non-empty directories
    if (target_dir->file != NULL || target_dir->subdir != NULL) {
        printf(DIR_NOT_EMPTY_MSG);
        return;
    }

    reset_inode(target_inode);
    // Save directory removal + bitmap update + inode write
    if (!sync_to_disk(vfs, parent, dir_item_to_remove, dir_item_to_remove->inode ,0,false)) {
        printf("Write error.\n");
        return;
    }

    // Remove dir_item from in-memory list
    dir_item *detached = remove_diritem(&parent->subdir, name);

    // Free directory structure from memory
    if ((*vfs)->all_dirs[dir_item_to_remove->inode]) {
        free((*vfs)->all_dirs[dir_item_to_remove->inode]);
        (*vfs)->all_dirs[dir_item_to_remove->inode] = NULL;
    }
    free(detached);
    printf(OK_MSG);
}



/*
 * Prints the absolute path of the current working directory.
 *
 * The function walks upward through parent directories until it reaches root.
 * Each directory name is prepended to the result path to reconstruct the full
 * hierarchy. Root (inode 0) is handled as a special case and prints "/".
 */
void cmd_pwd(VFS **vfs, char **args) {
    directory *cur = (*vfs)->current_dir;
    char path[MAX_PATH_LENGTH] = {0};
    char temp[256];

    // If we are at the root directory, simply print "/"
    if (cur->current->inode == 0) {
        printf("/\n");
        return;
    }

    // Traverse upward through parents and build the path from bottom to top
    while (cur != NULL) {
        if (cur->current->inode != 0) {
            // Prepend "/name" to the existing path
            snprintf(temp, sizeof(temp), "/%s", cur->current->item_name);

            char new_path[1024] = {0};
            strcpy(new_path, temp);
            strcat(new_path, path);
            strcpy(path, new_path);
        }

        // Stop once we reach the root that points to itself
        if (cur == cur->parent) break;
        cur = cur->parent;
    }

    // If path is empty for some reason, fallback to "/"
    if (strlen(path) == 0) {
        strcpy(path, "/");
    }

    printf("%s\n", path);
}


/*
 * Changes the current working directory.
 * The function resolves the given path using find_directory(),
 * validates that the directory exists, and then updates the
 * VFS current_dir pointer to the newly selected directory.
 */
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


/*
 * Prints detailed information about a file or directory specified by path.
 * Steps:
 *   - Special case: "." prints info about the current directory
 *   - Resolves the path into a parent directory + name
 *   - Handles the case when the root directory itself is queried
 *   - Searches for the name first among files, then subdirectories
 *   - If found, passes the matching dir_item to the printer function
 *   - If no matching entry exists, prints FILE_NOT_FOUND_MSG
 */
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

    item = find_directory_by_name(dir->file, name);
    if (item != NULL) {
        print_dir_item_info(vfs, item);
        return;
    }

    item = find_directory_by_name(dir->subdir, name);
    if (item != NULL) {
        print_dir_item_info(vfs, item);
        return;
    }

    printf(FILE_NOT_FOUND_MSG);
}


// void cmd_incp(VFS **vfs, char **args) {
//     int32_t *blocks, inode_id;
//     int i, block_count, real_block_count, tmp, last_block_index;
//     char *name;
//     directory *dir;
//     dir_item **new_dir_item;
//     char *filepath_dest = args[1];
//     char *filepath_src = args[0];
//
//     /* Find destination directory */
//     if (parse_path(vfs, filepath_dest, &name, &dir) == -1) {
//         printf(FILE_NOT_FOUND_MSG);
//         return;
//     }
//
//
//     if (strlen(name) >= MAX_ITEM_NAME_LENGTH) {
//         printf(FILENAME_TOO_LONG_MSG);
//     }
//
//     check_if_exists(dir, name);
//
//     if (strlen(name) == 0) {
//         name = strrchr(filepath_src, '/');
//
//         if (name == NULL) {
//             name = filepath_src;
//         } else {
//             name++; /* Skip the slash */
//         }
//     }
//
//     FILE *src_file = fopen(filepath_src, "rb");
//     if (src_file == NULL){
//         FILE_NOT_FOUND_MSG;
//     }
//
//     /* Get size of the copied file */
//     int fd = fileno(src_file);
//     struct stat buf;
//     fstat(fd, &buf);
//     int32_t file_size = buf.st_size;
//
//     block_count = file_size / CLUSTER_SIZE;
//
//     if (file_size % CLUSTER_SIZE != 0) block_count++;
//
//     real_block_count = get_block_count_with_indirect(block_count);
//
//     blocks = find_free_data_blocks(vfs, real_block_count);
//     if (blocks  == NULL) {
//         printf(NOT_ENOUGH_BLOCKS_MSG);
//     }
//
//     /* Get ID of a free i-node */
//     inode_id = vfs_find_free_inode(vfs);
//
//
//     if (inode_id == ERROR_CODE) {
//         printf(NO_FREE_INODE_MSG);
//         fclose(src_file);
//         return;
//     }
//
//     new_dir_item = &(dir->file);
//     while (*new_dir_item != NULL) {        /* Loop to the end of the list */
//         new_dir_item = &((*new_dir_item)->next);
//     }
//
//     *new_dir_item = create_directory_item(inode_id, name);
//
//     /* Initialize i-node */
//     last_block_index = initialize_inode(vfs, inode_id, file_size, block_count, blocks);
//
//     update_bitmap_in_file(vfs, *new_dir_item, 1, blocks, block_count);
//     write_inode_to_vfs(vfs, inode_id);
//     update_directory_in_file(vfs, dir, *new_dir_item, true); /* TODO */
//     update_sizes_in_file(vfs, dir, file_size);
//
//
//     /* Initialize buffer */
//     char buffer[CLUSTER_SIZE];
//     memset(buffer, 0, CLUSTER_SIZE);
//
//
//     /* Read block, seek in VFS and write into VFS */
//     for (i = 0; i < block_count - 1; i++) {
//         fread(buffer, sizeof(buffer), 1, src_file);
//         seek_data_cluster(vfs, blocks[i]);
//         write_vfs(vfs, buffer, sizeof(buffer), 1);
//     }
//
//     /* Read last block */
//     tmp = get_last_block_size(file_size % CLUSTER_SIZE);
//
//     /* TODO CHANGE EVERYWHERE TO PART BUFFER */
//     char part_buffer[tmp];
//
//     fread(buffer, sizeof(part_buffer), 1, src_file);
//     seek_data_cluster(vfs, blocks[last_block_index]);
//     write_vfs(vfs, part_buffer, sizeof(part_buffer), 1);
//
//     flush_vfs(vfs);
//
//     /* Cleanup */
//     fclose(src_file);
//     free(blocks);
//
//     printf(OK_MSG);
// }
//
//
// void cmd_outcp(VFS **vfs, char **args) {
//     int i, block_count, rest, last_block_size;
//     int32_t *blocks;
//     char *name;
//     char buffer[CLUSTER_SIZE];
//     directory *dir;
//     dir_item *item;
//     FILE *f;
//
//     char *filepath_dest = args[1];
//     char *filepath_src = args[0];
//
//     /* Parse source path */
//     if(parse_path(vfs, filepath_src, &name, &dir) == ERROR_CODE) {
//         printf(FILE_NOT_FOUND_MSG);
//         return;
//     }
//
//     /* Find item */
//     item = find_diritem(dir->file, name);
//     if (item == NULL) {
//         printf(FILE_NOT_FOUND_MSG);
//         return;
//     }
//
//     /* Open output file */
//     f = fopen(filepath_dest, "wb");
//     if (f == NULL) {
//         printf(FILE_NOT_FOUND_MSG);
//         return;
//     }
//
//     blocks = get_data_blocks(vfs, item->inode, &block_count, &rest);
//
//     /* Copy data blocks */
//     for (i = 0; i < block_count - 1; i++) {
//         seek_data_cluster(vfs, blocks[i]);
//         vfs_read(vfs, buffer, sizeof(buffer), 1);
//         fwrite(buffer, sizeof(buffer), 1, f);
//         fflush(f);
//     }
//
//     /* Copy last block */
//     last_block_size = get_last_block_size(rest);
//     char part_buffer[last_block_size];
//
//     seek_data_cluster(vfs, blocks[block_count - 1]);
//     vfs_read(vfs, part_buffer, sizeof(part_buffer), 1);
//     fwrite(part_buffer, sizeof(part_buffer), 1, f);
//
//     fflush(f);
//     flush_vfs(vfs);
//     fclose(f);
//     free(blocks);
//
//     printf(OK_MSG);
// }

/*
 * Prints general statistics about the virtual filesystem.
 *
 * Information shown:
 *   - total filesystem size (based on cluster count)
 *   - number of data blocks and how many are used/free (via bitmap)
 *   - total inode count and their usage
 *   - number of directories currently allocated in memory
 *
 * The function iterates over inode and bitmap structures to gather
 * live statistics, making it useful for diagnostics and monitoring
 * filesystem fragmentation and resource availability.
 */
void cmd_statfs(VFS **vfs, char **args) {
    superblock *sb = (*vfs)->superblock;

    int used_blocks = count_used_blocks(*vfs);
    int free_blocks = sb->data_cluster_count - used_blocks;

    int used_inodes = count_used_inodes(*vfs);
    int free_inodes = sb->inode_count - used_inodes;

    int directory_count = count_directories(*vfs);

    int total_size = sb->cluster_count * sb->cluster_size;

    printf("\n--- FILESYSTEM STATISTICS ---\n");
    printf("Filesystem size:        %d bytes\n", total_size);

    printf("Data blocks total:      %d\n", sb->data_cluster_count);
    printf(" ├─ Used blocks:        %d\n", used_blocks);
    printf(" └─ Free blocks:        %d\n", free_blocks);

    printf("Inodes total:           %d\n", sb->inode_count);
    printf(" ├─ Used inodes:        %d\n", used_inodes);
    printf(" └─ Free inodes:        %d\n", free_inodes);

    printf("Directory count:        %d\n", directory_count);

    printf("------------------------------\n\n");
}

/*
 * Executes a batch of commands from a text file.
 *  - Opens the specified file and reads it line-by-line
 *  - Removes trailing newline characters
 *  - Skips empty lines
 *  - Prints each command before executing it (simple tracing)
 *  - Dispatches each command through process_command_line()
 */
void cmd_load(VFS **vfs, char **args) {
    char *filename = args[0];
    FILE *command_file = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    printf("[DEBUG] Loading file: '%s'\n", filename);

    // Prevent infinite nested loads
    if (load_depth >= MAX_LOAD_DEPTH) {
        printf("ERROR: Maximum load depth exceeded (%d)\n", MAX_LOAD_DEPTH);
        return;
    }

    // Check that file exists on real disk
    if (!file_exists(filename)) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    command_file = fopen(filename, "r");
    if (!command_file) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    printf("[DEBUG] File opened successfully\n");
    load_depth++;
    printf("[DEBUG] Load depth: %d\n", load_depth);

    while ((read = getline(&line, &len, command_file)) != -1) {
        printf("[DEBUG] Read line: '%s' (length: %zd)\n", line, read);
        remove_nl_inplace(line);

        // Skip empty lines
        if (strlen(line) == 0) {
            printf("[DEBUG] Skipping empty line\n");
            continue;
        }

        // Show the command for user context
        printf("> %s\n", line);

        // Execute the command the same way as user input
        process_command_line(vfs, line);
    }

    printf("[DEBUG] Finished reading file\n");
    load_depth--;

    fclose(command_file);
    free(line);

    printf(OK_MSG);
}


/*
 * Prints the content of a file from the virtual filesystem.
 *  - Resolves the given path into parent directory + file name
 *  - Validates that the target exists and is not a directory
 *  - Uses stream_file_content() to read file data block-by-block
 *
 * Handles errors such as: incorrect path, missing file, or type mismatch.
 */
void cmd_cat(VFS **vfs, char **args) {
    char *path = args[0];
    char *name = NULL;
    directory *parent = NULL;
    if (parse_path(vfs, path, &name, &parent) == ERROR_CODE || !parent || str_empty(name)) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    dir_item *item = find_directory_by_name(parent->file, name);
    if (!item) { printf(FILE_NOT_FOUND_MSG); return; }

    inode *nd = &(*vfs)->inodes[item->inode];
    if (nd->isDirectory) { printf(FILE_NOT_FOUND_MSG); return; }

    if (!stream_file_content(vfs, item)) {
        printf("\nError: Failed to read file content\n");
    }
}

void cmd_cp(VFS **vfs, char **args){
    char* src_path = args[0];
    char* dest_path = args[1];
    char *src_name, *dest_name;
    directory *src_dir, *dest_dir;
    dir_item *src_item, **dest_item_ptr;
    int32_t *src_blocks, *dest_blocks;
    int block_count = 0, rest = 0, real_block_count = 0;
    int32_t inode_id;
    char buffer[CLUSTER_SIZE];

    if (src_path == NULL || dest_path == NULL) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    if (parse_path(vfs, src_path, &src_name, &src_dir) == ERROR_CODE) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    src_item = find_item_in_directory(src_dir, src_name);
    if (src_item == NULL) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    if (parse_path(vfs, dest_path, &dest_name, &dest_dir) == ERROR_CODE) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    if (str_empty(dest_name)) {
        dest_name = src_name;
    }

    if (check_if_exists(dest_dir, dest_name)) {
        printf(FILE_EXISTS_MSG);
        return;
    }

    src_blocks = get_data_blocks(vfs, src_item->inode, &block_count, &rest);
    if (!src_blocks || block_count <= 0) {
        printf(FILE_NOT_FOUND_MSG);
        free(src_blocks);
        return;
    }

    real_block_count = get_block_count_with_indirect(block_count);
    dest_blocks = find_free_data_blocks(vfs, real_block_count);
    if (!dest_blocks) {
        printf(NOT_ENOUGH_BLOCKS_MSG);
        free(src_blocks);
        return;
    }

    inode_id = vfs_find_free_inode(vfs);
    if (inode_id < 0) {
        printf(NO_FREE_INODE_MSG);
        free(src_blocks);
        free(dest_blocks);
        return;
    }

    dest_item_ptr = &(dest_dir->file);
    while (*dest_item_ptr) dest_item_ptr = &((*dest_item_ptr)->next);
    *dest_item_ptr = create_directory_item(inode_id, dest_name);

    int32_t file_size = (*vfs)->inodes[src_item->inode].file_size;

    (void)initialize_inode(vfs, inode_id, file_size, block_count, dest_blocks);

    update_bitmap_in_file(vfs, *dest_item_ptr, 1, dest_blocks, block_count);
    write_inode_to_vfs(vfs, inode_id);
    update_sizes_in_file(vfs, dest_dir, file_size);
    update_directory_in_file(vfs, dest_dir, *dest_item_ptr, 1);

    for (int i = 0; i < block_count - 1; i++) {
        seek_data_cluster(vfs, src_blocks[i]);
        size_t read_bytes = vfs_read(vfs, buffer, 1, CLUSTER_SIZE);
        if (read_bytes < CLUSTER_SIZE) memset(buffer + read_bytes, 0, CLUSTER_SIZE - read_bytes);

        seek_data_cluster(vfs, dest_blocks[i]);
        write_vfs(vfs, buffer, 1, CLUSTER_SIZE);
    }

    int last_block_size = (rest > 0) ? rest : CLUSTER_SIZE;
    memset(buffer, 0, CLUSTER_SIZE);

    seek_data_cluster(vfs, src_blocks[block_count - 1]);
    size_t read_tail = vfs_read(vfs, buffer, 1, last_block_size);

    seek_data_cluster(vfs, dest_blocks[block_count - 1]);
    write_vfs(vfs, buffer, 1, read_tail);

    flush_vfs(vfs);

    free(src_blocks);
    free(dest_blocks);

    printf(OK_MSG);
}

/*
 * Moves a file inside the virtual filesystem.
 *  - Resolves the source and destination paths
 *  - Validates that the source exists and the destination does not
 *  - Prevents circular directory moves (moving a folder into itself)
 *  - Removes the item from the source directory in both memory and VFS
 *  - Updates file sizes in all parent directories
 *  - Renames the item if needed and inserts it into the destination directory
 *  - Writes all changes back to the VFS file structure
 *
 * Supports renaming as: mv a/b.txt a/c.txt
 * Does NOT currently support moving directories (only files).
 */
void cmd_mv(VFS **vfs, char **args) {
    char *src_name, *dest_name;
    directory *src_dir, *dest_dir;
    dir_item *item_to_move = NULL;

    char *src_path = args[0];
    char *dest_path = args[1];

    if (parse_path(vfs, src_path, &src_name, &src_dir) == ERROR_CODE) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    if (parse_path(vfs, dest_path, &dest_name, &dest_dir) == ERROR_CODE) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    if (str_empty(dest_name)) {
        dest_name = src_name;
    }

    dir_item *src_item = find_directory_by_name(src_dir->file, src_name);
    if (src_item == NULL) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    if (is_circular_move(vfs, src_item->inode, dest_dir)) {
        printf(CIRCULAR_ERROR);
        return;
    }

    if (check_if_exists(dest_dir, dest_name)) {
        printf(FILE_EXISTS_MSG);
        return;
    }

    if (!remove_item_from_list(&(src_dir->file), src_name, &item_to_move)) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    int32_t file_size = (*vfs)->inodes[item_to_move->inode].file_size;
    update_sizes_in_file(vfs, src_dir, -file_size);

    if (update_directory_in_file(vfs, src_dir, item_to_move, false) == ERROR_CODE) {
        add_item_to_list(&(src_dir->file), item_to_move);
        update_sizes_in_file(vfs, src_dir, file_size);
        printf(REMOVE_FAILED);
        return;
    }

    safe_copy_name(item_to_move->item_name, dest_name, MAX_ITEM_NAME_LENGTH);

    add_item_to_list(&(dest_dir->file), item_to_move);

    update_sizes_in_file(vfs, dest_dir, file_size);

    if (update_directory_in_file(vfs, dest_dir, item_to_move, true) == ERROR_CODE) {
        printf(MOVE_FILE_FAILED);
        return;
    }

    printf(OK_MSG);
}

void cmd_rm(VFS **vfs, char **args) {
    char *name = NULL;
    directory *parent = NULL;
    char *path = args[0];

    if (parse_path(vfs, path, &name, &parent) == ERROR_CODE || !parent || str_empty(name)) {
        printf(PATH_NOT_FOUND_MSG);
        return;
    }

    dir_item *item = find_directory_by_name(parent->file, name);
    if (!item) {
        printf(FILE_NOT_FOUND_MSG);
        return;
    }

    inode *nd = &(*vfs)->inodes[item->inode];
    if (nd->isDirectory) {
        printf(RM_DIRECTORY_MSG, name);
        return;
    }

    if (nd->nodeid == ID_ITEM_FREE) {
        printf("ERROR: Invalid inode (file system corruption?)\n");
        return;
    }

    int result;
    if (nd->references > 1) {
        result = handle_hardlink_removal(vfs, parent, item, nd);
    } else {
        result = handle_full_file_removal(vfs, parent, item, nd);
    }

    if (result == NO_ERROR_CODE) {
        printf(OK_MSG);
    } else {
        printf("ERROR: Failed to remove file\n");
    }
}

/*
 * Exit program
 */
void cmd_exit() {
    printf("/--------------------\\\n");
    printf("|   END OF PROGRAM   |\n");
    printf("\\--------------------/\n\n");
}
