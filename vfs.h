//
// Created by Denis on 03.11.2025.
//

#ifndef FS_ON_INODE_VFS_H
#define FS_ON_INODE_VFS_H

#include "structures.h"
#include "constants.h"

void initialize_vfs(VFS **vfs, char *vfs_name);
bool load_vfs(VFS **vfs);
void needs_format(VFS **vfs);


size_t write_vfs(VFS **vfs, const void * ptr, size_t size, size_t count);
void write_inode_to_vfs(VFS **vfs, int id);
size_t vfs_write_int32(VFS **vfs, const void *ptr);
size_t vfs_write_int8(VFS **vfs, const void *ptr);
size_t vfs_read(VFS **vfs, void *ptr, size_t size, size_t count);
size_t vfs_read_int8(VFS **vfs, void *ptr);
size_t vfs_read_int32(VFS **vfs, void *ptr);
void vfs_read_sb(VFS **vfs);
void vfs_read_inodes(VFS **vfs, int index);
void vfs_load_directories(VFS **vfs, directory *dir);
int32_t *get_data_blocks(VFS** vfs, int32_t nodeid, int *block_count, int *rest);
int seek_data_cluster(VFS **vfs, int block_number);
int seek_set(VFS **vfs, long int offset);
int seek_cur(VFS **vfs, long int offset);
void load_directory_from_vfs(VFS** vfs, directory *dir, int id);
void rewind_vfs(VFS **vfs);
void flush_vfs(VFS **vfs);
int vfs_seek_from_start(VFS **vfs, long offset);
void vfs_init_inodes(VFS **vfs);
void vfs_init_root_directory(VFS **vfs);
bool vfs_init_memory_structures(VFS **vfs, int32_t vfs_size);
void vfs_write_superblock_to_file(VFS **vfs);
void vfs_write_bitmaps_to_file(VFS **vfs);
void vfs_write_inodes_to_file(VFS **vfs);


#endif //FS_ON_INODE_VFS_H