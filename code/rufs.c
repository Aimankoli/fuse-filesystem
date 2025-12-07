/*
 *  Copyright (C) 2025 CS416 Rutgers CS
 *	Rutgers Tiny File System
 *	File:	rufs.c
 *
 */

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

char diskfile_path[PATH_MAX];

// Root inode number is 0 in our implementation (although textbook says its usually 2)
#define ROOT_INO 0


// Declare your in-memory data structures here
// In-memory data structures
static struct superblock *sb = NULL;
static bitmap_t inode_bitmap = NULL;
static bitmap_t data_bitmap = NULL;

/* 
 * Get available inode number from bitmap
 */
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

		if (get_bitmap(data_bitmap, i) == 0){
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

/* 
 * inode operations
 */
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


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

	// Step 1: Call readi() to get the inode using ino (inode number of current directory)

	// Step 2: Get data block of current directory from inode

	// Step 3: Read directory's data block and check each directory entry.
	// If the name matches, then copy directory entry to dirent structure

	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
}


/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

	return 0;
}

/* 
 * Make file system
 */
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


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs

	// Step 1b: If disk file is found, just initialize in-memory data structures
	// and read superblock from disk

	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

	stbuf->st_mode   = S_IFDIR | 0755;
	stbuf->st_nlink  = 2;
	time(&stbuf->st_mtime);

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	
	return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}


/* 
 * Functions you DO NOT need to implement for this project
 * (stubs provided for completeness)
 */

static int rufs_rmdir(const char *path) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_unlink(const char *path) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.mkdir		= rufs_mkdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,

	//Operations that you don't have to implement.
	.rmdir		= rufs_rmdir,
	.releasedir	= rufs_releasedir,
	.unlink		= rufs_unlink,
	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}

