//
// Created by Denis on 03.11.2025.
//

#ifndef FS_ON_INODE_CONSTANTS_H
#define FS_ON_INODE_CONSTANTS_H


#define SUPERBLOCK_SIGNATURE "Khuda Denys"
#define SIGNATURE_LENGTH 12
#define MAX_ITEM_NAME_LENGTH 12
#define CLUSTER_SIZE            4096    // 4 kB
#define INT32_COUNT_IN_BLOCK (CLUSTER_SIZE / 4)
#define INODE_SIZE              40
#define MIN_FS           102400
#define NEGATIVE_SIZE_OF_INT32  -4
#define ID_ITEM_FREE            -1
#define EMPTY_ADDRESS           0
#define DIR_ENTRY_SIZE (sizeof(int32_t) + MAX_ITEM_NAME_LENGTH)
#define MAX_DIR_ENTRIES_PER_CLUSTER (CLUSTER_SIZE / DIR_ENTRY_SIZE)


#define FORMAT_VFS "Do you want to format new filesystem? (y/n): "
#define SRC_NOT_DEFINED_MSG "\n"
#define DEST_NOT_DEFINED_MSG "Destination path not defined \n"
#define FILE_OR_DIRECTORY_NOT_DEFINED "Directory or file path not defined \n"
#define DIRNAME_NOT_DEFINED_MSG "Directory name is not define \n"
#define FORMAT_ERROR_SIZE_MSG "Cannot format, too small filesystem. Specify size at least 102400 bits.\n"
#define OPEN_FILE_ERR_MSG ""
#define FS_SIZE_NOT_DEFINED_MSG "File size not defined.\n"
#define FORMAT_SUCCESS_MSG ""
#define ERROR_SB_READING "Error reading superblock.\n"
#define ERROR_LOADING "Error: directory structure corrupted or incomplete.\n"
#define NO_FREE_INODE "No free inode available.\n"
#define OK_MSG "OK \n"
#define ERROR_CODE      -1
#define NO_ERROR_CODE    0
#define PATH_NOT_FOUND_MSG      "PATH NOT FOUND (target path does not exist)\n"
#define FILE_NOT_FOUND_MSG      "FILE NOT FOUND (source does not exist)\n"
#define FILE_EXISTS_MSG         "EXIST (cannot create, already exists)\n"
#define DIR_NOT_EMPTY_MSG       "NOT EMPTY (directory contains subdirectories or files)\n"
#define NOT_ENOUGH_BLOCKS_MSG "Not enough blocks found. Probably no more space available. \n"


#define EXIT_COMMAND "exit"
#define HELP_COMMAND "help"
#define FORMAT_COMMAND "format"
#define DEBUG_COMMAND "debug"
#define INCP_COMMAND "incp"
#define OUTCP_COMMAND "outcp"
#define MKDIR_COMMAND "mkdir"
#define CD_COMMAND "cd"
#define LS_COMMAND "ls"
#define CAT_COMMAND "cat"
#define PWD_COMMAND "pwd"
#define INFO_COMMAND "info"
#define RM_COMMAND "rm"
#define RMDIR_COMMAND "rmdir"
#define CP_COMMAND "cp"
#define MV_COMMAND "mv"
#define LOAD_COMMAND "load"
#define CHECK_COMMAND "check"
#define SIZE_COMMAND "size"



#define START_NEEDS_FORMAT_MSG "VFS File not found, needs formatting.\n"
#define MEMORY_ERROR_MSG "Cannot allocate memory.\n"
#define UNKNOWN_COMMAND_MSG "Unknown command: '%s'. Type 'help' for list of commands.\n"
#define VFS_NOT_INITIALIZED_MSG "Error: Virtual filesystem not initialized. Use 'format' first.\n"
#define VFS_LOADING "Loading virtual filesystem: %s\n"
#define VFS_ERROR "Error loading virtual filesystem: %s\n"
#define VFS_LOAD_SUCCESS "VFS successfully initialized.\n"

#endif //FS_ON_INODE_CONSTANTS_H