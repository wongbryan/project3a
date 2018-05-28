/*
NAME: Steven La, Bryan Wong
EMAIL: stevenla@g.ucla.edu, bryanw0ng@g.ucla.edu
ID: 004781046, 504744476
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
struct ext2_group_desc groups_data;

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
	struct ext2_group_desc group_descripts[num_groups*sizeof(struct ext2_group_desc)];
	int bytes_read = pread(fd, group_descripts, num_groups*sizeof(struct ext2_group_desc), 1024+sizeof(struct ext2_super_block));
	if(bytes_read < 0){
		fprintf(stderr, "Error reading groups: %s\n", strerror(errno));
		exit(2);
	}
	int i = 0;
	int o_inodecount = superblock.s_inodes_count;
	int o_inodegroup = superblock.s_inodes_per_group;
	int o_blockcount = superblock.s_blocks_count;
	int o_blockgroup = superblock.s_blocks_per_group;
	while(i < num_groups){
		if (o_blockcount < superblock.s_blocks_per_group)
			o_blockgroup = o_blockcount;
		if (o_inodecount < superblock.s_inodes_per_group)
			o_inodegroup = o_inodecount;
		fprintf(stdout, "GROUP,%u,%u,%u,%u,%u,%u,%u,%u\n",
			i,
			o_blockgroup,
			o_inodegroup,
			group_descripts[i].bg_free_blocks_count,
			group_descripts[i].bg_free_inodes_count,
			group_descripts[i].bg_block_bitmap,
			group_descripts[i].bg_inode_bitmap,
			group_descripts[i].bg_inode_table
			);
		o_inodecount -= superblock.s_inodes_per_group;
		o_blockcount -= superblock.s_blocks_per_group;
		i++;
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

	exit(0);
}