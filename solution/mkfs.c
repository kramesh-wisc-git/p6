#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "wfs.h"

int main(int argc, char *argv[]) {
    if(argc < 9) { // invalid number of args
        exit(1);
    }
    FILE *disk_ptrs[10] = {0};
    int idx = 0;
    int raid_mode, numb_inodes, numb_data_blocks;
    int opt;
    while((opt = getopt(argc, argv, "r:d:i:b:")) != -1) {
        if(opt == 'r') { // raid mode
            raid_mode = atoi(optarg);
            if(!(raid_mode == 0 || raid_mode == 1)) exit(1);
        }
        else if(opt == 'd') { // disk image file
            disk_ptrs[idx] = fopen(optarg, "r+");
            if(!disk_ptrs[idx]) { // fopen failed
                exit(1);
            }
            idx++;
        }
        else if(opt == 'i') { // number of inodes
            numb_inodes = atoi(optarg);
            if(numb_inodes <= 0) exit(1);

            // align numb_inodes to a multiple of 32
            int rem = numb_inodes & (32-1);
            if(rem) numb_inodes += (32 - rem);
        }
        else if(opt == 'b') { // number of data blocks
            numb_data_blocks = atoi(optarg);
            if(numb_data_blocks <= 0) exit(1);

            // align numb_data_blocks to multiple of 32
            int rem = numb_data_blocks & (32-1);
            if(rem) numb_data_blocks += (32 - rem);
        }
    }
    
    // check for atleast 2 disk image files 
    if(idx <= 1) exit(1);

    // check file size
    struct stat st;
    fstat(fileno(disk_ptrs[0]), &st);
    int disk_size = st.st_size;
    int req_size = sizeof(struct wfs_sb) + numb_inodes / 8 + numb_data_blocks / 8 + BLOCK_SIZE * (numb_inodes + numb_data_blocks);
    if(req_size > disk_size) { // disk size is too small
        exit(-1);
    }

    // init bitmaps
    unsigned char i_bitmap[numb_inodes / 8];
    unsigned char d_bitmap[numb_data_blocks / 8];
    memset(i_bitmap, 0, sizeof(i_bitmap));
    memset(d_bitmap, 0, sizeof(d_bitmap));

    // init superblock
    struct wfs_sb sb;
    sb.num_inodes = numb_inodes;
    sb.num_data_blocks = numb_data_blocks;
    sb.i_bitmap_ptr = sizeof(struct wfs_sb);
    sb.d_bitmap_ptr = sb.i_bitmap_ptr + sizeof(i_bitmap);

    // align to 512 bytes
    sb.i_blocks_ptr = sb.d_bitmap_ptr + sizeof(d_bitmap);
    int rem = sb.i_blocks_ptr & (512-1);
    if(rem) sb.i_blocks_ptr += (512 - rem);
    sb.d_blocks_ptr = sb.i_blocks_ptr + BLOCK_SIZE * numb_inodes;
    sb.raid_mode = raid_mode;

    // init root inode
    struct wfs_inode inode;
    inode.num = 0;
    inode.mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR;
    inode.uid = getpid();
    inode.gid = inode.uid;
    inode.size = 0;
    inode.nlinks = 1;
    inode.atim = time(NULL);
    inode.mtim = inode.atim;
    inode.ctim = inode.atim;
    memset(inode.blocks, 0, sizeof(inode.blocks));

    i_bitmap[0] |= 1; // set inode bitmap for root inode

    // write superblock, bitmaps, and root inode to each disk image file
    for(int i = 0; i < idx; i++) {
        if(disk_ptrs[i]) {
            fwrite(&sb, sizeof(sb), 1, disk_ptrs[i]);
            fseek(disk_ptrs[i], sb.i_bitmap_ptr, 0);
            fwrite(&i_bitmap, sizeof(i_bitmap), 1, disk_ptrs[i]);
            fseek(disk_ptrs[i], sb.d_bitmap_ptr, 0);
            fwrite(&d_bitmap, sizeof(d_bitmap), 1, disk_ptrs[i]);
            fseek(disk_ptrs[i], sb.i_blocks_ptr, 0);
            fwrite(&inode, sizeof(inode), 1, disk_ptrs[i]);
        }
    }
    return 0;
}