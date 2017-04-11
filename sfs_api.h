
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define NAME "260639146.ssfs"
#define BLOCK_SIZE 1024
#define MAGIC_NUM 0xABCD0005
#define MAX_FILENAME 11 // extra space for null character
#define MAX_FD_ENTRY 32
#define NUM_BLOCKS 1026
#define NUM_DATA_BLOCKS 1024
#define NUM_POINTERS 14
#define NUM_INODES 63
#define NUM_INODE_BLOCKS 4
#define NUM_FILES 63

typedef struct
{
    char name[MAX_FILENAME];
    int inode;
} file_t;

typedef struct
{
    int id;
    int block_size;
    int num_blocks;
    int num_inodes;
    file_t root[NUM_FILES];
} super_block_t;

typedef struct
{
    char name[MAX_FILENAME];
    int inode;
    int write_pointer;
    int read_pointer;
} file_descriptor_t;

typedef struct
{
    int size;
    int ind_pointer;
    int pointers[NUM_POINTERS];
} inode_t;

typedef struct
{
    uint32_t bits[BLOCK_SIZE / sizeof(uint32_t)];
} bitmap_t;

void mkssfs(int fresh);
int ssfs_get_next_file_name(char *fname);
int ssfs_get_file_size(char *path);
int ssfs_fopen(char *name);
int ssfs_fclose(int fileID);
int ssfs_frseek(int fileID, int loc);
int ssfs_fwseek(int fileID, int loc);
int ssfs_fwrite(int fileID, char *buf, int length);
int ssfs_fread(int fileID, char *buf, int length);
int ssfs_remove(char *file);
