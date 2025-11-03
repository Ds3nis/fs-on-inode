//
// Created by Denis on 03.11.2025.
//

#include "commands.h"
#include "vfs.h"



static const char *ERR_SRC_DEST[] = {SRC_NOT_DEFINED_MSG, DEST_NOT_DEFINED_MSG};
static const char *ERR_DIRNAME[]  = {SRC_NOT_DEFINED_MSG};

Command commands[] = {
    {"incp",  true,  2, ERR_SRC_DEST, cmd_incp,  "Copy file from host to VFS"},
    {"outcp", true,  2, ERR_SRC_DEST, cmd_outcp, "Copy file from VFS to host"},
    {"cp",    true,  2, ERR_SRC_DEST, cmd_cp,    "Copy file inside VFS"},
    {"mkdir", true,  1, ERR_DIRNAME,  cmd_mkdir, "Create new directory"},
    {"format",false, 1, NULL,         cmd_format,"Format the virtual disk"},
    {"help",  false, 0, NULL,         cmd_help,  "Show available commands"},
    {"exit",  false, 0, NULL,         cmd_exit,  "Exit the program"},
};


const int command_count = sizeof(commands) / sizeof(Command);


bool validate_and_execute_command(VFS **vfs, Command *cmd, char *input) {
    if (cmd->requires_format && (*vfs == NULL || !(*vfs)->is_formatted)) {
        printf(VFS_NOT_INITIALIZED_MSG);
        return false;
    }

    char *args[10] = {0};
    for (int i = 0; i < cmd->expected_args; i++) {
        args[i] = strtok(NULL, " ");
        if (strempty(args[i])) {
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
 * Main command processor — looks up command in the table and runs handler
 */
int process_command_line(VFS **vfs, char *input) {
    char *command_name = strtok(input, " ");
    if (!command_name) return false;

    for (int i = 0; i < command_count; i++) {
        if (strcmp(command_name, commands[i].name) == 0) {
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
 * Shows how to use the program
 */
void help() {
    printf("/----------\\\n");
    printf("|   HELP   |\n");
    printf("\\----------/\n");
    printf("\n");
    printf("Starting the program: \n\n");
    printf("./vfs [filesystem_name]\n\n");
    printf("Available commands: \n\n");
    printf("cp s1 s2  --  Copies the file from path s1 to path s2\n");
    printf("mv s1 s2  --  Moves the file from path s1 to path s2\n");
    printf("rm s1  --  Deletes file s1\n");
    printf("mkdir a1  --  Creates directory a1\n");
    printf("rmdir a1  --  Deletes the directory a1\n");
    printf("ls a1  --  Lists the contents of the directory a1\n");
    printf("cat s1  --  Lists the contents of the file s1\n");
    printf("cd a1  --  Changes the current folder to the directory at address a1\n");
    printf("pwd  --  Lists the path to the current folder\n");
    printf("info a1/s1  --  Lists information about the given file/folder\n");
    printf("incp s1 s2  --  Moves a file from the real file system to the virtual one\n");
    printf("outcp s1 s2  --  Moves a file from the virtual file system to the real one\n");
    printf("load s1  --  Reads commands line by line from file s1 from the real file system\n");
    printf("format 600M  --  Formats the virtual file system (VFS)\n");
    printf("check  --  Performs a consistency check of the file system\n");
    printf("size inode_id 600M  --  Changes the size of the i-node with the given ID to the size specified by the parameter\n");
}
