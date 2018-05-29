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
#include <sys/stat.h>
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
	int intermediate = superblock.s_blocks_count-1;
	int num_groups = 1 + (intermediate) / superblock.s_blocks_per_group;
	int i = 0;
	unsigned int o_inodecount = superblock.s_inodes_count;
	unsigned int o_inodegroup = superblock.s_inodes_per_group;
	unsigned int o_blockcount = superblock.s_blocks_count;
	unsigned int o_blockgroup = superblock.s_blocks_per_group;

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
	while(i < num_groups){
		if (o_inodecount < superblock.s_inodes_per_group)
			o_inodegroup = o_inodecount;
		if (o_blockcount < superblock.s_blocks_per_group)
			o_blockgroup = o_blockcount;
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
		i += 1;
	}
}

void free_blocks(int fd) {
	unsigned long intermediate = superblock.s_blocks_count-1;
	unsigned long num_groups = 1 + (intermediate) / superblock.s_blocks_per_group;
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
				pos_bitmask *= 2;
			}
		}
	}
}

void free_inode(int fd) {
	unsigned long intermediate = superblock.s_blocks_count-1;
	unsigned long num_groups = 1 + (intermediate) / superblock.s_blocks_per_group;
	unsigned long block_size = 1024 << superblock.s_log_block_size;
	unsigned long total_size = block_size*num_groups*sizeof(uint8_t);
	inode_bitmap = malloc(total_size*5); //for some reason memory is getting overwritten -> just allocate more than needed for now
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
				pos_bitmask *= 2;
			}
		}
	}
}

void format_time(char* buf, uint32_t timestamp, int size) {
	time_t casted_timestamp = timestamp;
	struct tm gm_time;
	gm_time = *gmtime(&casted_timestamp);
	strftime(buf, size, "%m/%d/%y %H:%M:%S", &gm_time);
}

void scan_indirect_block(int fd, struct ext2_inode* inode, int parent_inode_num, int block_num, int base_offset){
	unsigned long block_size = 1024 << superblock.s_log_block_size;
	int indirect_block[block_size];

	if(pread(fd, indirect_block, block_size, 1024 + (block_num-1)*block_size) < 0) {
		fprintf(stderr, "Error reading blocks for directory: %s\n", strerror(errno));
	}

	fprintf(stderr, "INDIRECT BLOCK\n");

	unsigned int i=0;
	while(i < block_size/4){
		fprintf(stdout, "INDIRECT,%d,1,%d,%d,%d\n",
			parent_inode_num,
			base_offset + i,
			block_num,
			indirect_block[i] 
		);
		i++;
	}
}

void scan_direct_block(int fd, struct ext2_inode* inode, int parent_inode_num, int block_num){
	unsigned long block_size = 1024 << superblock.s_log_block_size;
	unsigned char block[block_size];
	if (pread(fd, block, block_size, 1024 + (block_num-1)*block_size) < 0) {
		fprintf(stderr, "Error reading blocks for directory: %s\n", strerror(errno));
	}
	struct ext2_dir_entry* dir_entry = (struct ext2_dir_entry*) block;
	unsigned int offset = 0;
	while((offset < inode->i_size) && dir_entry->file_type){
		if(dir_entry->name_len > 0 && dir_entry->inode > 0){
			int inode_num = dir_entry->inode;
			int entry_length = dir_entry->rec_len;
			int name_length = dir_entry->name_len;
			char* name = dir_entry->name;

			fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,'%s'\n",
				parent_inode_num,
				offset,
				inode_num,
				entry_length,
				name_length,
				name
			);
		}
		offset = offset + dir_entry->rec_len;
  		dir_entry = (void*)dir_entry + dir_entry->rec_len;
	}
}

void scan_directory(int fd, struct ext2_inode* inode, int parent_inode_num){
	unsigned long block_size = 1024 << superblock.s_log_block_size;
	unsigned char block[block_size];
	int b;
	for(b=0; b<12; b++){ //scan direct blocks
		int block_num = inode->i_block[b];
		scan_direct_block(fd, inode, parent_inode_num, block_num);
	}
	if(inode->i_block[12] != 0){
		scan_indirect_block(fd, inode, parent_inode_num, inode->i_block[12], 12);
	}
}

void scan_inodes(int fd){
	int i=0;
	int intermediate = superblock.s_blocks_count-1;
	int num_groups = 1 + (intermediate) / superblock.s_blocks_per_group;
	int num_inodes = superblock.s_inodes_count;
	unsigned long block_size = 1024 << superblock.s_log_block_size;

	while(i < num_groups){
		int inode_table = groups_data[i].bg_inode_table;
		int inode_num;
		for(inode_num=0; inode_num<num_inodes; inode_num++){
			struct ext2_inode inode;
			if(pread(fd, &inode, sizeof(struct ext2_inode), (1024 + (block_size * (inode_table-1)) + ((inode_num-1)*sizeof(struct ext2_inode)))) < 0){
				fprintf(stderr, "Error reading inode data: %s\n", strerror(errno));
				exit(2);
			}

			char file_type;

			int file_mode = inode.i_mode;
			if(S_ISDIR(file_mode)) {
				file_type = 'd';
			} else if (S_ISREG(file_mode)) {
				file_type = 'f';
			} else if (S_ISLNK(file_mode)) {
				file_type = 's';
			}
			else{
				file_type = '?';
			}

			char creation_time[32];
			char modified_time[32];
			char access_time[32];

			int owner = inode.i_uid;
			int group = inode.i_gid;
			int link_count = inode.i_links_count;
			int ctime = inode.i_ctime;
			format_time(creation_time, ctime, 32);
			int mtime = inode.i_mtime;
			format_time(modified_time, mtime, 32);
			int atime = inode.i_atime;
			format_time(access_time, atime, 32);
			int file_size = inode.i_size;
			int num_blocks = inode.i_blocks;

			fprintf(stdout, "INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d,",
				inode_num,
				file_type,
				file_mode & 4095,
				owner,
				group,
				link_count,
				creation_time,
				modified_time,
				access_time,
				file_size,
				num_blocks
			);

			if(file_type == 's' && num_blocks == 0){ //if short sym link, there are no blocks allocated -> do not print all 15 blocks
				fprintf(stdout, "%d\n", inode.i_block[0]);
			}
			else{
				int b;
				for(b=0; b<15; b++){
					char *format_str;
					if(b == 14){
						format_str = "%d\n";
					}
					else{
						format_str = "%d,";
					}
					fprintf(stdout, format_str, inode.i_block[b]);
				}
			}

			if(file_type == 'd'){
				scan_directory(fd, &inode, inode_num);
			}
		}
		++i;
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
	scan_inodes(fd);

	free(groups_data);
	free(inode_bitmap);
	exit(0);
}