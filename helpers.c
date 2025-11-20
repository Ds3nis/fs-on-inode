#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "helpers.h"
#include <stdio.h>

#include "vfs.h"


/*
 * Returns if string str1 equals str2
 */
bool streq(char *str1, char *str2) {
    if (strcmp(str1, str2) == 0) {
        return true;
    } else {
        return false;
    }
}


/*
 * Returns true if string is empty
 */
bool str_empty(char *str) {
    if (str == NULL || streq(str, "")) {
        return true;
    }

    return false;
}


char * get_line() {
    char * line = calloc(1, 100), * linep = line;
    size_t lenmax = 100, len = lenmax;
    int c;

    if(line == NULL) {
        return NULL;
    }

    while(true) {
        c = fgetc(stdin);
        if(c == EOF)
            break;

        if(--len == 0) {
            len = lenmax;
            char * linen = realloc(linep, lenmax *= 2);

            if(linen == NULL) {
                free(linep);
                return NULL;
            }
            line = linen + (line - linep);
            linep = linen;
        }

        if((*line++ = c) == '\n')
            break;
    }
    *line = '\0';
    return linep;
}

/*
 * Removes new line characters from string
 */
void remove_nl_inplace(char *message) {
    int len = strlen(message);
    int index = 0;
    for (int i = 0; i < len; i++) {
        if (message[i] != 10 && message[i] != 13) {
            message[index++] = message[i];
        }
    }
    message[index] = '\0';
}

superblock *superblock_init(int32_t vfs_size) {
    superblock *sb = calloc(1, sizeof(superblock));
    if (!sb) {
        return NULL;
    }

    memset(sb->signature, 0, SIGNATURE_LENGTH);
    strncpy(sb->signature, SUPERBLOCK_SIGNATURE, SIGNATURE_LENGTH - 1);

    sb->disk_size = vfs_size;
    sb->cluster_size = CLUSTER_SIZE;
    sb->cluster_count = vfs_size / CLUSTER_SIZE;

    // bitmap needs one byte per data cluster; compute how many clusters needed to store bitmap
    int32_t bitmap_bytes = sb->cluster_count * (int)sizeof(int8_t);
    int32_t bitmap_cluster_count = (bitmap_bytes + CLUSTER_SIZE - 1) / CLUSTER_SIZE;
    if (bitmap_cluster_count < 1) bitmap_cluster_count = 1;

    // allocate ~10% clusters for inodes (at least 1)
    int32_t inode_cluster_count = (int32_t)(sb->cluster_count * 0.10);
    if (inode_cluster_count < 1) inode_cluster_count = 1;

    // now data clusters are the rest
    int32_t data_cluster_count = sb->cluster_count - bitmap_cluster_count - inode_cluster_count;
    if (data_cluster_count < 1) {
        printf("Not enough space for data clusters (choose larger size).\n");
        exit(1);
    }

    // compute inode_count (how many inodes we can store)
    int32_t inodes_per_cluster = CLUSTER_SIZE / (int)sizeof(inode);
    int32_t inode_count = inode_cluster_count * inodes_per_cluster;

    int32_t bitmap_start_address = CLUSTER_SIZE;
    int32_t inode_start_address = bitmap_start_address + bitmap_cluster_count * CLUSTER_SIZE;
    int32_t data_start_address = inode_start_address + inode_cluster_count * CLUSTER_SIZE;


    sb->inode_count = inode_count;
    sb->bitmap_cluster_count = bitmap_cluster_count;
    sb->inode_cluster_count = inode_cluster_count;
    sb->data_cluster_count = data_cluster_count;
    sb->bitmap_start_address = bitmap_start_address;
    sb->inode_start_address = inode_start_address;
    sb->data_start_address = data_start_address;

    return sb;
}


dir_item *create_directory_item(int32_t inode_id, const char *name) {
    // create dir_item for root (inode 0, name "/")
    dir_item *dir_item = calloc(1, sizeof(dir_item));
    if (!dir_item) {return NULL; }


    dir_item->inode = inode_id;
    memset(dir_item->item_name, 0, MAX_ITEM_NAME_LENGTH);
    // set name to "/" (or empty string depending on convention)
    strncpy(dir_item->item_name, name, MAX_ITEM_NAME_LENGTH - 1);
    dir_item->next = NULL;

    return dir_item;
}

/*
 * Shows debug information
 */
void check_sb_info(VFS **vfs) {
    printf("Signature : %s\n"
           "Disk size: %d\n"
           "Cluster size: %d\n"
           "Cluster count: %d\n"
           "Max Inode Count: %d\n"
           "Bitmap cluster count: %d\n"
           "Inode cluster count: %d\n"
           "Data cluster count: %d\n"
           "Bitmap start address: %d\n"
           "Inode start address: %d\n"
           "Data start address: %d\n",
           (*vfs)->superblock->signature,
           (*vfs)->superblock->disk_size,
           (*vfs)->superblock->cluster_size,
           (*vfs)->superblock->cluster_count,
           (*vfs)->superblock->inode_count,
           (*vfs)->superblock->bitmap_cluster_count,
           (*vfs)->superblock->inode_cluster_count,
           (*vfs)->superblock->data_cluster_count,
           (*vfs)->superblock->bitmap_start_address,
           (*vfs)->superblock->inode_start_address,
           (*vfs)->superblock->data_start_address);

    printf("\nVytvořené Inode :\n");
    for (unsigned long i = 0 ; i < (*vfs)->superblock->inode_count; i++){
        if ((*vfs)->inodes[i].nodeid == ID_ITEM_FREE) {
            continue;
        }

        printf("%d ",(*vfs)->inodes[i].nodeid);
    }

    printf("\nData bitmapa:\n");
    for (int i = 0 ; i < (*vfs)->superblock->data_cluster_count; i++){
        printf("%d", (*vfs)->data_bitmap[i]);
    }
    printf("\n");
}

int parse_path(VFS **vfs, char *path, char **name, directory **dir) {
    if (str_empty(path)) {
        return ERROR_CODE;
    }

    if (streq(path, "..")) {
        *dir = (*vfs)->current_dir->parent;
        *name = "";
        return NO_ERROR_CODE;
    }

    char *slash = strrchr(path, '/');

    if (slash == NULL) {
        *dir = (*vfs)->current_dir;
        *name = path;
    } else {
        *name = slash + 1;
        int len = (int)(slash - path);
        char buff[256];
        memset(buff, 0, sizeof(buff));
        strncpy(buff, path, len);

        if (path[0] == '/' && len == 1) {
            strcpy(buff, "/");
        }

        *dir = find_directory(vfs, buff);
        if (*dir == NULL) {
            return ERROR_CODE;
        }
    }

    return NO_ERROR_CODE;
}

directory *find_directory(VFS **vfs, char *path) {
    if (str_empty(path)) {
        return NULL;
    }

    directory *current;

    if (path[0] == '/') {
        current = (*vfs)->all_dirs[0];
        path++;
        if (*path == '\0') {
            return current;
        }
    } else {
        current = (*vfs)->current_dir;
    }

    char buff[256];
    strncpy(buff, path, sizeof(buff) - 1);
    buff[sizeof(buff) - 1] = '\0';

    char *token = strtok(buff, "/");
    while (token != NULL) {
        if (streq(token, ".")) {
        } else if (streq(token, "..")) {
            current = current->parent;
        } else {
            dir_item *found = find_item_by_name(current->subdir, token);
            if (found == NULL) {
                return NULL;
            }
            current = (*vfs)->all_dirs[found->inode];
            if (current == NULL) {
                return NULL;
            }
        }
        token = strtok(NULL, "/");
    }

    return current;
}

dir_item *find_item_by_name(dir_item *first, const char *name) {
    if (first == NULL || name == NULL) {
        return NULL;
    }

    dir_item *current = first;
    while (current != NULL) {
        if (strncmp(current->item_name, name, MAX_ITEM_NAME_LENGTH) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

bool check_if_exists(directory *dir, char *name) {
    dir_item *item;

    /* Loop through files of the directory */
    item = dir->file;
    while (item != NULL) {
        if (streq(name, item->item_name)) {
            return true;
        }
        item = item->next;
    }

    /* Loop through subdirs of the directory */
    item = dir->subdir;
    while (item != NULL) {
        if (streq(name, item->item_name)) {
            return true;
        }
        item = item->next;
    }
    return false;
}

int32_t *find_free_data_blocks(VFS** vfs, int count) {
    int i, found_blocks = 0;
    int32_t *blocks = calloc(count, sizeof(int32_t));

    if (blocks == NULL) {
        printf(MEMORY_ERROR_MSG);
        return NULL;
    }

    /* Find all data blocks */
    for (i = 1; i < (*vfs)->superblock->data_cluster_count; i++) { /* Skip root */
        if ((*vfs)->data_bitmap[i] == 0) {
            blocks[found_blocks] = i;
            found_blocks++;
            if (found_blocks == count) {
                return blocks;
            }
        }
    }

    free(blocks);
    return NULL;
}

void print_directory_content(directory *dir) {
    printf("Directories:\n");
    dir_item *sub = dir->subdir;
    if (!sub) printf("  <none>\n");

    while (sub) {
        printf("DIR: %s\n", sub->item_name);
        sub = sub->next;
    }

    printf("\nFiles:\n");
    dir_item *file = dir->file;
    if (!file) printf("  <none>\n");

    while (file) {
        printf("FILE: %s\n", file->item_name);
        file = file->next;
    }
}

dir_item *find_diritem(dir_item *item,char *name) {
    if (item == NULL || name == NULL) {
        return NULL;
    }

    dir_item *current = item;
    while (current != NULL) {
        if (strncmp(current->item_name, name, MAX_ITEM_NAME_LENGTH) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

dir_item *remove_diritem(dir_item **head, const char *name) {
    if (head == NULL || *head == NULL || name == NULL) {
        return NULL;
    }

    dir_item *current = *head;
    dir_item *prev = NULL;

    while (current != NULL) {
        if (strncmp(current->item_name, name, MAX_ITEM_NAME_LENGTH) == 0) {
            if (prev == NULL) {
                *head = current->next;
            } else {
                prev->next = current->next;
            }
            current->next = NULL;
            return current;
        }
        prev = current;
        current = current->next;
    }

    return NULL;
}

void print_dir_item_info(VFS **vfs, dir_item *item) {
    inode node = (*vfs)->inodes[item->inode];

    printf("Name: %s\n", item->item_name);
    printf("Size: %d B\n", node.file_size);
    printf("i-node: %d\n", node.nodeid);
    printf("References: %d\n", node.references);
    printf(node.isDirectory ? "Type: Directory\n" : "Type: File\n");

    printf("Direct: ");
    int printed = 0;
    if (node.direct1 != ID_ITEM_FREE) { printf("%d", node.direct1); printed = 1; }
    if (node.direct2 != ID_ITEM_FREE) { printf(printed ? ", %d" : "%d", node.direct2); printed = 1; }
    if (node.direct3 != ID_ITEM_FREE) { printf(printed ? ", %d" : "%d", node.direct3); printed = 1; }
    if (node.direct4 != ID_ITEM_FREE) { printf(printed ? ", %d" : "%d", node.direct4); printed = 1; }
    if (node.direct5 != ID_ITEM_FREE) { printf(printed ? ", %d" : "%d", node.direct5); printed = 1; }
    if (!printed) printf("NONE");
    printf("\n");

    printf("Indirect 1: ");
    if (node.indirect1 != ID_ITEM_FREE) {
        printf("(%d): ", node.indirect1);
        seek_data_cluster(vfs, node.indirect1);
        int32_t number;
        int first = 1;
        for (int i = 0; i < INT32_COUNT_IN_BLOCK; i++) {
            vfs_read_int32(vfs, &number);
            if (number == EMPTY_ADDRESS) break;
            printf(first ? "%d" : ", %d", number);
            first = 0;
        }
        if (first) printf("EMPTY");
        printf("\n");
    } else {
        printf("FREE\n");
    }

    printf("Indirect 2: ");
    if (node.indirect2 != ID_ITEM_FREE) {
        printf("(%d): ", node.indirect2);
        seek_data_cluster(vfs, node.indirect2);
        int32_t number;
        int first = 1;
        for (int i = 0; i < INT32_COUNT_IN_BLOCK; i++) {
            vfs_read_int32(vfs, &number);
            if (number == EMPTY_ADDRESS) break;
            printf(first ? "%d" : ", %d", number);
            first = 0;
        }
        if (first) printf("EMPTY");
        printf("\n");
    } else {
        printf("FREE\n");
    }

    printf("\n");
}