//
// Created by Denis on 06.11.2025.
//

#ifndef FS_ON_INODE_HELPERS_H
#define FS_ON_INODE_HELPERS_H

bool streq(char *str1, char *str2);
bool str_empty(char *str);
char * get_line();
void remove_nl_inplace(char *message);

#endif //FS_ON_INODE_HELPERS_H


