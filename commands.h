//
// Created by Denis on 03.11.2025.
//

#ifndef FS_ON_INODE_COMMANDS_H
#define FS_ON_INODE_COMMANDS_H

#include "structures.h"

/* Command table: maps command name to handler, arg count, and help message */
extern Command commands[];

/* Number of commands in table */
extern const int command_count;

/*
 * Validates argument count and executes command handler.
 */
bool validate_and_execute_command(VFS **vfs, Command *cmd);

/*
 * Parses input and dispatches correct command.
 * Returns 1 if "exit" was executed.
 */
int process_command_line(VFS **vfs, char *input);

/*
 * Prints all supported commands and their descriptions.
 */
void cmd_help();

/*
 * Formats the virtual filesystem to a new size.
 * Creates a fresh superblock, bitmaps, inode table and root directory.
 * Overwrites the entire VFS file with zeros and writes all metadata structures.
 */
void cmd_format_vfs(VFS **vfs, char **args);

/*
 * Creates a new directory inside the virtual filesystem.
 * Resolves the target path, validates that the directory does not already exist,
 * allocates a free inode and one free data block, initializes directory metadata,
 * links the new directory into the parent directory structure, and writes
 * updated inode, bitmap, and directory contents back to the VFS file.
 */
void cmd_mkdir(VFS **vfs, char **args);

/*
 * Lists the contents of a directory.
 * If no path is provided, lists the current directory.
 * If a path is provided, resolves it using parse_path(), and if the last
 * component is a subdirectory name, navigates into it before printing contents.
 */
void cmd_ls(VFS **vfs, char **args);

/*
 * Removes an empty subdirectory:
 *  - Resolves the path and finds the directory entry
 *  - Ensures the target is a directory and it is empty
 *  - Removes dir_item from parent directory (memory + VFS file)
 *  - Frees inode and associated data blocks
 *  - Updates bitmap and saves changes to the VFS
 */
void cmd_rmdir(VFS **vfs, char **args);

/*
 * Prints the absolute path of the current working directory.
 *
 * The function walks upward through parent directories until it reaches root.
 * Each directory name is prepended to the result path to reconstruct the full
 * hierarchy. Root (inode 0) is handled as a special case and prints "/".
 */
void cmd_pwd(VFS **vfs, char **args);

/*
 * Changes the current working directory.
 * The function resolves the given path using find_directory(),
 * validates that the directory exists, and then updates the
 * VFS current_dir pointer to the newly selected directory.
 */
void cmd_cd(VFS **vfs, char **args);

/*
 * Prints detailed information about a file or directory specified by path.
 */
void cmd_info(VFS **vfs, char **args);

/*
 * Prints general statistics about the virtual filesystem.
 */
void cmd_statfs(VFS **vfs, char **args);

/*
 * Executes a batch of commands from a text file.
 */
void cmd_load(VFS **vfs, char **args);

void cmd_incp(VFS **vfs, char **args);
void cmd_outcp(VFS **vfs, char **args);

/*
 * Prints the content of a file from the virtual filesystem.
 */
void cmd_cat(VFS **vfs, char **args);

void cmd_cp(VFS **vfs, char **args);

/*
 * Moves a file inside the virtual filesystem.
 */
void cmd_mv(VFS **vfs, char **args);

/*
 * Removes a file from the virtual filesystem.
 */
void cmd_rm(VFS **vfs, char **args);

void cmd_ln(VFS **vfs, char **args);

/*
 * Exit program
 */
void cmd_exit();

#endif //FS_ON_INODE_COMMANDS_H