#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "helpers.h"
#include <sys/stat.h>
#include "vfs.h"


/*
 * Returns true if string str1 equals str2
 */
bool streq(const char *str1, const char *str2) {
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

/*
 * Reads a single line from standard input.
 * The buffer is dynamically resized as needed and always
 * null-terminated. The caller is responsible for freeing
 * the returned memory.
 *
 * Returns NULL on EOF or allocation failure.
 */
char *get_line() {
    size_t cap = 128;
    size_t len = 0;

    char *buf = malloc(cap);
    if (!buf) return NULL;

    int c;
    while ((c = fgetc(stdin)) != EOF) {

        if (len + 1 >= cap) {
            size_t newcap = cap * 2;
            char *tmp = realloc(buf, newcap);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
            cap = newcap;
        }

        buf[len++] = (char)c;

        if (c == '\n')
            break;
    }

    if (len == 0 && feof(stdin)) {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    return buf;
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
    sb->signature[SIGNATURE_LENGTH - 1] = '\0';


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

/*
 * Creates and initializes a directory item structure.
 * The item stores the inode reference and its name.
 */
dir_item *create_directory_item(int32_t inode_id, const char *name) {
    dir_item *dir_item = calloc(1, sizeof(struct DIR_ITEM));
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
    for (int32_t i = 0 ; i < (*vfs)->superblock->inode_count; i++){
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
        // printf("Buffer %s\n", buff);
        // printf("Path %s\n", path);
        if (path[0] == '/' && len == 0) {
            strcpy(buff, "/");
        }
        // printf("Buffer %s\n", buff);
        *dir = find_directory(vfs, buff);
        // printf("From parse: %s\n", (*dir)->current->item_name);
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
    directory *current;

    // Start from root for absolute path
    // printf("From find: %s\n", path);
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

/*
 * Prints the contents of a directory.
 * Lists subdirectories first, followed by files.
 * If no items exist in a category, "<none>" is shown.
 */
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

/*
 * Removes a directory item with the given name from a linked list.
 * The removed item is detached and returned to the caller,
 * who becomes responsible for freeing it.
 *
 * Returns NULL if the item was not found.
 */
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

/*
 * Checks whether a file exists in the real filesystem.
 *
 * Returns true if the file is found, false otherwise.
 */
bool file_exists (char *filename) {
    struct stat   buffer;
    return (stat (filename, &buffer) == 0);
}

/*
 * Appends a directory item to the end of a linked list.
 */
void add_item_to_list(dir_item **list_head, dir_item *new_item) {
    if (!list_head || !new_item) return;

    dir_item **current = list_head;
    while (*current) {
        current = &((*current)->next);
    }
    *current = new_item;
}

/*
 * Validates that a new file name fits into directory entry limits.
 */
bool validate_new_item_name(char *name) {
    if (strlen(name) >= MAX_ITEM_NAME_LENGTH) {
        printf(NAME_TOO_LONG, MAX_ITEM_NAME_LENGTH - 1);
        return false;
    }
    return true;
}

/*
 * Creates a new directory structure in memory and links it
 * with its parent directory and inode.
 * The created directory and its dir_item are returned via output parameters.
 */
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

/*
 * Synchronizes directory changes, inode state, and bitmap updates
 * to the virtual filesystem file.
 */
bool sync_to_disk(VFS **vfs, directory *parent,dir_item *item, int inode_id,int32_t *block, bool create, int8_t value)
{
    if (update_directory_in_file(vfs, parent, item, create) == ERROR_CODE) {
        return false;
    }

    update_bitmap_in_file(vfs, item, value, block, 1);
     if (create == false) {
        reset_inode(&(*vfs)->inodes[item->inode]);
    }

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

        size_t bytes_to_read =
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

/*
 * Searches for a file or subdirectory with the given name
 * inside a directory.
 *
 * Returns the corresponding dir_item or NULL if not found.
 */
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

/*
 * Removes a dir_item from a directory list in memory and frees the allocated memory.
 * This handles only the in-memory structure, not the VFS file metadata.
 *
 * A tiny but crucial helper for rm, mv, and rmdir.
 */
void remove_and_free_dir_entry(directory *parent, const char *name) {
    dir_item *detached = remove_diritem(&parent->file, name);
    if (detached) {
        detached->next = NULL;
        free(detached);
    }
}

/*
 * Handles the case when a file has more than one reference (hardlinks).
 * In this scenario:
 *   - Only the directory entry is removed
 *   - The inode's reference count is decremented
 *   - Parent directory size is updated
 *   - VFS is synced (directory table, inode)
 *
 * Ensures rollback if any disk operation fails.
 */
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


/*
 * Removes a file completely (last reference):
 *   - Retrieves all its data blocks (direct & indirect)
 *   - Zeroes their content
 *   - Clears indirect block tables
 *   - Updates bitmap to free these blocks
 *   - Updates parent directory sizes
 *   - Removes directory entry (both in memory and on disk)
 *   - Resets inode and writes it back
 *
 * Performs cleanup and error reporting if any step fails.
 */
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

/*
 * Extracts the filename from a filesystem path.
 * If no path separator is found, the whole string is returned.
 */
char* extract_filename_from_path(const char *path) {
    char *name = strrchr(path, '\\');

    if (name == NULL) {
        return (char *)path;
    } else {
        return name + 1;
    }
}

/*
 * Retrieves the size of a real filesystem file without changing
 * the current file position.
 */
int get_file_size(FILE *file, int32_t *size_out) {
    if (!file || !size_out)
        return ERROR_CODE;

    long pos = ftell(file);
    if (pos < 0) return ERROR_CODE;

    if (fseek(file, 0, SEEK_END) != 0)
        return ERROR_CODE;

    long size = ftell(file);
    if (size < 0) return ERROR_CODE;

    *size_out = (int32_t)size;

    // Restore original file position
    fseek(file, pos, SEEK_SET);
    return NO_ERROR_CODE;
}

/*
 * Reverts all filesystem changes made during a failed import operation.
 * This includes directory entries, bitmap updates, and inode allocation.
 */
void rollback_import(VFS **vfs, directory *dir, dir_item *item,
                     int32_t *blocks, int block_count, int inode_id) {
    if (item) {
        remove_diritem(&dir->file, item->item_name);
        update_directory_in_file(vfs, dir, item, false);
    }

    if (blocks) {
        update_bitmap_in_file(vfs, item, 0, blocks, block_count);
    }

    if (inode_id != ERROR_CODE) {
        reset_inode(&(*vfs)->inodes[inode_id]);
        write_inode_to_vfs(vfs, inode_id);
    }
}


/*
 * Resolves a directory name inside a parent directory.
 * Returns the directory if the name exists and refers to a directory inode.
 */
directory *resolve_destination_directory(VFS **vfs, directory *parent_dir,
                                         char *name) {
    if (!parent_dir || !name || *name == '\0') {
        return NULL;
    }

    dir_item *item = find_directory_by_name(parent_dir->subdir, name);
    if (!item) {
        return NULL;
    }

    inode *node = &(*vfs)->inodes[item->inode];
    if (!node->isDirectory) {
        return NULL;
    }
    return (*vfs)->all_dirs[item->inode];
}


/*
 * Determines the final destination directory and file name.
 * If dest_path points to a directory, the source name is reused.
 * Otherwise, dest_path is treated as a new file name.
 */
bool smart_parse_destination(VFS **vfs, const char *src_name,
                            const char *dest_path,
                            char **final_name, directory **final_dir) {
    char *dest_name = NULL;
    directory *dest_dir = NULL;

    if (parse_path(vfs, (char *)dest_path, &dest_name, &dest_dir) == ERROR_CODE) {
        return false;
    }

    if (!dest_dir) {
        return false;
    }

    if (str_empty(dest_name)) {
        *final_name = (char *)src_name;
        *final_dir = dest_dir;
        return true;
    }

    directory *target_subdir = resolve_destination_directory(vfs, dest_dir, dest_name);

    if (target_subdir) {
        *final_name = (char *)src_name;
        *final_dir = target_subdir;
    } else {
        *final_name = dest_name;
        *final_dir = dest_dir;
    }
    return true;
}