//
// Created by Denis on 03.11.2025.
//

#ifndef FS_ON_INODE_COMMANDS_H
#define FS_ON_INODE_COMMANDS_H

#include <structures.h>
#include <vfs.h>


extern Command commands[];
extern const int command_count;

void help();

#endif //FS_ON_INODE_COMMANDS_H