//
// Created by Denis on 06.11.2025.
//

#ifndef FS_ON_INODE_HELPERS_H
#define FS_ON_INODE_HELPERS_H

#include <stdint.h>
#include "structures.h"


bool streq(const char *str1, const char *str2);
bool str_empty(char *str);
char * get_line();
void remove_nl_inplace(char *message);
superblock *superblock_init(int32_t vfs_size);
dir_item *create_directory_item(int32_t inode_id, const char *name);
void check_sb_info(VFS **vfs);
int parse_path(VFS **vfs, char *path, char **name, directory **dir);
directory *find_directory(VFS **vfs, char *path);
dir_item *find_item_by_name(dir_item *first, const char *name);
bool check_if_exists(directory *dir, char *name);
int32_t *find_free_data_blocks(VFS** vfs, int count);
void print_directory_content(directory *dir);
dir_item *find_diritem(dir_item *item,char *name);
dir_item *remove_diritem(dir_item **head, const char *name);
void print_dir_item_info(VFS **vfs, dir_item *item);
void update_sizes_in_file(VFS** vfs, directory *dir, int32_t size);
int get_last_block_size(int rest);
bool file_exists(char *filename);
void add_item_to_list(dir_item **list_head, dir_item *new_item);
bool validate_new_item_name(char *name);
bool allocate_new_inode_and_block(VFS **vfs, int *inode_id, int32_t *block);
void init_directory_inode(inode *node, int id, int32_t block);
bool create_directory_structure(VFS **vfs, directory *parent, char *name,int inode_id, directory **out_dir,dir_item **out_item);
bool sync_to_disk(VFS **vfs, directory *parent,dir_item *item, int inode_id,int32_t *block, bool create, int8_t value);
dir_item *find_directory_by_name(dir_item *dir, char *name);
void print_indirect_block(VFS **vfs, int32_t block);
int count_used_blocks(VFS *v);
int count_used_inodes(VFS *v);
int count_directories(VFS *v);
void reset_inode(inode *nd);
bool stream_file_content(VFS **vfs, dir_item *file_item);
dir_item *find_item_in_directory(directory *parent, char *name);
int calculate_required_clusters(int data_blocks);
bool is_circular_move(VFS **vfs, int32_t src_inode, directory *dest_dir);
bool remove_item_from_list(dir_item **head, const char *name, dir_item **removed_item);
void safe_copy_name(char *dest, const char *src, size_t max_len);
int handle_full_file_removal(VFS **vfs, directory *parent,dir_item *item, inode *nd);
int handle_hardlink_removal(VFS **vfs, directory *parent, dir_item *item, inode *nd);
void remove_and_free_dir_entry(directory *parent, const char *name);
int zero_indirect_blocks(VFS **vfs, int32_t indirect1, int32_t indirect2);
size_t calculate_last_block_size(int32_t file_size);
int copy_full_block(VFS **vfs, FILE *dest, int32_t block_num);
int copy_partial_block(VFS **vfs, FILE *dest, int32_t block_num, size_t size);
char* extract_filename_from_path(const char *path);
int get_file_size(FILE *file, int32_t *size_out);
void rollback_import(VFS **vfs, directory *dir, dir_item *item,int32_t *blocks, int block_count, int inode_id);
int copy_block_to_vfs(FILE *src, VFS **vfs, int32_t block_num, size_t size);

#endif //FS_ON_INODE_HELPERS_H


