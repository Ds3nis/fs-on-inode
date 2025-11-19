//
// Created by Denis on 06.11.2025.
//

#ifndef FS_ON_INODE_HELPERS_H
#define FS_ON_INODE_HELPERS_H

#include <stdint.h>
#include "structures.h"


bool streq(char *str1, char *str2);
bool str_empty(char *str);
char * get_line();
void remove_nl_inplace(char *message);
superblock *superblock_init(int32_t vfs_size);
dir_item *create_directory_item(int32_t inode_id, const char *name);
void check_sb_info(VFS **vfs);
int parse_path(VFS **vfs, char *path, char **name, directory **dir);
directory *find_directory(VFS **vfs, char *path);
dir_item *find_item_by_name(dir_item *first, const char *name);
bool check_if_exists(directory *dir, char *name);
int32_t *find_free_data_blocks(VFS** vfs, int count);

#endif //FS_ON_INODE_HELPERS_H


