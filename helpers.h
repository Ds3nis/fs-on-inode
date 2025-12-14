//
// Created by Denis on 06.11.2025.
//

#ifndef FS_ON_INODE_HELPERS_H
#define FS_ON_INODE_HELPERS_H

#include <stdint.h>
#include "structures.h"


/*
 * Returns true if string str1 equals str2
 */
bool streq(const char *str1, const char *str2);

/*
 * Returns true if string is empty
 */
bool str_empty(char *str);

/*
 * Reads a single line from standard input.
 * The buffer is dynamically resized as needed and always
 * null-terminated. The caller is responsible for freeing
 * the returned memory.
 *
 * Returns NULL on EOF or allocation failure.
 */
char * get_line();

/*
 * Removes new line characters from string
 */
void remove_nl_inplace(char *message);

/*
 * Initializes the superblock based on total filesystem size.
 * Computes number of clusters for bitmap, inodes and data,
 * determines start offsets of each section and prepares metadata.
 */
superblock *superblock_init(int32_t vfs_size);

/*
 * Creates and initializes a directory item structure.
 * The item stores the inode reference and its name.
 */
dir_item *create_directory_item(int32_t inode_id, const char *name);

/*
 * Shows debug information
 */
void check_sb_info(VFS **vfs);

/*
 * Parses the provided path and splits it into: parent directory (dir) and final name.
 * Handles relative paths, absolute paths, and parent navigation (“..”).
 * If path contains '/', extracts parent directory component and resolves it in the VFS.
 * Returns NO_ERROR_CODE on success or ERROR_CODE if the path cannot be resolved.
 */
int parse_path(VFS **vfs, char *path, char **name, directory **dir);

/*
* Resolves a directory path into a directory* structure.
*/
directory *find_directory(VFS **vfs, char *path);

/*
 * Checks whether an item with the given name already exists in the directory.
 * Iterates through file items first, then subdirectory items.
 * Returns true if name matches any entry, false otherwise.
 */
bool check_if_exists(directory *dir, char *name);

/*
 * Allocates a specified number of free data blocks.
 * Scans the data bitmap starting from block 1 (block 0 reserved for root),
 * and collects the required number of unused blocks.
 * Returns an array of block indices or NULL if not enough blocks exist.
 */
int32_t *find_free_data_blocks(VFS** vfs, int count);

/*
 * Prints the contents of a directory.
 * Lists subdirectories first, followed by files.
 * If no items exist in a category, "<none>" is shown.
 */
void print_directory_content(directory *dir);

/*
 * Removes a directory item with the given name from a linked list.
 * The removed item is detached and returned to the caller,
 * who becomes responsible for freeing it.
 *
 * Returns NULL if the item was not found.
 */
dir_item *remove_diritem(dir_item **head, const char *name);

/*
 * Prints full metadata of an item based on its directory entry.
 */
void print_dir_item_info(VFS **vfs, dir_item *item);


void print_indirect_block(VFS **vfs, int32_t block);

/*
 * Checks whether a file exists in the real filesystem.
 *
 * Returns true if the file is found, false otherwise.
 */
bool file_exists(char *filename);

/*
 * Appends a directory item to the end of a linked list.
 */
void add_item_to_list(dir_item **list_head, dir_item *new_item);

/*
 * Validates that a new file name fits into directory entry limits.
 */
bool validate_new_item_name(char *name);

/*
 * Creates a new directory structure in memory and links it
 * with its parent directory and inode.
 * The created directory and its dir_item are returned via output parameters.
 */
bool create_directory_structure(VFS **vfs, directory *parent, char *name,int inode_id, directory **out_dir,dir_item **out_item);

/*
 * Synchronizes directory changes, inode state, and bitmap updates
 * to the virtual filesystem file.
 */
bool sync_to_disk(VFS **vfs, directory *parent,dir_item *item, int inode_id,int32_t *block, bool create, int8_t value);

/*
 * Searches for a subdirectory with the given name inside 'dir'.
 * Returns dir_item* if found, NULL if not found.
 */
dir_item *find_directory_by_name(dir_item *dir, char *name);

/*
 * Counts how many data blocks are currently marked as used in the bitmap.
 * A block is considered used if its bitmap entry is non-zero.
 */
int count_used_blocks(VFS *v);

/*
 * Counts how many inodes are currently allocated.
 * An inode is treated as used if its nodeid is not equal to ID_ITEM_FREE.
 */
int count_used_inodes(VFS *v);

/*
 * Counts how many directory structures are present in memory.
 * A directory exists if the corresponding pointer in all_dirs[] is non-NULL.
 */
int count_directories(VFS *v);

/*
 * Streams the contents of a file to stdout.
 *  - Retrieves all allocated data blocks for the file
 *  - Iterates over each block and reads only the required number of bytes
 *  - Handles files of any size, including those spanning multiple clusters
 *  - Supports direct, indirect1 and indirect2 block lists
 *
 * Returns true on full success or false if any cluster cannot be read.
 */
bool stream_file_content(VFS **vfs, dir_item *file_item);

/*
 * Searches for a file or subdirectory with the given name
 * inside a directory.
 *
 * Returns the corresponding dir_item or NULL if not found.
 */
dir_item *find_item_in_directory(directory *parent, char *name);

/*
 * Checks whether moving a directory into dest_dir would cause
 * a cyclic structure (i.e., moving folder A into its own subfolder).
 *
 * Only applies to directories; moving files always returns false.
 *
 * Walks upward from dest_dir through parent pointers
 * and detects if any directory in the chain matches src_inode.
 */
bool is_circular_move(VFS **vfs, int32_t src_inode, directory *dest_dir);

/*
* Removes a dir_item from a linked list of directory entries.
*/
bool remove_item_from_list(dir_item **head, const char *name, dir_item **removed_item);

void safe_copy_name(char *dest, const char *src, size_t max_len);

/*
 * Removes a dir_item from a directory list in memory and frees the allocated memory.
 * This handles only the in-memory structure, not the VFS file metadata.
 *
 * A tiny but crucial helper for rm, mv, and rmdir.
 */
void remove_and_free_dir_entry(directory *parent, const char *name);

/*
 * Removes a file completely (last reference):
 * Performs cleanup and error reporting if any step fails.
 */
int handle_full_file_removal(VFS **vfs, directory *parent,dir_item *item, inode *nd);

/*
 * Handles the case when a file has more than one reference (hardlinks).
 * Ensures rollback if any disk operation fails.
 */
int handle_hardlink_removal(VFS **vfs, directory *parent, dir_item *item, inode *nd);

/*
 * Extracts the filename from a filesystem path.
 * If no path separator is found, the whole string is returned.
 */
char* extract_filename_from_path(const char *path);

/*
 * Retrieves the size of a real filesystem file without changing
 * the current file position.
 */
int get_file_size(FILE *file, int32_t *size_out);

/*
 * Reverts all filesystem changes made during a failed import operation.
 * This includes directory entries, bitmap updates, and inode allocation.
 */
void rollback_import(VFS **vfs, directory *dir, dir_item *item,int32_t *blocks, int block_count, int inode_id);

/*
 * Determines the final destination directory and file name.
 * If dest_path points to a directory, the source name is reused.
 * Otherwise, dest_path is treated as a new file name.
 */
bool smart_parse_destination(VFS **vfs, const char *src_name,const char *dest_path,char **final_name, directory **final_dir);

/*
 * Resolves a directory name inside a parent directory.
 * Returns the directory if the name exists and refers to a directory inode.
 */
directory *resolve_destination_directory(VFS **vfs, directory *parent_dir, char *name);
#endif //FS_ON_INODE_HELPERS_H


