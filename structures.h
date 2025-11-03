//
// Created by Denis on 03.11.2025.
//

#ifndef FS_ON_INODE_STRUCTURES_H
#define FS_ON_INODE_STRUCTURES_H

#include <stdio.h>
#include <stdint-gcc.h>
#include <stdbool.h>
#include "constants.h"

typedef struct Command{
    const char *name;
    bool requires_format;
  	int expected_args;
    const char **arg_error_msgs;
    void (*handler)(VFS **vfs, char **args);
    const char *help;
} Command;


typedef struct DIR_ITEM {
    int32_t inode;
	char item_name[MAX_ITEM_NAME_LENGTH];
	struct DIR_ITEM *next;
} dir_item;

typedef struct DIRECTORY {
    struct DIRECTORY *parent;
    dir_item *current;
    dir_item *subdir;
    dir_item *file;
} directory;

typedef struct INODE {
    int32_t nodeid;
    bool isDirectory;
    int8_t references;
    int32_t file_size;
    int32_t direct1, direct2, direct3, direct4, direct5;
    int32_t indirect1, indirect2;
} inode;

typedef struct SUPERBLOCK {
    char signature[SIGNATURE_LENGTH];

    int32_t disk_size;              //celkova velikost VFS
    int32_t cluster_size;           //velikost clusteru
    int32_t cluster_count;          //pocet clusteru
    int32_t inode_count;			// Count of i-nodes
    int32_t bitmap_cluster_count;	// Count of clusters for bitmap
    int32_t inode_cluster_count;	// Count of clusters for i-nodes
    int32_t data_cluster_count;		// Count of clusters for data
    int32_t bitmap_start_address;   // Start address of the bitmap of the data blocks
    int32_t inode_start_address;    // Start address of the i-nodes
    int32_t data_start_address;     // Start address of data blocks
} superblock;

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

#endif //FS_ON_INODE_STRUCTURES_H