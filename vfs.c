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
    if (!vfs_read_sb(vfs)) {
        printf(ERROR_SB_READING);
        return false;
    }

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


    for (int i = 0; i < (*vfs)->superblock->inode_count; i++) {
        vfs_read_inodes(vfs, i);
    }


    (*vfs)->all_dirs = calloc((*vfs)->superblock->inode_count, sizeof(directory *));
    if (!(*vfs)->all_dirs) {
        printf(MEMORY_ERROR_MSG);
        return false;
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
    if (!vfs_load_directories(vfs, root)) {
        printf(ERROR_LOADING);
        return false;
    }
    printf(VFS_LOAD_SUCCESS);
    check_sb_info(vfs);
    return true;
}

bool vfs_read_sb(VFS **vfs) {

    size_t bytes_read = 0;

    bytes_read = vfs_read(vfs, (*vfs)->superblock->signature, sizeof(char), SIGNATURE_LENGTH);
    if (bytes_read != SIGNATURE_LENGTH) {
        printf("Error: Failed to read superblock signature\n");
        return false;
    }

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


    return true;
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


bool vfs_load_directories(VFS **vfs, directory *root) {
    if (!vfs || !*vfs || !root) return false;
    return load_directory_from_vfs(vfs, root, root->current->inode);
}

bool load_directory_from_vfs(VFS **vfs, directory *dir, int inode_id) {
    if (!vfs || !*vfs || !dir) return false;

    int block_count = 0;
    int32_t *data_blocks = get_data_blocks(vfs, inode_id, &block_count, NULL);
    if (!data_blocks || block_count <= 0) {
        free(data_blocks);
        return true;
    }

    dir_item **last_subdir = &dir->subdir;
    dir_item **last_file = &dir->file;

    for (int i = 0; i < block_count; i++) {
        seek_data_cluster(vfs, data_blocks[i]);

        for (int j = 0; j < MAX_DIR_ENTRIES_PER_CLUSTER; j++) {
            int32_t node_id;
            char filename[MAX_ITEM_NAME_LENGTH] = {0};

            if (!vfs_read_int32(vfs, &node_id)) break;
            if (!vfs_read(vfs, filename, sizeof(filename), 1)) break;

            if (node_id <= 0) continue;

            dir_item *item = create_directory_item(node_id, filename);
            if (!item) continue;

            if ((*vfs)->inodes[node_id].isDirectory) {
                *last_subdir = item;
                last_subdir = &item->next;
            } else {
                *last_file = item;
                last_file = &item->next;
            }
        }
    }

    free(data_blocks);

    for (dir_item *sub = dir->subdir; sub; sub = sub->next) {
        directory *new_dir = calloc(1, sizeof(directory));
        if (!new_dir) {
            printf(MEMORY_ERROR_MSG);
            return false;
        }

        new_dir->parent = dir;
        new_dir->current = sub;
        (*vfs)->all_dirs[sub->inode] = new_dir;

        if (!load_directory_from_vfs(vfs, new_dir, sub->inode)) {
            return false;
        }
    }
    return true;
}



int32_t *get_data_blocks(VFS **vfs, int32_t nodeid, int *block_count, int *rest) {
    inode *node = &(*vfs)->inodes[nodeid];
    if (!node) return NULL;

    int max_blocks = 5 + (CLUSTER_SIZE / 4) + (CLUSTER_SIZE / 4) * (CLUSTER_SIZE / 4);
    int32_t *blocks = calloc(max_blocks, sizeof(int32_t));
    if (!blocks) return NULL;

    int count = 0;


    int32_t *directs[] = {
        &node->direct1, &node->direct2, &node->direct3,
        &node->direct4, &node->direct5
    };
    for (int i = 0; i < 5; i++) {
        if (*directs[i] != ID_ITEM_FREE)
            blocks[count++] = *directs[i];
    }

    if (node->indirect1 != ID_ITEM_FREE) {
        seek_data_cluster(vfs, node->indirect1);
        for (int i = 0; i < CLUSTER_SIZE / 4; i++) {
            int32_t ref = 0;
            vfs_read_int32(vfs, &ref);
            if (ref > 0) blocks[count++] = ref;
            else break;
        }
    }

    if (node->indirect2 != ID_ITEM_FREE) {
        seek_data_cluster(vfs, node->indirect2);
        for (int i = 0; i < CLUSTER_SIZE / 4; i++) {
            int32_t ref_block = 0;
            vfs_read_int32(vfs, &ref_block);
            if (ref_block > 0) {
                seek_data_cluster(vfs, ref_block);
                for (int j = 0; j < CLUSTER_SIZE / 4; j++) {
                    int32_t ref = 0;
                    vfs_read_int32(vfs, &ref);
                    if (ref > 0) blocks[count++] = ref;
                    else break;
                }
            }
        }
    }

    *block_count = count;
    return blocks;
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


size_t write_vfs(VFS **vfs, const void * ptr, size_t size, size_t count) {
    return fwrite(ptr, size, count, (*vfs)->vfs_file);
}


/*
 * Writes a single inode to its correct position on disk.
 * Converts the in-memory inode struct into serialized fields.
 */
void write_inode_to_vfs(VFS **vfs, int id) {
    vfs_seek_from_start(vfs, (*vfs)->superblock->inode_start_address + id * INODE_SIZE);

    vfs_write_int32(vfs, &((*vfs)->inodes[id].nodeid));
    vfs_write_int8 (vfs, &((*vfs)->inodes[id].isDirectory));
    vfs_write_int8 (vfs, &((*vfs)->inodes[id].references));
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

/*
 * Resets the entire inode table.
 * Marks each inode as unused and clears all block references.
 */
void vfs_init_inodes(VFS **vfs) {
    for (int32_t i = 0; i < (*vfs)->superblock->inode_count; ++i) {
        inode *n = &(*vfs)->inodes[i];
        n->nodeid = ID_ITEM_FREE;
        n->isDirectory = 0;
        n->references = 0;
        n->file_size = 0;

        n->direct1 = n->direct2 = n->direct3 = n->direct4 = n->direct5 = ID_ITEM_FREE;
        n->indirect1 = n->indirect2 = ID_ITEM_FREE;
    }
}

/*
 * Creates and initializes the root directory (inode 0).
 * Links directory structures in memory, marks cluster 0 as used,
 * and sets up the root inode to be a directory located in cluster 0.
 */
bool vfs_init_root_directory(VFS **vfs) {
    directory *root = calloc(1, sizeof(directory));
    dir_item *root_item = create_directory_item(0, "/");
    if (!root_item || !root) return false;

    root->parent = root;      // root's parent is itself
    root->current = root_item;
    root->subdir = NULL;
    root->file = NULL;

    (*vfs)->current_dir = root;
    (*vfs)->all_dirs[0] = root;

    // Mark cluster 0 as used
    memset((*vfs)->data_bitmap, 0, (*vfs)->superblock->cluster_count);
    (*vfs)->data_bitmap[0] = 1;

    // Configure root inode
    inode *root_inode = &(*vfs)->inodes[0];
    root_inode->nodeid = 0;
    root_inode->isDirectory = 1;
    root_inode->references = 1;
    root_inode->file_size = 0;
    root_inode->direct1 = 0;

    return true;
}


/*
 * Allocates and initializes core memory structures of the filesystem.
 * Builds the superblock, bitmap array, inode table, directory table,
 * and initializes all inodes including the root directory.
 */
bool vfs_init_memory_structures(VFS **vfs, int32_t vfs_size) {
    (*vfs)->superblock = superblock_init(vfs_size);
    if (!(*vfs)->superblock) return false;

    (*vfs)->data_bitmap = calloc(1, (*vfs)->superblock->cluster_count);
    (*vfs)->inodes = calloc((*vfs)->superblock->inode_count, sizeof(inode));
    (*vfs)->all_dirs = calloc((*vfs)->superblock->inode_count, sizeof(directory *));

    if (!(*vfs)->data_bitmap || !(*vfs)->inodes || !(*vfs)->all_dirs)
        return false;

    // Mark all inodes as free
    vfs_init_inodes(vfs);

    // Create root inode + root directory structure
    return vfs_init_root_directory(vfs);
}


/*
 * Writes superblock metadata to the beginning of the VFS file.
 * Saves all structural fields in a fixed layout.
 */
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


/*
 * Stores the full data bitmap into the VFS file.
 * Bitmap is written starting from bitmap_start_address.
 */
void vfs_write_bitmaps_to_file(VFS **vfs) {
    vfs_seek_from_start(vfs, (*vfs)->superblock->bitmap_start_address);
    fwrite((*vfs)->data_bitmap, sizeof(int8_t),
           (*vfs)->superblock->cluster_count, (*vfs)->vfs_file);
}


/*
 * Writes the entire inode table to the VFS file.
 * Exports every inode sequentially using write_inode_to_vfs().
 */
void vfs_write_inodes_to_file(VFS **vfs) {
    vfs_seek_from_start(vfs, (*vfs)->superblock->inode_start_address);
    for (int i = 0; i < (*vfs)->superblock->inode_count; i++) {
        write_inode_to_vfs(vfs, i);
    }
}


/*
 * Searches the inode table for the first free inode.
 * Returns its index or ID_ITEM_FREE if none are available.
 */
int32_t vfs_find_free_inode(VFS **vfs) {
    for (int i = 0; i < (*vfs)->superblock->inode_count; i++) {
        if ((*vfs)->inodes[i].nodeid == ID_ITEM_FREE) {
            return i;
        }
    }
    return ID_ITEM_FREE;
}


/*
 * Writes directory structure changes to the VFS file.
 * If create == true, inserts a new directory entry.
 * Otherwise removes an existing entry.
 * Delegates the actual work to helper functions responsible for file layout.
 */
int update_directory_in_file(VFS** vfs, directory *dir, dir_item *item, bool create) {
    if (create) {
        return create_directory_in_file(vfs, dir, item);
    } else {
        return remove_directory_from_file(vfs, dir, item);
    }
}



int create_directory_in_file(VFS** vfs, directory *dir, dir_item *item) {
    int i, j, block_count;
    int32_t *blocks, *free_block;
    int max_items_in_block = 64;
    int32_t nodeid;
    inode *dir_node;

    /* Get data blocks */
    blocks = get_data_blocks(vfs, dir->current->inode, &block_count, NULL);

    for (i = 0; i < block_count; i++) {
        seek_data_cluster(vfs, blocks[i]);
        for (j = 0; j < max_items_in_block; j++) {
            vfs_read_int32(vfs, &nodeid);
            if (nodeid == 0) {
                seek_cur(vfs, -4); /* Rewind back for a size of int32_t (4 bytes) */
                vfs_write_int32(vfs, &(item->inode)); /* Store address of inode */
                write_vfs(vfs, item->item_name, sizeof(item->item_name), 1); /* Store name of folder */
                flush_vfs(vfs);
                free(blocks);
                return NO_ERROR_CODE;
            } else {
                seek_cur(vfs, MAX_ITEM_NAME_LENGTH);	/* Skip filename */
            }
        }
    }

    /* No free space left, we need to assign new data cluster */
    free_block = find_free_data_blocks(vfs, 1);
    if (!free_block) {
        return ERROR_CODE;
    }


    dir_node = &((*vfs)->inodes[dir->current->inode]);

    if (dir_node->direct1 == ID_ITEM_FREE) dir_node->direct1 = free_block[0];
    else if (dir_node->direct2 == ID_ITEM_FREE) dir_node->direct2 = free_block[0];
    else if (dir_node->direct3 == ID_ITEM_FREE) dir_node->direct3 = free_block[0];
    else if (dir_node->direct4 == ID_ITEM_FREE) dir_node->direct4 = free_block[0];
    else if (dir_node->direct5 == ID_ITEM_FREE) dir_node->direct5 = free_block[0];
    else {
        free(free_block);
        free_block = find_free_data_blocks(vfs, 2);	/* Use indirect reference (need 2 free blocks - one to store addresses in indirect reference and one for the dirs) */
        if (free_block == NULL) return ERROR_CODE;

        if (dir_node->indirect1 == ID_ITEM_FREE) {
            dir_node->indirect1 = free_block[1];
        }
        else if (dir_node->indirect2 == ID_ITEM_FREE) {
            dir_node->indirect2 = free_block[1];
        }

        seek_data_cluster(vfs, free_block[1]);
        vfs_write_int32(vfs, &(free_block[0]));
    }

    seek_data_cluster(vfs, free_block[0]);
    vfs_write_int32(vfs, &(item->inode));
    write_vfs(vfs, item->item_name, sizeof(item->item_name), 1);

    flush_vfs(vfs);
    update_bitmap_in_file(vfs, dir->current, 1, NULL, 0);
    write_inode_to_vfs(vfs, dir->current->inode);
    free(free_block);
    free(blocks);
    return NO_ERROR_CODE;
}

int remove_directory_from_file(VFS** vfs, directory *dir, dir_item *item) {
    int block_number, j, block_count, item_count, rest, found = 0;
    int32_t *blocks, number, count, zero = 0;
    int empty[4];
    int max_items_in_block = 64;
    int32_t nodeid;

    memset(empty, 0, sizeof(empty));

    /* Get data blocks */
    blocks = get_data_blocks(vfs, dir->current->inode, &block_count, &rest);

    for (block_number = 0; block_number < block_count; block_number++) {
        seek_data_cluster(vfs, blocks[block_number]);

        item_count = 0;	// Counter of items in this data block
        for (j = 0; j < max_items_in_block; j++) {
            vfs_read_int32(vfs, &nodeid);
            if (nodeid > 0) {
                item_count++;
            }

            if (!found) {
                if (nodeid == (item->inode)) {
                    seek_cur(vfs, -4);
                    write_vfs(vfs, &empty, sizeof(empty), 1);
                    flush_vfs(vfs);
                    found = 1;

                    if (item_count > 1) break;
                }
            }
        }

        if (found) {	/* Verify if data block is free - If yes remove reference to it */
            if (item_count == 1) {
                inode *node = &((*vfs)->inodes[item->inode]);

                if (node->direct1 != block_number) {	/* Don't remove first direct */
                    if (node->direct2 == block_number) {
                        node->direct2 = ID_ITEM_FREE;
                    }
                    else if (node->direct3 == block_number) {
                        node->direct3 = ID_ITEM_FREE;
                    }
                    else if (node->direct4 == block_number) {
                        node->direct4 = ID_ITEM_FREE;
                    }
                    else if (node->direct5 == block_number) {
                        node->direct5 = ID_ITEM_FREE;
                    }
                    else {
                        for (int i = 0; i < 2; i++) {
                            if (i == 0)	// Go through indirect1
                                seek_data_cluster(vfs, node->indirect1);
                            else 		// Go through indirect2
                                seek_data_cluster(vfs, node->indirect2);

                            count = 0;
                            found = 0;
                            for (j = 0; j < INT32_COUNT_IN_BLOCK; j++) {
                                vfs_read_int32(vfs, &number);
                                if (number > 0)
                                    count++;
                                if (!found) {
                                    if (number == block_number) {
                                        found = 1;
                                        seek_cur(vfs, NEGATIVE_SIZE_OF_INT32);
                                        vfs_write_int32(vfs, &zero);
                                        flush_vfs(vfs);
                                        if (count > 1)
                                            break;
                                    }
                                }
                            }

                            if (found) {
                                /* Remove indirect references if they are empty */
                                if (count == 1) {
                                    if (i == 0) {
                                        node->indirect1 = ID_ITEM_FREE;
                                    }
                                    else {
                                        node->indirect2 = ID_ITEM_FREE;
                                    }
                                }
                                break;
                            }
                        }
                    }

                    update_bitmap_in_file(vfs, item, 0, NULL, 0);
                    write_inode_to_vfs(vfs, item->inode);
                }
            }

            free(blocks);
            return NO_ERROR_CODE;
        }
    }

    return ERROR_CODE;
}

/*
 * Updates the data bitmap both in memory and on disk for all blocks
 * associated with a particular directory item.
 * Supports direct and indirect blocks. Writes updated bitmap entries
 * to the correct bitmap offsets inside the VFS file.
 */
void update_bitmap_in_file(VFS** vfs, dir_item *item, int8_t value, int32_t *data_blocks, int b_count) {
    int32_t *blocks;
    int block_count;

    // If block list not provided, resolve data blocks from inode
    if (!data_blocks) {
        blocks = get_data_blocks(vfs, item->inode, &block_count, NULL);
    } else {
        blocks = data_blocks;
        block_count = b_count;
    }

    // Update direct data blocks
    for (int i = 0; i < block_count; i++) {
        (*vfs)->data_bitmap[blocks[i]] = value;
        seek_set(vfs, (*vfs)->superblock->bitmap_start_address + blocks[i]);
        vfs_write_int8(vfs, &value);
    }

    // Update indirect block #1
    if ((*vfs)->inodes[item->inode].indirect1 != ID_ITEM_FREE) {
        int32_t blk = (*vfs)->inodes[item->inode].indirect1;
        (*vfs)->data_bitmap[blk] = value;
        seek_set(vfs, (*vfs)->superblock->bitmap_start_address + blk);
        vfs_write_int8(vfs, &value);
    }

    // Update indirect block #2
    if ((*vfs)->inodes[item->inode].indirect2 != ID_ITEM_FREE) {
        int32_t blk = (*vfs)->inodes[item->inode].indirect2;
        (*vfs)->data_bitmap[blk] = value;
        seek_set(vfs, (*vfs)->superblock->bitmap_start_address + blk);
        vfs_write_int8(vfs, &value);
    }

    flush_vfs(vfs);
}


/*
 * Initializes new i-node. Returns index of the last block.
 */
int initialize_inode(VFS** vfs, int32_t inode_id, int32_t size, int block_count, int32_t *blocks) {
    int tmp_block_count;
    int last_data_block;

    inode *node = &((*vfs)->inodes[inode_id]);

    int block_count_with_indirect = get_block_count_with_indirect(block_count);

    node->nodeid = inode_id;
    node->isDirectory = 0;
    node->references = 1;
    node->file_size = size;
    node->direct1 = blocks[0]; /* First block is always used */

    last_data_block = 0;
    if (block_count > 1) {
        node->direct2 = blocks[1];
        last_data_block = 1;
    }

    if (block_count > 2) {
        node->direct3 = blocks[2];
        last_data_block = 2;
    }

    if (block_count > 3) {
        node->direct4 = blocks[3];
        last_data_block = 3;
    }

    if (block_count > 4) {
        node->direct5 = blocks[4];
        last_data_block = 4; /* This is the last data block. If we have partial data, we store them always in last direct block */
    }

    if (block_count > 5) {
        node->indirect1 = blocks[block_count_with_indirect - 1]; /* Write address of first indirect block to indirect1 */
    }

    if (block_count > (CLUSTER_SIZE / sizeof(int32_t) + 5)) {
        node->indirect2 = blocks[block_count_with_indirect - 2]; /* Use second to last block to write indirect 2*/
        seek_data_cluster(vfs, node->indirect1);
        write_vfs(vfs, &blocks[5], sizeof(int32_t), CLUSTER_SIZE / 4);

        tmp_block_count = block_count - (CLUSTER_SIZE / sizeof(int32_t) + 5);
        seek_data_cluster(vfs, node->indirect2);
        write_vfs(vfs, &blocks[(CLUSTER_SIZE / sizeof(int32_t) + 5)], sizeof(int32_t), tmp_block_count);
    }
    else  {
        tmp_block_count = block_count - 5;
        seek_data_cluster(vfs, node->indirect1);
        write_vfs(vfs,&blocks[5], sizeof(int32_t), tmp_block_count);
    }

    return last_data_block;
}

int32_t *alloc_free_clusters(VFS **vfs, int count) {
    if (vfs == NULL || *vfs == NULL || count <= 0) {
        return NULL;
    }

    int32_t *clusters = calloc(count, sizeof(int32_t));
    if (!clusters) {
        fprintf(stderr, "%s", MEMORY_ERROR_MSG);
        return NULL;
    }

    int found = 0;
    for (int i = 1; i < (*vfs)->superblock->data_cluster_count; i++) {
        if ((*vfs)->data_bitmap[i] == 0) {
            clusters[found++] = i;
            if (found == count) {
                return clusters;
            }
        }
    }

    free(clusters);
    return NULL;
}
