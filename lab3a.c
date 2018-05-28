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
	int bytes_read = pread(fd, &groups_data, sizeof(struct ext2_group_desc), 1024 + sizeof(struct ext2_group_desc));
	if(bytes_read < 0){
		fprintf(stderr, "Error reading groups: %s\n", strerror(errno));
		exit(2);
	}
	int num_groups = 1 + superblock.s_blocks_count / superblock.s_blocks_per_group;
	int i=0;
	while(i < num_groups){
		
	}
}

int main(int argc, char** argv){
	if(argc != 2){
		fprintf(stderr, "Error: invalid number of parameters given.\n");
		exit(1);
	}
	char* file_system = argv[1];
	int fd = open(file_system, O_RDONLY);
	scan_superblock(fd);
	scan_groups(fd);

	exit(0);
}