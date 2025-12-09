#ifndef FS_ON_INODE_STRUCTURES_H
#define FS_ON_INODE_STRUCTURES_H

#include <stdio.h>
#include <stdint-gcc.h>
#include <stdbool.h>
#include "constants.h"

/*
 * Represents one entry inside a directory.
 * Stores:
 *   - inode number of the file/directory
 *   - name of the entry
 *   - pointer to the next item (linked list)
 *
 * Used to build the list of files and subdirectories in memory.
 */
typedef struct DIR_ITEM {
    int32_t inode;
    char item_name[MAX_ITEM_NAME_LENGTH];
    struct DIR_ITEM *next;
} dir_item;

/*
 * Represents a directory in memory.
 * Contains:
 *   - pointer to parent directory
 *   - pointer to the dir_item describing itself
 *   - linked list of subdirectories (subdir)
 *   - linked list of files (file)
 *
 * This structure is not stored directly on disk — it is built
 * in memory when the VFS is loaded.
 */
typedef struct DIRECTORY {
    struct DIRECTORY *parent;
    dir_item *current;
    dir_item *subdir;
    dir_item *file;
} directory;

/*
 * Represents one i-node stored on disk.
 * Contains file metadata:
 *   - node ID
 *   - type (file or directory)
 *   - reference count
 *   - file size in bytes
 *   - up to 5 direct block references
 *   - one indirect and one double-indirect reference
 *
 * Defines how files/directories map to data clusters.
 */
typedef struct INODE {
    int32_t nodeid;
    bool isDirectory;
    int8_t references;
    int32_t file_size;
    int32_t direct1, direct2, direct3, direct4, direct5;
    int32_t indirect1, indirect2;
} inode;

/*
 * Superblock stores global metadata of the virtual filesystem.
 * Contains:
 *   - signature (for VFS identification)
 *   - total disk size and cluster layout
 *   - number of inodes and data clusters
 *   - how many clusters are used for bitmap and inodes
 *   - byte offsets where bitmap, inodes and data start
 *
 * Loaded at startup and used for all read/write operations.
 */
typedef struct SUPERBLOCK {
    char signature[SIGNATURE_LENGTH];

    int32_t disk_size;
    int32_t cluster_size;
    int32_t cluster_count;
    int32_t inode_count;
    int32_t bitmap_cluster_count;
    int32_t inode_cluster_count;
    int32_t data_cluster_count;

    int32_t bitmap_start_address;
    int32_t inode_start_address;
    int32_t data_start_address;
} superblock;

/*
 * Represents the entire Virtual File System instance.
 * Contains all runtime data:
 *   - loaded superblock
 *   - array of i-nodes
 *   - bitmap representing allocated data blocks
 *   - pointers to in-memory directory structures
 *   - currently active directory
 *   - file handle for the VFS file
 *   - name of the VFS file
 *
 * This structure is passed to all filesystem commands.
 */
typedef struct vfs {
    superblock *superblock;
    inode *inodes;
    int8_t *data_bitmap;
    bool is_formatted;
    directory *current_dir;
    directory **all_dirs;
    char *name;
    FILE *vfs_file;
} VFS;

/*
 * Defines a single command in the VFS shell.
 * Contains:
 *   - command name (string the user types)
 *   - whether VFS must be formatted before execution
 *   - required number of arguments
 *   - messages for missing arguments
 *   - pointer to handler function
 *   - help text shown in 'help' command
 */
typedef struct Command {
    char *name;
    bool requires_format;
    int expected_args;
    const char **arg_error_msgs;
    void (*handler)(VFS **vfs, char **args);
    const char *help;
} Command;

#endif //FS_ON_INODE_STRUCTURES_H
