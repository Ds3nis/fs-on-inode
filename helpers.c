#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "helpers.h"
#include <stdio.h>
#include <sys/stat.h>


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

/*
 * Initializes the superblock based on total filesystem size.
 * Computes number of clusters for bitmap, inodes and data,
 * determines start offsets of each section and prepares metadata.
 */
superblock *superblock_init(int32_t vfs_size) {
    superblock *sb = calloc(1, sizeof(superblock));
    if (!sb) return NULL;

    strncpy(sb->signature, SUPERBLOCK_SIGNATURE, SIGNATURE_LENGTH - 1);

    sb->disk_size = vfs_size;
    sb->cluster_size = CLUSTER_SIZE;
    sb->cluster_count = vfs_size / CLUSTER_SIZE;

    // Calculate bitmap size in clusters
    int32_t bitmap_bytes = sb->cluster_count * sizeof(int8_t);
    int32_t bitmap_cluster_count = (bitmap_bytes + CLUSTER_SIZE - 1) / CLUSTER_SIZE;
    if (bitmap_cluster_count < 1) bitmap_cluster_count = 1;

    // Reserve ~10% for inode clusters
    int32_t inode_cluster_count = (int32_t)(sb->cluster_count * 0.10);
    if (inode_cluster_count < 1) inode_cluster_count = 1;

    // Remaining area is data clusters
    int32_t data_cluster_count = sb->cluster_count - bitmap_cluster_count - inode_cluster_count - 1;
    if (data_cluster_count < 1) {
        printf("Not enough space for data clusters (choose larger size).\n");
        exit(1);
    }

    // Compute number of inodes based on inode storage size
    int32_t inodes_per_cluster = CLUSTER_SIZE / sizeof(inode);
    int32_t inode_count = inode_cluster_count * inodes_per_cluster;

    // Compute start addresses of filesystem regions
    int32_t bitmap_start_address = CLUSTER_SIZE;
    int32_t inode_start_address = bitmap_start_address + bitmap_cluster_count * CLUSTER_SIZE;
    int32_t data_start_address  = inode_start_address + inode_cluster_count * CLUSTER_SIZE;

    sb->inode_count          = inode_count;
    sb->bitmap_cluster_count = bitmap_cluster_count;
    sb->inode_cluster_count  = inode_cluster_count;
    sb->data_cluster_count   = data_cluster_count;
    sb->bitmap_start_address = bitmap_start_address;
    sb->inode_start_address  = inode_start_address;
    sb->data_start_address   = data_start_address;

    return sb;
}


dir_item *create_directory_item(int32_t inode_id, const char *name) {
    // create dir_item for root (inode 0, name "/")
    dir_item *dir_item = calloc(1, sizeof(dir_item));
    if (!dir_item) {return NULL;}


    dir_item->inode = inode_id;
    memset(dir_item->item_name, 0, MAX_ITEM_NAME_LENGTH);
    strncpy(dir_item->item_name, name, MAX_ITEM_NAME_LENGTH - 1);
    dir_item->item_name[MAX_ITEM_NAME_LENGTH - 1] = '\0';
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


/*
 * Parses the provided path and splits it into: parent directory (dir) and final name.
 * Handles relative paths, absolute paths, and parent navigation (“..”).
 * If path contains '/', extracts parent directory component and resolves it in the VFS.
 * Returns NO_ERROR_CODE on success or ERROR_CODE if the path cannot be resolved.
 */
int parse_path(VFS **vfs, char *path, char **name, directory **dir) {
    int length;
    char buff[256];

    if (str_empty(path)) {
        return ERROR_CODE;
    }

    if (streq(path, "..")) {
        *name = "";
        *dir = (*vfs)->current_dir->parent;
    } else if ((*name = strrchr(path, '/')) == NULL) {	/* If there is no slash - Only filename/directory */
        *name = path;
        *dir = (*vfs)->current_dir;
    }
    else  {
        length = strlen(path) - strlen(*name);
        if (path[0] == '/') {
            if (!strstr(path + 1, "/")) { /* If there is only root */
                length = 1;
            }

        }

        *name = *name + 1;
        memset(buff, '\0', 256);
        strncpy(buff, path, length);

        // Find the directory
        *dir = find_directory(vfs, buff);
        if (*dir == NULL) {
            return ERROR_CODE;
        }
    }

    return NO_ERROR_CODE;
}


/*
 * Resolves a directory path into a directory* structure.
 *
 * Supports:
 *   - Absolute paths ("/a/b/c")
 *   - Relative paths ("a/b", "../x", "./y")
 *   - Current directory "."
 *   - Parent directory ".."
 *
 * The function walks through the path tokens and navigates through
 * the directory tree. If at any point a directory does not exist,
 * NULL is returned to indicate an invalid path.
 */
directory *find_directory(VFS **vfs, char *path) {
    if (str_empty(path)) {
        return NULL;
    }

    directory *current;

    // Start from root for absolute path
    if (path[0] == '/') {
        current = (*vfs)->all_dirs[0];
        path++;  // skip leading '/'
        if (*path == '\0') {
            return current; // path = "/"
        }
    } else {
        // Otherwise start from current directory
        current = (*vfs)->current_dir;
    }

    // Copy path into a temporary buffer for tokenization
    char buff[256];
    strncpy(buff, path, sizeof(buff) - 1);
    buff[sizeof(buff) - 1] = '\0';

    // Process each folder name
    char *token = strtok(buff, "/");
    while (token != NULL) {

        if (streq(token, ".")) {
            // Stay in the same directory
        }
        else if (streq(token, "..")) {
            // Move to parent directory
            current = current->parent;
        }
        else {
            // Look for the subdirectory by name
            dir_item *found = find_directory_by_name(current->subdir, token);
            if (found == NULL) {
                return NULL; // Name not found
            }

            current = (*vfs)->all_dirs[found->inode];
            if (current == NULL) {
                return NULL; // Corrupted or missing directory structure
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

/*
 * Checks whether an item with the given name already exists in the directory.
 * Iterates through file items first, then subdirectory items.
 * Returns true if name matches any entry, false otherwise.
 */
bool check_if_exists(directory *dir, char *name) {
    dir_item *item;

    // Check files
    item = dir->file;
    while (item != NULL) {
        if (streq(name, item->item_name)) {
            return true;
        }
        item = item->next;
    }

    // Check subdirectories
    item = dir->subdir;
    while (item != NULL) {
        if (streq(name, item->item_name)) {
            return true;
        }
        item = item->next;
    }
    return false;
}


/*
 * Allocates a specified number of free data blocks.
 * Scans the data bitmap starting from block 1 (block 0 reserved for root),
 * and collects the required number of unused blocks.
 * Returns an array of block indices or NULL if not enough blocks exist.
 */
int32_t *find_free_data_blocks(VFS** vfs, int count) {
    int32_t *blocks = calloc(count, sizeof(int32_t));
    if (!blocks) {
        printf(MEMORY_ERROR_MSG);
        return NULL;
    }

    int found_blocks = 0;

    // Scan bitmap for free blocks
    for (int i = 1; i < (*vfs)->superblock->data_cluster_count; i++) {
        if ((*vfs)->data_bitmap[i] == 0) {
            blocks[found_blocks++] = i;

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

/*
 * Prints full metadata of an item based on its directory entry.
 * Displays:
 *   - name, size, inode number and reference count
 *   - whether the item is a file or directory
 *   - all direct block addresses (or NONE)
 *   - lists contents of indirect blocks (1st- and 2nd-level):
 *       * reads the cluster containing addresses
 *       * prints stored block numbers until EMPTY_ADDRESS
 *
 * Used for diagnostics and filesystem debugging.
 */
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
    print_indirect_block(vfs, node.indirect1);

    printf("Indirect 2: ");
    print_indirect_block(vfs, node.indirect2);


    printf("\n");
}

void print_indirect_block(VFS **vfs, int32_t block) {
    if (block == ID_ITEM_FREE) {
        printf("FREE\n");
        return;
    }

    printf("(%d): ", block);
    seek_data_cluster(vfs, block);

    int32_t val;
    int first = 1;
    for (int i = 0; i < INT32_COUNT_IN_BLOCK; i++) {
        vfs_read_int32(vfs, &val);
        if (val == EMPTY_ADDRESS) break;
        printf(first ? "%d" : ", %d", val);
        first = 0;
    }
    if (first) printf("EMPTY");
    printf("\n");
}


int calculate_required_clusters(int data_blocks) {
    if (data_blocks <= 5) {
        return data_blocks;  // Тільки direct
    }
    else if (data_blocks <= (CLUSTER_SIZE / 4) + 5) {
        return data_blocks + 1;  // + indirect1
    }
    else {
        return data_blocks + 2;  // + indirect1 + indirect2
    }
}


/*
 * Propagates a file size change upward through the directory tree.
 * Every directory maintains the total size of its contents via file_size.
 *
 * Walks from the given directory up to the root and adjusts file_size
 * for each ancestor, writing updated inode information to disk.
 */
void update_sizes_in_file(VFS** vfs, directory *dir, int32_t size) {
    directory *d = dir;
    while (d != (*vfs)->all_dirs[0]) {
        (*vfs)->inodes[d->current->inode].file_size += size;
        write_inode_to_vfs(vfs, d->current->inode);
        d = d->parent;
    }

    /* Update root */
    (*vfs)->inodes[d->current->inode].file_size += size;
    write_inode_to_vfs(vfs, d->current->inode);
}

int get_last_block_size(int rest) {
    if (rest != 0) {
        return rest;
    } else {
        return CLUSTER_SIZE;
    }
}

bool file_exists (char *filename) {
    struct stat   buffer;
    return (stat (filename, &buffer) == 0);
}


void add_item_to_list(dir_item **list_head, dir_item *new_item) {
    if (!list_head || !new_item) return;

    dir_item **current = list_head;
    while (*current) {
        current = &((*current)->next);
    }
    *current = new_item;
}

bool validate_new_item_name(char *name) {
    if (strlen(name) >= MAX_ITEM_NAME_LENGTH) {
        printf(NAME_TOO_LONG, MAX_ITEM_NAME_LENGTH - 1);
        return false;
    }
    return true;
}

bool allocate_new_inode_and_block(VFS **vfs, int *inode_id, int32_t *block) {
    *inode_id = vfs_find_free_inode(vfs);
    if (*inode_id == -1) {
        printf(NO_FREE_INODE);
        return false;
    }

    int32_t *blk = find_free_data_blocks(vfs, 1);
    if (!blk) {
        printf(NOT_ENOUGH_BLOCKS_MSG);
        return false;
    }

    *block = blk[0];
    free(blk);
    return true;
}

void init_directory_inode(inode *node, int id, int32_t block) {
    memset(node, 0, sizeof(inode));
    node->nodeid = id;
    node->isDirectory = true;
    node->file_size = 0;
    node->references = 1;
    node->direct1 = block;
    node->direct2 = node->direct3 = node->direct4 =
    node->direct5 = node->indirect1 = node->indirect2 = ID_ITEM_FREE;
}

bool create_directory_structure(VFS **vfs, directory *parent, char *name,
                                int inode_id, directory **out_dir,
                                dir_item **out_item)
{
    dir_item *item = create_directory_item(inode_id, name);
    if (!item) {
        printf(MEMORY_ERROR_MSG);
        return false;
    }

    directory *dir = calloc(1, sizeof(directory));
    if (!dir) {
        free(item);
        printf(MEMORY_ERROR_MSG);
        return false;
    }

    dir->current = item;
    dir->parent = parent;

    (*vfs)->all_dirs[inode_id] = dir;

    *out_dir = dir;
    *out_item = item;

    return true;
}

bool sync_to_disk(VFS **vfs, directory *parent,dir_item *item, int inode_id,int32_t *block, bool create)
{
    if (update_directory_in_file(vfs, parent, item, create) == ERROR_CODE) {
        return false;
    }
    update_bitmap_in_file(vfs, item, 1, block, 1);
    write_inode_to_vfs(vfs, inode_id);
    flush_vfs(vfs);
    return true;
}

/*
 * Searches for a subdirectory with the given name inside 'dir'.
 * Returns dir_item* if found, NULL if not found.
 */
dir_item *find_directory_by_name(dir_item *dir, char *name) {
    dir_item *sub = dir;

    while (sub) {
        if (streq(sub->item_name, name)) {
            return sub;
        }
        sub = sub->next;
    }
    return NULL;
}

/*
 * Resets inode fields to the "free" state.
 */
void reset_inode(inode *nd) {
    nd->nodeid      = ID_ITEM_FREE;
    nd->isDirectory = 0;
    nd->references  = 0;
    nd->file_size   = 0;

    nd->direct1 = nd->direct2 = nd->direct3 = nd->direct4 = nd->direct5 = ID_ITEM_FREE;
    nd->indirect1 = nd->indirect2 = ID_ITEM_FREE;
}

/*
 * Counts how many data blocks are currently marked as used in the bitmap.
 * A block is considered used if its bitmap entry is non-zero.
 */
int count_used_blocks(VFS *v) {
    int used = 0;
    for (int i = 0; i < v->superblock->data_cluster_count; i++)
        if (v->data_bitmap[i]) used++;
    return used;
}

/*
 * Counts how many inodes are currently allocated.
 * An inode is treated as used if its nodeid is not equal to ID_ITEM_FREE.
 */
int count_used_inodes(VFS *v) {
    int used = 0;
    for (int i = 0; i < v->superblock->inode_count; i++)
        if (v->inodes[i].nodeid != ID_ITEM_FREE) used++;
    return used;
}

/*
 * Counts how many directory structures are present in memory.
 * A directory exists if the corresponding pointer in all_dirs[] is non-NULL.
 */
int count_directories(VFS *v) {
    int count = 0;
    for (int i = 0; i < v->superblock->inode_count; i++)
        if (v->all_dirs[i] != NULL) count++;
    return count;
}

/*
 * Streams the contents of a file to stdout.
 *  - Retrieves all allocated data blocks for the file
 *  - Iterates over each block and reads only the required number of bytes
 *  - Handles files of any size, including those spanning multiple clusters
 *  - Supports direct, indirect1 and indirect2 block lists
 *
 * Returns true on full success or false if any cluster cannot be read.
 */
bool stream_file_content(VFS **vfs, dir_item *file_item) {
    if (!vfs || !*vfs || !file_item) {
        return false;
    }

    inode *node = &(*vfs)->inodes[file_item->inode];
    int32_t file_size = node->file_size;

    if (file_size == 0) {
        return true; // empty file
    }

    int block_count = 0;
    int32_t *blocks = get_data_blocks(vfs, file_item->inode, &block_count, NULL);
    if (!blocks || block_count == 0) {
        printf("Error: Failed to get data blocks for file\n");
        return false;
    }

    char buffer[CLUSTER_SIZE];
    int32_t bytes_remaining = file_size;
    bool success = true;

    for (int i = 0; i < block_count && bytes_remaining > 0; i++) {
        // Seek to correct cluster
        if (seek_data_cluster(vfs, blocks[i]) != 0) {
            printf("Error: Failed to seek to cluster %d\n", blocks[i]);
            success = false;
            break;
        }

        int32_t bytes_to_read =
            (bytes_remaining < CLUSTER_SIZE) ? bytes_remaining : CLUSTER_SIZE;

        size_t read = vfs_read(vfs, buffer, 1, bytes_to_read);
        if (read != bytes_to_read) {
            printf("Error: Failed to read from cluster %d\n", blocks[i]);
            success = false;
            break;
        }

        // Print to stdout
        if (fwrite(buffer, 1, bytes_to_read, stdout) != bytes_to_read) {
            printf("Error: Failed to write to stdout\n");
            success = false;
            break;
        }

        bytes_remaining -= bytes_to_read;
    }

    fflush(stdout);
    free(blocks);

    if (bytes_remaining > 0) {
        printf("\nWarning: Not all data was read (%d bytes remaining)\n", bytes_remaining);
        return false;
    }

    return success;
}

dir_item *find_item_in_directory(directory *parent, char *name) {
    dir_item *item;

    // Check files
    item = parent->file;
    while (item != NULL) {
        if (streq(name, item->item_name)) {
            return item;
        }
        item = item->next;
    }

    // Check subdirectories
    item = parent->subdir;
    while (item != NULL) {
        if (streq(name, item->item_name)) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}


/*
 * Checks whether moving a directory into dest_dir would cause
 * a cyclic structure (i.e., moving folder A into its own subfolder).
 *
 * Only applies to directories; moving files always returns false.
 *
 * Walks upward from dest_dir through parent pointers
 * and detects if any directory in the chain matches src_inode.
 */
bool is_circular_move(VFS **vfs, int32_t src_inode, directory *dest_dir) {
    if (!(*vfs)->inodes[src_inode].isDirectory) {
        return false;
    }

    directory *curr = dest_dir;
    while (curr != NULL) {
        if (curr->current && curr->current->inode == src_inode) {
            return true;
        }
        curr = curr->parent;
    }

    return false;
}

/*
 * Removes a dir_item from a linked list of directory entries.
 *  - Searches for an item with the given name
 *  - Detaches it from the list
 *  - Returns the removed node via 'removed_item'
 *
 * Does not free memory, so the caller can reinsert the item elsewhere
 * (as required for a move operation).
 */
bool remove_item_from_list(dir_item **head, const char *name, dir_item **removed_item) {
    dir_item **curr_ptr = head;

    while (*curr_ptr != NULL) {
        if (streq((*curr_ptr)->item_name, name)) {
            *removed_item = *curr_ptr;
            *curr_ptr = (*curr_ptr)->next;
            (*removed_item)->next = NULL;
            return true;
        }
        curr_ptr = &((*curr_ptr)->next);
    }

    return false;
}

void safe_copy_name(char *dest, const char *src, size_t max_len) {
    strncpy(dest, src, max_len - 1);
    dest[max_len - 1] = '\0';
}

static int zero_data_blocks(VFS **vfs, int32_t *blocks, int block_count) {
    static char zero[CLUSTER_SIZE] = {0};

    for (int i = 0; i < block_count; i++) {
        if (blocks[i] == ID_ITEM_FREE) continue;

        if (seek_data_cluster(vfs, blocks[i]) == ERROR_CODE) {
            return ERROR_CODE;
        }
        if (write_vfs(vfs, zero, sizeof(zero), 1) != 1) {
            return ERROR_CODE;
        }
    }

    return NO_ERROR_CODE;
}


int zero_indirect_blocks(VFS **vfs, int32_t indirect1, int32_t indirect2) {
    static char zero[CLUSTER_SIZE] = {0};

    if (indirect1 != ID_ITEM_FREE) {
        if (seek_data_cluster(vfs, indirect1) == ERROR_CODE ||
            write_vfs(vfs, zero, sizeof(zero), 1) != 1) {
            return ERROR_CODE;
        }
    }

    if (indirect2 != ID_ITEM_FREE) {
        if (seek_data_cluster(vfs, indirect2) == ERROR_CODE ||
            write_vfs(vfs, zero, sizeof(zero), 1) != 1) {
            return ERROR_CODE;
        }
    }

    return NO_ERROR_CODE;
}


void remove_and_free_dir_entry(directory *parent, const char *name) {
    dir_item *detached = remove_diritem(&parent->file, name);
    if (detached) {
        detached->next = NULL;
        free(detached);
    }
}


int handle_hardlink_removal(VFS **vfs, directory *parent, dir_item *item, inode *nd) {
    int inode_id = item->inode;

    nd->references--;
    write_inode_to_vfs(vfs, inode_id);

    update_sizes_in_file(vfs, parent, -(nd->file_size));

    if (update_directory_in_file(vfs, parent, item, false) == ERROR_CODE) {
        nd->references++;
        write_inode_to_vfs(vfs, inode_id);
        update_sizes_in_file(vfs, parent, nd->file_size);
        return ERROR_CODE;
    }

    remove_and_free_dir_entry(parent, item->item_name);

    return NO_ERROR_CODE;
}


int handle_full_file_removal(VFS **vfs, directory *parent,dir_item *item, inode *nd) {
    int block_count = 0, rest = 0;
    int32_t *blocks = NULL;
    int result = ERROR_CODE;

    blocks = get_data_blocks(vfs, item->inode, &block_count, &rest);
    if (!blocks) {
        printf("ERROR: Failed to retrieve data blocks\n");
        goto cleanup;
    }

    if (zero_data_blocks(vfs, blocks, block_count) == ERROR_CODE) {
        printf("ERROR: Failed to zero data blocks\n");
        goto cleanup;
    }

    if (zero_indirect_blocks(vfs, nd->indirect1, nd->indirect2) == ERROR_CODE) {
        printf("ERROR: Failed to zero indirect blocks\n");
        goto cleanup;
    }

    flush_vfs(vfs);

    update_bitmap_in_file(vfs, item, 0, blocks, block_count);

    update_sizes_in_file(vfs, parent, -(nd->file_size));

    if (update_directory_in_file(vfs, parent, item, false) == ERROR_CODE) {
        printf("ERROR: Failed to update directory on disk\n");
        goto cleanup;
    }

    reset_inode(nd);
    write_inode_to_vfs(vfs, item->inode);

    remove_and_free_dir_entry(parent, item->item_name);

    result = NO_ERROR_CODE;

    cleanup:
    free(blocks);
    return result;
}
