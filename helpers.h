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

#endif //FS_ON_INODE_HELPERS_H


