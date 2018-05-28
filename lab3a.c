/*
NAME: Steven La,Bryan Wong
EMAIL: stevenla@g.ucla.edu,bryanw0ng@g.ucla.edu
ID: 004781046,504744476
*/

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include "ext2_fs.h"

struct ext2_super_block superblock;
struct ext2_group_desc *groups_data;

int *inode_bitmap;

void scan_superblock(int fd){
	int bytes_read = pread(fd, &superblock, sizeof(struct ext2_super_block), 1024);
	if(bytes_read < 0){
		fprintf(stderr, "Error reading superblock: %s\n", strerror(errno));
		exit(2);
	}
	int num_blocks = superblock.s_blocks_count;
	int num_inodes = superblock.s_inodes_count;
	int block_size = 1024 << superblock.s_log_block_size;
	int inode_size = superblock.s_inode_size;
	int blocks_per_group = superblock.s_blocks_per_group;
	int inodes_per_group = superblock.s_inodes_per_group;
	int first_inode = superblock.s_first_ino;

	fprintf(stdout, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", 
		num_blocks,
		num_inodes,
		block_size,
		inode_size,
		blocks_per_group,
		inodes_per_group,
		first_inode
	);
}

void scan_groups(int fd){
	int num_groups = 1 + (superblock.s_blocks_count-1) / superblock.s_blocks_per_group;
	groups_data = malloc(num_groups*sizeof(struct ext2_group_desc));
	if (groups_data == NULL) {
		fprintf(stderr, "Error in allocating dynamic memory: %s\n", strerror(errno));
		exit(2);
	}
	int bytes_read = pread(fd, groups_data, num_groups*sizeof(struct ext2_group_desc), 1024+sizeof(struct ext2_super_block));
	if(bytes_read < 0){
		fprintf(stderr, "Error reading groups: %s\n", strerror(errno));
		exit(2);
	}
	int i = 0;
	unsigned int o_inodecount = superblock.s_inodes_count;
	unsigned int o_inodegroup = superblock.s_inodes_per_group;
	unsigned int o_blockcount = superblock.s_blocks_count;
	unsigned int o_blockgroup = superblock.s_blocks_per_group;
	while(i < num_groups){
		if (o_blockcount < superblock.s_blocks_per_group)
			o_blockgroup = o_blockcount;
		if (o_inodecount < superblock.s_inodes_per_group)
			o_inodegroup = o_inodecount;
		fprintf(stdout, "GROUP,%u,%u,%u,%u,%u,%u,%u,%u\n",
			i,
			o_blockgroup,
			o_inodegroup,
			groups_data[i].bg_free_blocks_count,
			groups_data[i].bg_free_inodes_count,
			groups_data[i].bg_block_bitmap,
			groups_data[i].bg_inode_bitmap,
			groups_data[i].bg_inode_table
			);
		o_inodecount -= superblock.s_inodes_per_group;
		o_blockcount -= superblock.s_blocks_per_group;
		i++;
	}
}

void free_blocks(int fd) {
	unsigned long num_groups = 1 + (superblock.s_blocks_count-1) / superblock.s_blocks_per_group;
	unsigned long block_size = 1024 << superblock.s_log_block_size;
	unsigned long i = 0;
	unsigned long j = 0;
	unsigned long k = 0;
	for (i = 0; i < num_groups; i++) {
		for (j = 0; j < block_size; j++) {
			uint8_t byte;
			int pos_bitmask = 1;
			int r = pread(fd, &byte, 1, (block_size*groups_data[i].bg_block_bitmap)+j);
			if (r < 0) {
				fprintf(stderr, "Error reading byte from block's bitmap: %s\n", strerror(errno));
				exit(2);
			}
			for (k = 0; k < 8; k++) {
				int c = byte&pos_bitmask;
				if (c == 0) {
					fprintf(stdout, "BFREE,%lu\n", i * superblock.s_blocks_per_group + j * 8 + k + 1);
				}
				pos_bitmask <<= 1;
			}
		}
	}
}

void free_inode(int fd) {
	unsigned long num_groups = 1 + (superblock.s_blocks_count-1) / superblock.s_blocks_per_group;
	unsigned long block_size = 1024 << superblock.s_log_block_size;
	inode_bitmap = malloc(block_size*num_groups*sizeof(uint8_t));
	if (inode_bitmap == NULL) {
		fprintf(stderr, "Error in allocating dynamic memory: %s\n", strerror(errno));
		exit(2);
	}
	unsigned long i = 0;
	unsigned long j = 0;
	unsigned long k = 0;
	for (i = 0; i < num_groups; i++) {
		for (j = 0; j < block_size; j++) {
			uint8_t byte;
			int pos_bitmask = 1;
			int r = pread(fd, &byte, 1, (block_size*groups_data[i].bg_inode_bitmap)+j);
			if (r < 0) {
				fprintf(stderr, "Error reading byte from inode's bitmap: %s\n", strerror(errno));
				exit(2);
			}
			inode_bitmap[j+i] = byte;
			for (k = 0; k < 8; k++) {
				int c = byte&pos_bitmask;
				if (c == 0)
					fprintf(stdout, "IFREE,%lu\n", i * superblock.s_inodes_per_group + j * 8 + k + 1);
				pos_bitmask <<= 1;
			}
		}
	}
}

int main(int argc, char** argv){
	// option parsing
	if(argc != 2){
		fprintf(stderr, "Error: invalid number of arguments.\n");
		exit(1);
	}
	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Error: could not open file system.\n");
		exit(1);
	}
	// produce CSV summaries
	scan_superblock(fd);
	scan_groups(fd);
	free_blocks(fd);
	free_inode(fd);

	free(groups_data);
	free(inode_bitmap);
	exit(0);
}