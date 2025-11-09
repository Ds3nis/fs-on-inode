//
// Created by Denis on 05.11.2025.
//

#include <stdio.h>
#include <stdlib.h>
#include "vfs.h"
#include "structures.h"
#include <string.h>
#include "constants.h"
#include "commands.h"
#include "helpers.h"

void initialize_vfs(VFS **vfs, char *vfs_name) {
    *vfs = calloc(1, sizeof(VFS));

    if (!*vfs) {
        printf(MEMORY_ERROR_MSG);
        exit(1);
    }

    (*vfs)->name = strdup(vfs_name);

    FILE *file = fopen(vfs_name, "rb+");
    if (file == NULL) {
        needs_format(vfs);
        return;
    }

    (*vfs)->vfs_file = file;
    (*vfs)->is_formatted = true;


    if (!load_vfs(vfs)) {
        printf(VFS_ERROR, vfs_name);
        fclose(file);
        free(vfs);
        exit(1);
    }

    printf(VFS_LOAD_SUCCESS);
}

void needs_format(VFS **vfs) {
    printf(START_NEEDS_FORMAT_MSG);
    (*vfs)->is_formatted = false;
    (*vfs)->vfs_file = NULL;

    printf(FORMAT_VFS);
    char *choice_line = get_line();
    remove_nl_inplace(choice_line);

    if (choice_line[0] == 'y' || choice_line[0] == 'Y') {
        printf("Enter filesystem size in bytes: ");
        char *fs_size = get_line();
        remove_nl_inplace(fs_size);
        char *size_args[2] = {fs_size};
        cmd_format_vfs(vfs, size_args);
        free(fs_size);
    }

    free(choice_line);
}


bool load_vfs(VFS **vfs) {
    if (!vfs || !*vfs) {return false;}
    (*vfs)->superblock = calloc(1, sizeof(superblock));
    if (!(*vfs)->superblock) {
        printf(MEMORY_ERROR_MSG);
        return false;
    }

    rewind_vfs(vfs);
    vfs_read_sb(vfs);


    (*vfs)->data_bitmap = calloc((*vfs)->superblock->cluster_count, sizeof(int8_t));
    if (!(*vfs)->data_bitmap) {
        printf(MEMORY_ERROR_MSG);
        return false;
    }

    vfs_seek_from_start(vfs, (*vfs)->superblock->bitmap_start_address);
    vfs_read(vfs, (*vfs)->data_bitmap, sizeof(int8_t), (*vfs)->superblock->cluster_count);

    (*vfs)->inodes = calloc((*vfs)->superblock->inode_count, sizeof(inode));
    if (!(*vfs)->inodes) {
        printf(MEMORY_ERROR_MSG);
        return false;
    }

    (*vfs)->all_dirs = calloc((*vfs)->superblock->inode_count, sizeof(directory *));
    if (!(*vfs)->all_dirs) {
        printf(MEMORY_ERROR_MSG);
        return false;
    }

    for (int i = 0; i < (*vfs)->superblock->inode_count; i++) {
        vfs_read_inodes(vfs, i);
    }

    directory *root = calloc(1, sizeof(directory));
    if (!root) {
        printf(MEMORY_ERROR_MSG);
        return false;
    }

    dir_item *root_item = create_directory_item(0, "/");
    root->current = root_item;
    root->parent = root;
    root->subdir = NULL;
    root->file = NULL;

    (*vfs)->current_dir = root;
    (*vfs)->all_dirs[0] = root;
    (*vfs)->is_formatted = true;
    vfs_load_directories(vfs, root);
    printf(VFS_LOAD_SUCCESS);
    check_sb_info(vfs);
    return true;
}

void vfs_read_sb(VFS **vfs) {
    vfs_read((*vfs), (*vfs)->superblock->signature, sizeof(char), SIGNATURE_LENGTH);
    vfs_read_int32(vfs, &(*vfs)->superblock->disk_size);
    vfs_read_int32(vfs, &(*vfs)->superblock->cluster_size);
    vfs_read_int32(vfs, &(*vfs)->superblock->cluster_count);
    vfs_read_int32(vfs, &(*vfs)->superblock->inode_count);
    vfs_read_int32(vfs, &(*vfs)->superblock->bitmap_cluster_count);
    vfs_read_int32(vfs, &(*vfs)->superblock->inode_cluster_count);
    vfs_read_int32(vfs, &(*vfs)->superblock->data_cluster_count);
    vfs_read_int32(vfs, &(*vfs)->superblock->bitmap_start_address);
    vfs_read_int32(vfs, &(*vfs)->superblock->inode_start_address);
    vfs_read_int32(vfs, &(*vfs)->superblock->data_start_address);
}

void vfs_read_inodes(VFS **vfs, int index) {
    long base = (*vfs)->superblock->inode_start_address + (long)index * INODE_SIZE;
    vfs_seek_from_start(vfs, base);
    vfs_read_int32(vfs, &(*vfs)->inodes[index].nodeid);
    vfs_read_int8 (vfs, &(*vfs)->inodes[index].isDirectory);
    vfs_read_int8 (vfs, &(*vfs)->inodes[index].references);
    vfs_read_int32(vfs, &(*vfs)->inodes[index].file_size);
    vfs_read_int32(vfs, &(*vfs)->inodes[index].direct1);
    vfs_read_int32(vfs, &(*vfs)->inodes[index].direct2);
    vfs_read_int32(vfs, &(*vfs)->inodes[index].direct3);
    vfs_read_int32(vfs, &(*vfs)->inodes[index].direct4);
    vfs_read_int32(vfs, &(*vfs)->inodes[index].direct5);
    vfs_read_int32(vfs, &(*vfs)->inodes[index].indirect1);
    vfs_read_int32(vfs, &(*vfs)->inodes[index].indirect2);
}

void vfs_load_directories(VFS **vfs, directory *dir) {
    int i, j, block_count;
    int inode_count = 64;	/* Maximum count of i-nodes in one data block */
    int32_t *data_blocks;
    int32_t node_id;
    char filename[12];
    directory *new_directory;
    dir_item *item, *temp_subdir;
    dir_item **last_subdir_address = &(dir->subdir);
    dir_item **last_file_address = &(dir->file);

    /* Get data blocks of this directory */
    data_blocks = get_data_blocks(vfs, dir->current->inode, &block_count, NULL);

    for (i = 0; i < block_count; i++) {
        seek_data_cluster(vfs, data_blocks[i]);
        for (j = 0; j < inode_count; j++) {
            vfs_read_int32(vfs, &node_id);
            if (node_id > 0) { /* Invalid inode, or root */
                vfs_read(vfs, filename, sizeof(filename), 1);
                item = create_directory_item(node_id, filename);
                if ((*vfs)->inodes[node_id].isDirectory) {
                    *last_subdir_address = item;
                    last_subdir_address = &(item->next);
                } else {
                    *last_file_address = item;
                    last_file_address = &(item->next);
                }
            } else { /* Skip */
                seek_cur(vfs, sizeof(filename));
            }
        }
    }
    free(data_blocks);

    /* Recursively load subdirs */
    temp_subdir = dir->subdir;
    while (temp_subdir != NULL) {
        new_directory = calloc(1, sizeof(directory));
        new_directory->parent = dir;
        new_directory->current = temp_subdir;
        new_directory->subdir = NULL;
        new_directory->file = NULL;

        (*vfs)->all_dirs[temp_subdir->inode] = new_directory;
        load_directory_from_vfs(vfs, new_directory, temp_subdir->inode);

        temp_subdir = temp_subdir->next;
    }
}


int32_t *get_data_blocks(VFS** vfs, int32_t nodeid, int *block_count, int *rest) {
    int32_t *data_blocks;
    int32_t number;
    int i, tmp, counter;

    int max_numbers = 2 * (CLUSTER_SIZE / 4) + 5;	/*  Maximum data blocks */
    inode *node = &((*vfs)->inodes[nodeid]);

    if (node->isDirectory) {	/*  If item is directory we cannot determine size. We have to loop through all blocks until we reach end */
        counter = 0;
        data_blocks = calloc(max_numbers, sizeof(int32_t));

        if (node->direct1 != ID_ITEM_FREE) data_blocks[counter++] = node->direct1;
        if (node->direct2 != ID_ITEM_FREE) data_blocks[counter++] = node->direct2;
        if (node->direct3 != ID_ITEM_FREE) data_blocks[counter++] = node->direct3;
        if (node->direct4 != ID_ITEM_FREE) data_blocks[counter++] = node->direct4;
        if (node->direct5 != ID_ITEM_FREE) data_blocks[counter++] = node->direct5;

        if (node->indirect1 != ID_ITEM_FREE) {
            seek_data_cluster(vfs, node->indirect1);
            for (i = 0; i < CLUSTER_SIZE / 4; i++) {
                vfs_read_int32(vfs, &number);
                if (number > 0) {
                    data_blocks[counter++] = number;
                } else {
                    break;
                }
            }
        }

        if (node->indirect2 != ID_ITEM_FREE) {
            seek_data_cluster(vfs, node->indirect2);
            for (i = 0; i < CLUSTER_SIZE / 4; i++) {
                vfs_read_int32(vfs, &number);
                if (number > 0) {
                    data_blocks[counter++] = number;
                }
            }
        }

        *block_count = counter;
    } else {	/*  If item is file */
        *block_count = node->file_size / CLUSTER_SIZE;

        if (rest != NULL) {
            *rest = node->file_size % CLUSTER_SIZE;
        }

        if (node->file_size % CLUSTER_SIZE != 0) {      /* Need one more block to save rest of the file */
            (*block_count)++;
        }

        data_blocks = calloc((*block_count), sizeof(int32_t));

        data_blocks[0] = node->direct1;
        if (*block_count > 1) data_blocks[1] = node->direct2;
        if (*block_count > 2) data_blocks[2] = node->direct3;
        if (*block_count > 3) data_blocks[3] = node->direct4;
        if (*block_count > 4) data_blocks[4] = node->direct5;

        if (*block_count > 5) {
            if (*block_count > (CLUSTER_SIZE / 4) + 5) {	                                                            /*  Uses second indirect reference */
                seek_data_cluster(vfs, node->indirect1);                                                                /*  Move to first indirect reference */
                vfs_read(vfs, &data_blocks[5], sizeof(int32_t), CLUSTER_SIZE / sizeof(int32_t));            /*  Read all INT32 address from cluster */

                tmp = *block_count - (CLUSTER_SIZE / 4) - 5;                                                            /*  Number of blocks to read */
                seek_data_cluster(vfs, node->indirect2);                                                                /*  Move to second indirect reference */
                vfs_read(vfs, &data_blocks[(CLUSTER_SIZE / 4) + 5], sizeof(int32_t), tmp);                         /*  Read rest of indirect references from cluster */
            }
            else {	/*  Only first indirect reference is used */
                tmp = *block_count - 5;
                seek_data_cluster(vfs, node->indirect1);
                vfs_read(vfs, &data_blocks[5], sizeof(int32_t), tmp);
            }
        }
    }

    return data_blocks;
}

int seek_data_cluster(VFS **vfs, int block_number) {
    return seek_set(vfs, (*vfs)->superblock->data_start_address + block_number * CLUSTER_SIZE);
}

int seek_set(VFS **vfs, long int offset) {
    return fseek((*vfs)->vfs_file, offset, SEEK_SET);
}

int seek_cur(VFS **vfs, long int offset) {
    return fseek((*vfs)->vfs_file, offset, SEEK_CUR);
}

void load_directory_from_vfs(VFS** vfs, directory *dir, int id) {
    int i, j, block_count;
    int inode_count = 64;	/* Maximum count of i-nodes in one data block */
    int32_t *data_blocks;
    int32_t node_id;
    char filename[12];
    directory *new_directory;
    dir_item *item, *temp_subdir;
    dir_item **last_subdir_address = &(dir->subdir);
    dir_item **last_file_address = &(dir->file);

    /* Get data blocks of this directory */
    data_blocks = get_data_blocks(vfs, dir->current->inode, &block_count, NULL);

    for (i = 0; i < block_count; i++) {
        seek_data_cluster(vfs, data_blocks[i]);
        for (j = 0; j < inode_count; j++) {
            vfs_read_int32(vfs, &node_id);
            if (node_id > 0) { /* Invalid inode, or root */
                vfs_read(vfs, filename, sizeof(filename), 1);
                item = create_directory_item(node_id, filename);
                if ((*vfs)->inodes[node_id].isDirectory) {
                    *last_subdir_address = item;
                    last_subdir_address = &(item->next);
                } else {
                    *last_file_address = item;
                    last_file_address = &(item->next);
                }
            } else { /* Skip */
                seek_cur(vfs, sizeof(filename));
            }
        }
    }
    free(data_blocks);

    /* Recursively load subdirs */
    temp_subdir = dir->subdir;
    while (temp_subdir != NULL) {
        new_directory = calloc(1, sizeof(directory));
        new_directory->parent = dir;
        new_directory->current = temp_subdir;
        new_directory->subdir = NULL;
        new_directory->file = NULL;

        (*vfs)->all_dirs[temp_subdir->inode] = new_directory;
        load_directory_from_vfs(vfs, new_directory, temp_subdir->inode);

        temp_subdir = temp_subdir->next;
    }
}


size_t write_vfs(VFS **vfs, const void * ptr, size_t size, size_t count) {
    return fwrite(ptr, size, count, (*vfs)->vfs_file);
}


void write_inode_to_vfs(VFS **vfs, int id) {
    vfs_seek_from_start(vfs, (*vfs)->superblock->inode_start_address + id * INODE_SIZE);

    vfs_write_int32(vfs, &((*vfs)->inodes[id].nodeid));
    vfs_write_int8(vfs, &((*vfs)->inodes[id].isDirectory));
    vfs_write_int8(vfs, &((*vfs)->inodes[id].references));
    vfs_write_int32(vfs, &((*vfs)->inodes[id].file_size));
    vfs_write_int32(vfs, &((*vfs)->inodes[id].direct1));
    vfs_write_int32(vfs, &((*vfs)->inodes[id].direct2));
    vfs_write_int32(vfs, &((*vfs)->inodes[id].direct3));
    vfs_write_int32(vfs, &((*vfs)->inodes[id].direct4));
    vfs_write_int32(vfs, &((*vfs)->inodes[id].direct5));
    vfs_write_int32(vfs, &((*vfs)->inodes[id].indirect1));
    vfs_write_int32(vfs, &((*vfs)->inodes[id].indirect2));

    flush_vfs(vfs);
}

size_t vfs_write_int32(VFS **vfs, const void *ptr) {
    return fwrite(ptr, sizeof(int32_t), 1, (*vfs)->vfs_file);
}

size_t vfs_write_int8(VFS **vfs, const void *ptr) {
    return fwrite(ptr, sizeof(int8_t), 1, (*vfs)->vfs_file);
}

/*
 * Read raw data from VFS file
 */
size_t vfs_read(VFS **vfs, void *ptr, size_t size, size_t count) {
    return fread(ptr, size, count, (*vfs)->vfs_file);
}

/*
 * Read int8_t (1 byte)
 */
size_t vfs_read_int8(VFS **vfs, void *ptr) {
    return fread(ptr, sizeof(int8_t), 1, (*vfs)->vfs_file);
}

/*
 * Read int32_t (4 bytes)
 */
size_t vfs_read_int32(VFS **vfs, void *ptr) {
    return fread(ptr, sizeof(int32_t), 1, (*vfs)->vfs_file);
}

void rewind_vfs(VFS **vfs) {
    rewind((*vfs)->vfs_file);
}

void flush_vfs(VFS **vfs) {
    if (vfs && *vfs && (*vfs)->vfs_file) {
        fflush((*vfs)->vfs_file);
    }
}

int vfs_seek_from_start(VFS **vfs, long offset) {
    return fseek((*vfs)->vfs_file, offset, SEEK_SET);
}

void vfs_init_inodes(VFS **vfs) {
    // initialize all inodes as free
    for (int32_t i = 0; i < (*vfs)->superblock->inode_count; ++i) {
        (*vfs)->inodes[i].nodeid = ID_ITEM_FREE;
        (*vfs)->inodes[i].isDirectory = 0;
        (*vfs)->inodes[i].references = 0;
        (*vfs)->inodes[i].file_size = 0;
        (*vfs)->inodes[i].direct1 = ID_ITEM_FREE;
        (*vfs)->inodes[i].direct2 = ID_ITEM_FREE;
        (*vfs)->inodes[i].direct3 = ID_ITEM_FREE;
        (*vfs)->inodes[i].direct4 = ID_ITEM_FREE;
        (*vfs)->inodes[i].direct5 = ID_ITEM_FREE;
        (*vfs)->inodes[i].indirect1 = ID_ITEM_FREE;
        (*vfs)->inodes[i].indirect2 = ID_ITEM_FREE;
    }
}

void vfs_init_root_directory(VFS **vfs) {
    directory *root = calloc(1, sizeof(directory));
    dir_item *root_item = create_directory_item(0, "/");

    root->parent = root;
    root->current = root_item;
    root->subdir = NULL;
    root->file = NULL;

    (*vfs)->current_dir = root;
    (*vfs)->all_dirs[0] = root;

    (*vfs)->data_bitmap[0] = 1;
    inode *root_inode = &(*vfs)->inodes[0];
    root_inode->nodeid = 0;
    root_inode->isDirectory = 1;
    root_inode->references = 1;
    root_inode->file_size = 0;
    root_inode->direct1 = 0;
}



bool vfs_init_memory_structures(VFS **vfs, int32_t vfs_size) {
    (*vfs)->superblock = superblock_init(vfs_size);
    if (!(*vfs)->superblock) return false;

    (*vfs)->data_bitmap = calloc(1, (*vfs)->superblock->cluster_count);
    (*vfs)->inodes = calloc((*vfs)->superblock->inode_count, sizeof(inode));
    (*vfs)->all_dirs = calloc((*vfs)->superblock->inode_count, sizeof(directory *));

    if (!(*vfs)->data_bitmap || !(*vfs)->inodes || !(*vfs)->all_dirs)
        return false;

    vfs_init_inodes(vfs);

    vfs_init_root_directory(vfs);

    return true;
}

void vfs_write_superblock_to_file(VFS **vfs) {
    write_vfs(vfs, (*vfs)->superblock->signature, sizeof(char), SIGNATURE_LENGTH);
    vfs_write_int32(vfs, &(*vfs)->superblock->disk_size);
    vfs_write_int32(vfs, &(*vfs)->superblock->cluster_size);
    vfs_write_int32(vfs, &(*vfs)->superblock->cluster_count);
    vfs_write_int32(vfs, &(*vfs)->superblock->inode_count);
    vfs_write_int32(vfs, &(*vfs)->superblock->bitmap_cluster_count);
    vfs_write_int32(vfs, &(*vfs)->superblock->inode_cluster_count);
    vfs_write_int32(vfs, &(*vfs)->superblock->data_cluster_count);
    vfs_write_int32(vfs, &(*vfs)->superblock->bitmap_start_address);
    vfs_write_int32(vfs, &(*vfs)->superblock->inode_start_address);
    vfs_write_int32(vfs, &(*vfs)->superblock->data_start_address);
}

void vfs_write_bitmaps_to_file(VFS **vfs) {
    vfs_seek_from_start(vfs, (*vfs)->superblock->bitmap_start_address);
    fwrite((*vfs)->data_bitmap, sizeof(int8_t), (*vfs)->superblock->cluster_count, (*vfs)->vfs_file);
}

void vfs_write_inodes_to_file(VFS **vfs) {
    vfs_seek_from_start(vfs, (*vfs)->superblock->inode_start_address);
    for (int i = 0; i < (*vfs)->superblock->inode_count; i++) {
        write_inode_to_vfs(vfs, i);
    }
}