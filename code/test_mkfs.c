#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "rufs.h"


// Root inode number is 0 in our implementation (although textbook says its usually 2)
#define ROOT_INO 0

// Declare global variables (normally in rufs.c)
char diskfile_path[PATH_MAX] = "./TEST_DISK";
struct superblock *sb = NULL;
bitmap_t inode_bitmap = NULL;
bitmap_t data_bitmap = NULL;

// Forward declarations
int get_avail_ino();
int get_avail_blkno();
int readi(uint16_t ino, struct inode *inode);
int writei(uint16_t ino, struct inode *inode);
int rufs_mkfs();

int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	sb = malloc(sizeof(struct superblock));
	memset(sb, 0, sizeof(struct superblock));
	sb->magic_num = MAGIC_NUM;
	sb->max_inum = MAX_INUM;
	sb->max_dnum = MAX_DNUM;
	sb->i_bitmap_blk = 1;
	sb->d_bitmap_blk = 2;
	sb->i_start_blk = 3;

	int inodes_per_block = BLOCK_SIZE / sizeof(struct inode);
	int blocks_for_inodes = (MAX_INUM + inodes_per_block - 1)/ inodes_per_block;
	sb->d_start_blk = sb->i_start_blk + blocks_for_inodes;

	// Write superblock to disk
    void *buffer = malloc(BLOCK_SIZE);
    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, sb, sizeof(struct superblock));
    bio_write(0, buffer);

	// initialize inode bitmap
	inode_bitmap = malloc(BLOCK_SIZE);
	memset(inode_bitmap, 0, BLOCK_SIZE);
	set_bitmap(inode_bitmap, ROOT_INO); // mark root inode as used
	bio_write(sb->i_bitmap_blk, inode_bitmap);


	// initialize data block bitmap
	data_bitmap = malloc(BLOCK_SIZE);
	memset(data_bitmap, 0, BLOCK_SIZE);
	bio_write(sb->d_bitmap_blk, data_bitmap);

	// update bitmap information for root directory
		//already done above

	// update inode for root directory
	//get a block
	int root_data_block = get_avail_blkno();

	struct inode root_inode;
    memset(&root_inode, 0, sizeof(struct inode));
    root_inode.ino = ROOT_INO;
    root_inode.valid = 1;
    root_inode.size = BLOCK_SIZE;
    root_inode.type = S_IFDIR;
    root_inode.link = 2;
    root_inode.direct_ptr[0] = root_data_block;
    
    // Set permissions and timestamps
    root_inode.vstat.st_mode = S_IFDIR | 0755;
    root_inode.vstat.st_nlink = 2;
    root_inode.vstat.st_uid = getuid();
    root_inode.vstat.st_gid = getgid();
    time(&root_inode.vstat.st_mtime);
    root_inode.vstat.st_atime = root_inode.vstat.st_mtime;
    
    writei(ROOT_INO, &root_inode);

	// Initialize root directory data block
    memset(buffer, 0, BLOCK_SIZE);
    bio_write(sb->d_start_blk + root_data_block, buffer);
    
    free(buffer);
    return 0;
}

int readi(uint16_t ino, struct inode *inode) {

	// Step 1: Get the inode's on-disk block number
	int inodes_per_block = BLOCK_SIZE / sizeof(struct inode);
	
	int block_num = sb->i_start_blk + (ino / inodes_per_block);

	// Step 2: Get offset of the inode in the inode on-disk block
	int offset = (ino % inodes_per_block) * sizeof(struct inode);

	// Step 3: Read the block from disk and then copy into inode structure
	void *buffer = malloc(BLOCK_SIZE);
	if (bio_read(block_num, buffer) < 0) {
		perror("Failed to read inode block");
		free(buffer);
		return -1;
	}
	memcpy(inode, (char*)buffer + offset, sizeof(struct inode));
	free(buffer);
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int inodes_per_block = BLOCK_SIZE / sizeof(struct inode);
    
    int block_num = sb->i_start_blk + (ino / inodes_per_block);
	// Step 2: Get the offset in the block where this inode resides on disk
	int offset = (ino % inodes_per_block) * sizeof(struct inode);

	// Step 3: Write inode to disk 
	//read block into memopry, modify with memcpy, write back to disk
	void *buffer = malloc(BLOCK_SIZE);
	if (bio_read(block_num, buffer) < 0) {
		perror("Failed to read inode block");
		free(buffer);
		return -1;
	}
	memcpy((char*)buffer + offset, inode, sizeof(struct inode));
	if (bio_write(block_num, buffer) < 0) {
		perror("Failed to write inode block");
		free(buffer);
		return -1;
	}
	free(buffer);

	return 0;
}

int get_avail_ino() {

	// Step 1: Read inode bitmap from disk

	if (inode_bitmap == NULL) {
		inode_bitmap = malloc(BLOCK_SIZE);

		if (bio_read(sb->i_bitmap_blk, inode_bitmap) < 0) {
			perror("Failed to read inode bitmap");
			return -1;
		}
	}
	//checks done, now lets move on


	// Step 2: Traverse inode bitmap to find an available slot
	for (int i = 0; i<MAX_INUM; i++){
		if (get_bitmap(inode_bitmap, i) == 0){
			//FOUND AN AVAILABLE INODE!!

			
			// Step 3: Update inode bitmap and write to disk
			set_bitmap(inode_bitmap, i);
			if (bio_write(sb->i_bitmap_blk, inode_bitmap) < 0) {
				perror("Failed to write inode bitmap");
				return -1;
			}
			return i;
		}
	} 
	printf("No available inode found\n");
	return -1; //no available inode found
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	if (data_bitmap == NULL) {
		data_bitmap = malloc(BLOCK_SIZE);

		if (bio_read(sb->d_bitmap_blk, data_bitmap) < 0) {
			perror("Failed to read data block bitmap");
			return -1;
		}
	}
	
	// Step 2: Traverse data block bitmap to find an available slot
	for (int i = 0; i<MAX_DNUM; i++){

		if (get_bitmap(data_bitmap, i == 0)){
			//FOUND AN AVAILABLE DATA BLOCK!!
			// Step 3: Update data block bitmap and write to disk 

			set_bitmap(data_bitmap, i);
			if (bio_write(sb->d_bitmap_blk, data_bitmap) < 0) {
				perror("Failed to write data block bitmap");
				return -1;
			}
			return i;
		}
	}
	return -1; //no available data block found
}

int main() {
    printf("Testing rufs_mkfs...\n");
    
    // Create filesystem
    if (rufs_mkfs() < 0) {
        printf("FAILED: rufs_mkfs returned error\n");
        return 1;
    }
    printf("✓ rufs_mkfs succeeded\n");
    
    // Test: Read root inode
    struct inode root;
    if (readi(0, &root) < 0) {
        printf("FAILED: Couldn't read root inode\n");
        return 1;
    }
    printf("✓ Read root inode\n");
    
    // Verify root inode
    if (root.ino != 0) {
        printf("FAILED: Root ino is %d, expected 0\n", root.ino);
        return 1;
    }
    printf("✓ Root inode number is correct\n");
    
    if (root.type != S_IFDIR) {
        printf("FAILED: Root type is not directory\n");
        return 1;
    }
    printf("✓ Root is a directory\n");
    
    if (root.link != 2) {
        printf("FAILED: Root link count is %d, expected 2\n", root.link);
        return 1;
    }
    printf("✓ Root link count is correct\n");
    
    // Test: Allocate a new inode
    int new_ino = get_avail_ino();
    if (new_ino < 0) {
        printf("FAILED: Couldn't allocate inode\n");
        return 1;
    }
    printf("✓ Allocated inode %d\n", new_ino);
    
    // Test: Allocate a new data block
    int new_block = get_avail_blkno();
    if (new_block < 0) {
        printf("FAILED: Couldn't allocate data block\n");
        return 1;
    }
    printf("✓ Allocated data block %d\n", new_block);
    
    printf("\n✅ All tests passed!\n");
    
    // Cleanup
    dev_close();
    free(sb);
    free(inode_bitmap);
    free(data_bitmap);
    
    return 0;
}