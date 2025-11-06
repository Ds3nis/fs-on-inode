//
// Created by Denis on 03.11.2025.
//

#ifndef FS_ON_INODE_VFS_H
#define FS_ON_INODE_VFS_H

#include "structures.h"
#include "constants.h"

void initialize_vfs(VFS **vfs, char *vfs_name);
bool load_vfs(VFS **vfs);
void needs_format(VFS **vfs);

#endif //FS_ON_INODE_VFS_H