//
// Created by Denis on 03.11.2025.
//

#ifndef FS_ON_INODE_VFS_H
#define FS_ON_INODE_VFS_H

#include "structures.h"
#include "constants.h"

/*
 * Initializes the VFS structure.
 * If the VFS file already exists, it is opened and loaded into memory.
 * Otherwise, the user is prompted to format a new virtual filesystem.
 */
void initialize_vfs(VFS **vfs, char *vfs_name);

/*
 * Loads an existing VFS from disk into memory.
 * Reads the superblock, bitmap, inodes, and reconstructs
 * the directory hierarchy starting from the root directory.
 */
bool load_vfs(VFS **vfs);

/*
 * Handles the case when the VFS file does not exist.
 * Asks the user whether to format a new filesystem and
 * performs formatting if confirmed.
 */
void needs_format(VFS **vfs);

/*
 * Writes raw data to the VFS file.
 */
size_t write_vfs(VFS **vfs, const void * ptr, size_t size, size_t count);

/*
 * Writes a single inode to its correct position on disk.
 * Converts the in-memory inode struct into serialized fields.
 */
void write_inode_to_vfs(VFS **vfs, int id);
size_t vfs_write_int32(VFS **vfs, const void *ptr);
size_t vfs_write_int8(VFS **vfs, const void *ptr);

/*
 * Read raw data from VFS file
 */
size_t vfs_read(VFS **vfs, void *ptr, size_t size, size_t count);

/*
 * Read int8_t (1 byte)
 */
size_t vfs_read_int8(VFS **vfs, void *ptr);

/*
 * Read int32_t (4 bytes)
 */
size_t vfs_read_int32(VFS **vfs, void *ptr);

/*
 * Reads the superblock from the VFS file.
 * The superblock contains all essential metadata
 * describing the layout of the filesystem.
 */
bool vfs_read_sb(VFS **vfs);

/*
 * Reads a single inode from disk into memory.
 * The inode is loaded based on its index in the inode table.
 */
void vfs_read_inodes(VFS **vfs, int index);

/*
 * Loads the directory tree from the VFS recursively,
 * starting from the root directory.
 */
bool vfs_load_directories(VFS **vfs, directory *dir);

/*
 * Collects all data block indices belonging to a file:
 *  - First copies all direct pointers (up to 5)
 *  - Then loads all references from the first indirect block
 *  - Finally resolves two-level indirect addressing (indirect2)
 *
 * Returns a dynamically allocated array of block indices and stores
 * the number of blocks in `block_count`.
 *
 * Caller is responsible for freeing the returned array.
 */
int32_t *get_data_blocks(VFS** vfs, int32_t nodeid, int *block_count, int *rest);

/*
 * Moves the file cursor to the beginning of a data cluster.
 */
int seek_data_cluster(VFS **vfs, int block_number);


int seek_set(VFS **vfs, long int offset);
int seek_cur(VFS **vfs, long int offset);

/*
 * Reads directory entries stored in data clusters and
 * reconstructs the directory structure in memory.
 * Subdirectories are loaded recursively.
 */
bool load_directory_from_vfs(VFS** vfs, directory *dir, int id);
void rewind_vfs(VFS **vfs);

/*
 * Flushes buffered VFS data to disk.
 */
void flush_vfs(VFS **vfs);
int vfs_seek_from_start(VFS **vfs, long offset);

/*
 * Resets the entire inode table.
 * Marks each inode as unused and clears all block references.
 */
void vfs_init_inodes(VFS **vfs);

/*
 * Creates and initializes the root directory (inode 0).
 * Links directory structures in memory, marks cluster 0 as used,
 * and sets up the root inode to be a directory located in cluster 0.
 */
bool vfs_init_root_directory(VFS **vfs);

/*
 * Allocates and initializes core memory structures of the filesystem.
 * Builds the superblock, bitmap array, inode table, directory table,
 * and initializes all inodes including the root directory.
 */
bool vfs_init_memory_structures(VFS **vfs, int32_t vfs_size);

/*
 * Writes superblock metadata to the beginning of the VFS file.
 * Saves all structural fields in a fixed layout.
 */
void vfs_write_superblock_to_file(VFS **vfs);

/*
 * Stores the full data bitmap into the VFS file.
 * Bitmap is written starting from bitmap_start_address.
 */
void vfs_write_bitmaps_to_file(VFS **vfs);

/*
 * Writes the entire inode table to the VFS file.
 * Exports every inode sequentially using write_inode_to_vfs().
 */
void vfs_write_inodes_to_file(VFS **vfs);

/*
 * Searches the inode table for the first free inode.
 * Returns its index or ID_ITEM_FREE if none are available.
 */
int32_t vfs_find_free_inode(VFS **vfs);

/*
 * Writes directory structure changes to the VFS file.
 * If create == true, inserts a new directory entry.
 * Otherwise removes an existing entry.
 * Delegates the actual work to helper functions responsible for file layout.
 */
int update_directory_in_file(VFS** vfs, directory *dir, dir_item *item, bool create);

/*
 * Writes a new directory entry into the parent directory on disk.
 * If no free space is available, a new data cluster is allocated.
 */
int create_directory_in_file(VFS** vfs, directory *dir, dir_item *item);

/*
 * Removes a directory entry from the parent directory on disk.
 * Frees data clusters and indirect references if they become unused.
 */
int remove_directory_from_file(VFS** vfs, directory *dir, dir_item *item);

/*
 * Updates the data bitmap both in memory and on disk for all blocks
 * associated with a particular directory item.
 * Supports direct and indirect blocks. Writes updated bitmap entries
 * to the correct bitmap offsets inside the VFS file.
 */
void update_bitmap_in_file(VFS** vfs, dir_item *item, int8_t value, int32_t *data_blocks, int b_count);

/*
 * Initializes new i-node. Returns index of the last block.
 */
int initialize_inode(VFS** vfs, int32_t inode_id, int32_t size, int block_count, int32_t *blocks);

/*
 * Allocates a specified number of free data clusters
 * by scanning the data bitmap.
 *
 * Returns an array of cluster indices or NULL if allocation fails.
 */
int32_t *alloc_free_clusters(VFS **vfs, int count);

/*
* Overwrites all data clusters of a file with zero bytes.
*/
int zero_data_blocks(VFS **vfs, int32_t *blocks, int block_count);

/*
 * Clears content of indirect block tables (indirect1 and indirect2),
 * effectively removing references to secondary and tertiary data blocks.
 *
 * Does NOT clear the referenced blocks themselves — only the pointer tables.
 * The caller is responsible for clearing actual file data blocks.
 */
int zero_indirect_blocks(VFS **vfs, int32_t indirect1, int32_t indirect2);

/*
 * Copies one full data cluster from the VFS into a real filesystem file.
 */
int copy_full_block(VFS **vfs, FILE *dest, int32_t block_num);

/*
 * Copies only the valid portion of the last data block.
 * This is used when the file size does not fully occupy
 * the final cluster.
 */
int copy_partial_block(VFS **vfs, FILE *dest, int32_t block_num, size_t size);

/*
 * Copies a single data block from a real file into a specific
 * data cluster in the virtual filesystem.
 */
int copy_block_to_vfs(FILE *src, VFS **vfs, int32_t block_num, size_t size);

/*
 * Propagates a file size change upward through the directory tree.
 * Every directory maintains the total size of its contents via file_size.
 *
 * Walks from the given directory up to the root and adjusts file_size
 * for each ancestor, writing updated inode information to disk.
 */
void update_sizes_in_file(VFS** vfs, directory *dir, int32_t size);

/*
 * Calculates how many clusters are required to store a file,
 * including indirect and double-indirect block references.
 */
int calculate_required_clusters(int data_blocks);

/*
 * Calculates the number of valid bytes stored in the last data block
 * of a file based on its total size.
 */
size_t calculate_last_block_size(int32_t file_size);

/*
 * Allocates a free inode and a single free data block.
 * Used mainly for directory creation.
 */
bool allocate_new_inode_and_block(VFS **vfs, int *inode_id, int32_t *block);

/*
 * Initializes an inode structure to represent a directory.
 * Sets default values and assigns the first data block.
 */
void init_directory_inode(inode *node, int id, int32_t block);

/*
 * Resets inode fields to the "free" state.
 */
void reset_inode(inode *nd);
#endif //FS_ON_INODE_VFS_H