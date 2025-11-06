//
// Created by Denis on 03.11.2025.
//

#ifndef FS_ON_INODE_COMMANDS_H
#define FS_ON_INODE_COMMANDS_H

#include "structures.h"


extern Command commands[];
extern const int command_count;

bool validate_and_execute_command(VFS **vfs, Command *cmd, char *input);
int process_command_line(VFS **vfs, char *input);
void cmd_format_vfs(VFS **vfs, char **args);
void cmd_incp();
void cmd_outcp();
void cmd_cp();
void cmd_mkdir();
void cmd_format();
void cmd_help();
void cmd_exit();

#endif //FS_ON_INODE_COMMANDS_H