#include "disk_emu.h"
#include "sfs_api.h"

// some global vars
bitmap_t free_bitmap;
inode_t inode_table[NUM_INODES];
file_descriptor_t file_descriptors[MAX_FD_ENTRY];
super_block_t super_block;

// set the k'th bit in the bit array arr
void set_bit(uint32_t arr[], int k)
{
    int i = k / 32;     // i = array index (use: arr[i])
    int pos = k % 32;   // pos = bit position in arr[i]
    uint32_t flag = 1;  // flag = 0000......00001
    flag = flag << pos; // flag = 0000...010...000 (shift k positions)

    arr[i] = arr[i] | flag; // set the bit at the k'th position in arr[i]
}

// clear the k'th bit in the bit array arr
void clear_bit(uint32_t arr[], int k)
{
    int i = k / 32;
    int pos = k % 32;
    uint32_t flag = 1;
    flag = flag << pos;     // flag = 0000...010...000 (shifted k positions)
    flag = ~flag;           // flag = 1111...101...111
    arr[i] = arr[i] & flag; // RESET the bit at the k'th position in arr[i]
}

// test the k'th bit in the bit array arr
int test_bit(uint32_t arr[], int k)
{
    int i = k / 32;
    int pos = k % 32;
    uint32_t flag = 1;
    flag = flag << pos;

    if (arr[i] & flag) // test the bit at the k-th position
        return 1;      // k'th bit is 1
    else
        return 0; // k'th bit is 0
}

// initialzies inode table
void initialize_inode_table()
{
    for (int i = 0; i < NUM_INODES; i++)
    {
        inode_table[i].ind_pointer = -1;
        inode_table[i].size = -1;

        for (int j = 0; j < NUM_POINTERS; j++)
            inode_table[i].pointers[j] = -1;
    }
}

// initializes free bitmap
void initialize_free_bitmap()
{
    for (int i = 0; i < NUM_DATA_BLOCKS; i++)
        clear_bit(free_bitmap.bits, i);

    for (int j = 0; j < 5; j++)
        set_bit(free_bitmap.bits, j);
}

// initializes fd table
void initialize_fd_table()
{
    // initialize the file descriptor fd_table
    for (int i = 0; i < MAX_FD_ENTRY; i++)
    {
        memset(file_descriptors[i].name, 0, sizeof(file_descriptors[i].name));
        file_descriptors[i].inode = -1;
        file_descriptors[i].write_pointer = 0;
        file_descriptors[i].read_pointer = 0;
    }
}

// initializes super block
void initialize_super_block()
{
    super_block.block_size = BLOCK_SIZE;
    super_block.id = MAGIC_NUM;
    super_block.num_blocks = NUM_BLOCKS;
    for (int i = 0; i < NUM_FILES; i++)
        super_block.root[i].inode = -1;
}

// writes the inode table to disk
int write_inode_table()
{
    void *buffer = malloc(NUM_INODE_BLOCKS * BLOCK_SIZE);
    if (buffer == NULL)
    {
        printf("ERROR (write_inode_table): could not allocate memory for buffer.\n");
        return -1;
    }
    memset(buffer, 0, NUM_INODE_BLOCKS * BLOCK_SIZE);
    memcpy(buffer, &inode_table, sizeof(inode_table));
    write_blocks(1, NUM_INODE_BLOCKS, buffer);
    free(buffer);
    return 0;
}

// writes free bitmap to disk
void write_free_bitmap()
{
    void *buffer = malloc(BLOCK_SIZE);
    if (buffer == NULL)
    {
        printf("ERROR (write_free_bitmap): could not allocate memory for buffer.\n");
        exit(-1);
    }
    memset(buffer, 0, sizeof(bitmap_t));
    memcpy(buffer, &free_bitmap, sizeof(free_bitmap));
    write_blocks(NUM_BLOCKS - 1, 1, buffer);
    free(buffer);
}

// writes the super block to the disk
void write_super_block()
{
    void *buffer = malloc(BLOCK_SIZE);
    if (buffer == NULL)
    {
        printf("ERROR (write_super_block): could not allocate memory for buffer.\n");
        exit(-1);
    }
    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, &super_block, sizeof(super_block));
    write_blocks(0, 1, buffer);
    free(buffer);
}

// gets a index of block containing file at specific location
int get_block(inode_t node, int loc)
{
    int pointer_index = loc / BLOCK_SIZE;

    if (pointer_index >= NUM_POINTERS)
        return get_block(inode_table[node.ind_pointer], loc - NUM_POINTERS * BLOCK_SIZE);

    return node.pointers[pointer_index];
}

// gets first unsused block index from free bitmap
int get_free_bit()
{
    for (int i = 0; i < NUM_DATA_BLOCKS; i++)
        if (test_bit(free_bitmap.bits, i) == 0)
            return i;

    return -1;
}

// gets index from root directory
int get_file_index(char *name)
{

    int name_len = strlen(name);
    if (name_len > MAX_FILENAME - 1)
        name_len = MAX_FILENAME - 1;
    for (int i = 0; i < NUM_FILES; i++)
    {
        if (super_block.root[i].name[0] != '\0')
        {
            //printf("get_file_index(%s): super_block.root[i].name = %s\n", super_block.root[i].name);
            if (strncmp(name, super_block.root[i].name, name_len) == 0)
                return i;
        }
    }

    return -1;
}

// gets first unsused block index from free bitmap
int get_unused_block()
{
    for (int i = 0; i < NUM_DATA_BLOCKS; i++)
        if (test_bit(free_bitmap.bits, i) == 0)
            return i;

    return -1;
}

// gets an unused inode
int get_unused_inode()
{
    for (int i = 0; i < NUM_INODES; i++)
        if (inode_table[i].size == -1)
            return i;

    return -1;
}

int get_unused_fd()
{
    for (int i = 0; i < MAX_FD_ENTRY; i++)
        if (file_descriptors[i].inode == -1)
            return i;

    return -1;
}

int get_unused_dir_slot()
{
    for (int i = 0; i < NUM_FILES; i++)
        if (super_block.root[i].inode == -1)
            return i;

    return -1;
}

// gets the index of the inode pointing to the file's data at a specific location
int get_file_inode_index(int node_index, int loc)
{
    int pointer_index = loc / BLOCK_SIZE;

    if (pointer_index >= NUM_POINTERS)
    {
        if (inode_table[node_index].ind_pointer == -1)
        {
            printf("ERROR (get_file_inode_index): recursion led to unitialized indirect pointer.\n");
            return -1;
        }
        else
            return get_file_inode_index(inode_table[node_index].ind_pointer, loc - NUM_POINTERS * BLOCK_SIZE);
    }
    else
        return node_index;
}

// either initializes a new disk or loads an existing one depending on fresh
void mkssfs(int fresh)
{

    // initialize the disk
    if (fresh == 1)
    {

        if (init_fresh_disk(NAME, BLOCK_SIZE, NUM_BLOCKS) != 0)
        {
            printf("Error (mkssfs): could not create disk.\n");
            exit(-1);
        }

        // initialize and write the super block
        initialize_super_block();
        write_super_block();
        // initialzie and write the inode table
        initialize_inode_table();
        write_inode_table();
        // initialize and write the free bitmap
        initialize_free_bitmap();
        write_free_bitmap();
        // initialize the file descriptor table
        initialize_fd_table();
    }
    // get existing disk
    else
    {
        if (init_disk(NAME, BLOCK_SIZE, NUM_BLOCKS) != 0)
        {
            printf("Error (mkssfs): could not create disk.\n");
            exit(-1);
        }

        // read the super block, inode table, and free bitmap from the disk
        read_blocks(0, 1, &super_block);
        read_blocks(1, 4, &inode_table);
        read_blocks(NUM_BLOCKS - 1, 1, &free_bitmap);
        // initialize the file descriptor table
        initialize_fd_table();
    }
}

int ssfs_fopen(char *name)
{
    // no change from removing this 
    // for (int i = 0; i < MAX_FD_ENTRY; i++)
    //     if (strcmp(file_descriptors[i].name, name) == 0)
    //         return i;

    int directory_index = -1;
    int fd_index;
    int inode_index;

    // get an unsused file descriptor slot
    fd_index = get_unused_fd();
    if (fd_index == -1)
    {
        printf("ERROR (ssfs_open): already at maximum amount of file descriptors.\n");
        return -1;
    }
    // printf("fopen(): fd_index = %i\n", fd_index);
    // printf("fopen(): name = %s\n", name);

    // get the directory index of the file if it already exists
    int name_len = strlen(name);
    if (name_len > MAX_FILENAME - 1)
        name_len = MAX_FILENAME - 1;
    for (int i = 0; i < NUM_FILES; i++)
    {
        // names are not getting copied over
        if (super_block.root[i].name[0] != '\0')
        {
            // printf("fopen(): super_block.root[i].name = %s\n", super_block.root[i].name);
            if (strncmp(name, super_block.root[i].name, name_len) == 0)
                directory_index = i;
        }
    }

    // check to see if the file already exits in the directory
    if (directory_index < 0 || super_block.root[directory_index].inode == -1)
    {
        // printf("file with name '%s' does not exist, creating new file...\n", name);
        // printf("going to create a file with directory index = %i\n", directory_index);

        // get the index of an unused inode
        inode_index = get_unused_inode();
        if (inode_index < 0)
        {
            printf("ERROR (ssfs_open): could not find an empty inode.\n");
            return -1;
        }

        // get the index of an unsused directory slot
        directory_index = get_unused_dir_slot();
        if (directory_index < 0)
        {
            printf("ERROR (ssfs_open): could not find an empty directory slot.\n");
            return -1;
        }

        // update inode table, file descriptor table, super block
        int len = strlen(name);
        if (len > MAX_FILENAME - 1)
            len = MAX_FILENAME - 1;
        inode_table[inode_index].size = 0;
        file_descriptors[fd_index].inode = inode_index;
        memcpy(file_descriptors[fd_index].name, name, len);
        memcpy(&super_block.root[directory_index].name, name, len);
        super_block.root[directory_index].inode = inode_index;
        write_super_block();
        write_inode_table();
        // printf("fopen(): super_block.root[directory_index].inode = %i\n", super_block.root[directory_index].inode);
        // printf("fopen(): super_block.root[directory_index].name = %s\n", super_block.root[directory_index].name);
    }
    // if the file was already exists on the disk
    else
    {
        // printf("file with name '%s' already exists on disk, opening existing file...\n", name);
        // get inode from directory entry and initialize file descriptor
        int len = strlen(name);
        if (len > MAX_FILENAME - 1)
            len = MAX_FILENAME - 1;
        inode_index = super_block.root[directory_index].inode;
        memcpy(file_descriptors[fd_index].name, name, len);
        file_descriptors[fd_index].inode = super_block.root[directory_index].inode;
        file_descriptors[fd_index].write_pointer = inode_table[inode_index].size;
    }

    return fd_index;
}

int ssfs_fclose(int fileID)
{
    // check for invalid fileID
    if (fileID > MAX_FD_ENTRY)
    {
        printf("fclose(): fileID > MAX_FD_ENTRY\n");
        return -1;
    }
    else if (fileID < 0)
    {
        printf("fclose(): fileID < 0\n");
        return -1;
    }
    else if (file_descriptors[fileID].inode == -1)
    {
        printf("fclose(): file_descriptors[fileID].inode == -1\n");
        return -1;
    }
    else
    {
        // check for empty file...
        if (inode_table[file_descriptors[fileID].inode].size == 0)
        {
            inode_table[file_descriptors[fileID].inode].size = 0;
            write_inode_table();
        }

        // printf("fclose(): closing file #%i...\n", fileID);
        // reset the memory of the appropriate entry
        memset(&file_descriptors[fileID].name, 0, MAX_FILENAME);
        file_descriptors[fileID].inode = -1;
        file_descriptors[fileID].read_pointer = 0;
        file_descriptors[fileID].write_pointer = 0;
        return 0;
    }
}

// recursively resets a file's inode_table
void reset_file_inodes(inode_t node)
{
    if (node.ind_pointer != -1)
        reset_file_inodes(inode_table[node.ind_pointer]);

    for (int i = 0; i < NUM_POINTERS; i++)
    {
        clear_bit(free_bitmap.bits, node.pointers[i]);
        node.pointers[i] = -1;
    }
    node.size = -1;
    node.ind_pointer = -1;
    write_free_bitmap();
    write_inode_table();
}

// moves the read pointer
int ssfs_frseek(int fileID, int loc)
{

    // check for invalid value
    if (loc < 0)
    {
        printf("ERROR (frseek): loc < 0\n");
        return -1;
    }
    // removing this doesn't change anyhing
    // else if (fileID < 0)
    // {
    //     printf("ERROR (frseek): fileID < 0\n");
    //     return -1;
    // }

    // get the inode index
    int index = file_descriptors[fileID].inode;

    // check for invalid size
    if (loc > inode_table[index].size)
    {
        printf("ERROR (frseek): loc > file_descriptors[fileID].inode.size\n");
        return -1;
    }

    file_descriptors[fileID].read_pointer = loc;
    return 0;
}

// moves the write pointer
int ssfs_fwseek(int fileID, int loc)
{
    // check for invalid value
    if (loc < 0)
    {
        printf("ERROR (fwseek): loc < 0\n");
        return -1;
    }
    // doensn't change anything if removed
    // else if (fileID < 0)
    // {
    //     printf("ERORR (fwseek): fileID < 0\n");
    //     return -1;
    // }

    // get the inode index
    int index = file_descriptors[fileID].inode;

    // check for invalid size
    if (loc > inode_table[index].size)
    {
        printf("ERROR (fwseek): loc > inode_table[index].size\n");
        return -1;
    }

    file_descriptors[fileID].write_pointer = loc;
    return 0;
}

// reads length bytes from fileID into buf
// returns number of bytes read
int ssfs_fread(int fileID, char *buf, int length)
{
    int remaining = length;
    int read_amount = 0;
    int buf_pointer = 0;
    int copy_amount;
    void *buffer = malloc(BLOCK_SIZE);
    int inode_index = file_descriptors[fileID].inode;
    int location = file_descriptors[fileID].read_pointer % BLOCK_SIZE;
    int block_index;

    // check for invalid length or fileID
    if (length < 0 || fileID < 0 || fileID >= NUM_FILES)
    {
        free(buffer);
        return -1;
    }
    else if (length == 0)
    {
        free(buffer);
        return 0;
    }

    // check for a closed file
    if (inode_index == -1)
    {
        printf("ERROR: (ssfs_read): fd entry was unitialized.\n");
        free(buffer);
        return read_amount;
    }

    // check for empty inode
    if (inode_table[inode_index].size == 0)
    {
        // printf("opened file's inode lead to empty inode...\n");
        // printf("file_descriptors[fileID].inode = %i\n", file_descriptors[fileID].inode);
        free(buffer);
        return read_amount;
    }

    // initialize buffers
    memset(buffer, 0, BLOCK_SIZE);
    memset(buf, 0, length);

    // read a block from the disk
    block_index = get_block(inode_table[inode_index], file_descriptors[fileID].read_pointer);
    if (block_index == -1)
    {
        printf("ERROR (ssfs_read): inode's pointer was unitialized.\n");
        free(buffer);
        return read_amount;
    }
    read_blocks(block_index, 1, buffer);

    // copy to buf
    if (remaining > BLOCK_SIZE - location)
        copy_amount = BLOCK_SIZE - location;
    else if (remaining > strlen(buf) - 1)
        copy_amount = strlen(buf) - 1;
    else
        copy_amount = remaining;

    // debug
    // printf("fread(): fileID = %i\n", fileID);
    // printf("fread(): inode_index = %i\n", inode_index);
    // printf("fread(): location = %i\n", location);
    // printf("fread(): strlen(buf) = %zu\n", strlen(buf));
    // printf("fread(): length = %i\n", length);
    // printf("fread(): copy_amount = %i\n\n", copy_amount);

    memcpy(buf + buf_pointer, buffer + location, copy_amount);

    // update buf pointer, read pointer, remaining amount to read, and location
    buf_pointer += copy_amount;
    file_descriptors[fileID].read_pointer += copy_amount;
    if (file_descriptors[fileID].read_pointer >= inode_table[inode_index].size)
        file_descriptors[fileID].read_pointer = 0;
    remaining -= copy_amount;
    read_amount += copy_amount;

    // check to see if we've filled up the buffer
    if (copy_amount == length)
    {
        free(buffer);
        printf("fread(): returning %i", copy_amount);
        return copy_amount;
    }

    while (remaining > 0)
    {
        // clear the buffer
        memset(buffer, 0, BLOCK_SIZE);

        // read a block from the disk
        block_index = get_block(inode_table[inode_index], file_descriptors[fileID].read_pointer);
        if (block_index == -1)
        {
            printf("ERROR (ssfs_read): inode's pointer was unitialized.\n");
            free(buffer);
            return read_amount;
        }
        read_blocks(block_index, 1, buffer);

        // copy the read block to buf
        if (remaining > BLOCK_SIZE)
            copy_amount = BLOCK_SIZE;
        else
            copy_amount = remaining;
        memcpy(buf + buf_pointer, buffer, copy_amount);

        // increment values
        buf_pointer += copy_amount;
        file_descriptors[fileID].read_pointer += copy_amount;
        if (file_descriptors[fileID].read_pointer >= inode_table[inode_index].size)
            file_descriptors[fileID].read_pointer = 0;
        remaining -= copy_amount;
        read_amount += copy_amount;
    }

    free(buffer);
    return read_amount;
}

// writes length bytes to fileID from buf
// returns number of bytes written
int ssfs_fwrite(int fileID, char *buf, int length)
{

    int remaining = length;
    int buf_pointer = 0;
    int copy_amount = 0;
    int ind_inode_index;
    int location = file_descriptors[fileID].write_pointer % BLOCK_SIZE;
    int pointer_index = file_descriptors[fileID].write_pointer / BLOCK_SIZE;
    int direct_inode_index = file_descriptors[fileID].inode;
    int inode_index = direct_inode_index;
    int block_index = get_block(inode_table[direct_inode_index], file_descriptors[fileID].write_pointer);
    // create a buffer
    void *buffer = malloc(BLOCK_SIZE);
    memset(buffer, 0, BLOCK_SIZE);

    // check for invalid length or fileID
    if (length < 0 || fileID < 0 || fileID >= NUM_FILES)
    {
        free(buffer);
        return -1;
    }
    else if (length == 0)
    {
        free(buffer);
        return 0;
    }

    if (buffer == NULL)
    {
        printf("Error (ssfs_fwrite): Could not allocate memory for buffer.\n");
        free(buffer);
        return -1;
    }

    // check to see if the block pointed by the write pointer is already initialized
    if (block_index != -1)
    {
        read_blocks(inode_table[inode_index].pointers[pointer_index], 1, buffer);

        // get the amount of data to copy
        if (remaining > BLOCK_SIZE - location)
            copy_amount = BLOCK_SIZE - location;
        else
            copy_amount = remaining;

        // write to disk
        memcpy(buffer + location, buf + buf_pointer, copy_amount);
        write_blocks(inode_table[inode_index].pointers[pointer_index], 1, buffer);

        // update values
        buf_pointer = buf_pointer + copy_amount;
        remaining = remaining - copy_amount;
        file_descriptors[fileID].write_pointer = file_descriptors[fileID].write_pointer + copy_amount;
        inode_table[direct_inode_index].size += copy_amount;
        write_inode_table();

        // update the pointer index
        if (pointer_index < NUM_POINTERS - 1)
        {
            pointer_index++;
            ind_inode_index = -1;
        }
        else
        {
            // create an indirect pointer
            ind_inode_index = get_unused_inode();
            if (ind_inode_index < 0)
            {
                printf("Could not find an empty inode for new indirect pointer.\n");
                free(buffer);
                return -1;
            }
            inode_table[inode_index].ind_pointer = ind_inode_index;
            inode_index = ind_inode_index;
            inode_table[inode_index].size = 0;
            pointer_index = 0;
            write_inode_table();
        }
    }

    // if there is still data remianing to be stored
    while (remaining > 0)
    {
        // clear the buffer
        memset(buffer, 0, BLOCK_SIZE);

        // get the amount to copy
        if (remaining > BLOCK_SIZE)
            copy_amount = BLOCK_SIZE;
        else
            copy_amount = remaining;

        // copy the appropriate amount to the buffer
        memcpy(buffer, buf + buf_pointer, copy_amount);
        remaining = remaining - copy_amount;
        buf_pointer = buf_pointer + copy_amount;

        // update the block index
        inode_index = get_file_inode_index(direct_inode_index, file_descriptors[fileID].write_pointer);
        block_index = get_block(inode_table[direct_inode_index], file_descriptors[fileID].write_pointer);
        if (block_index == -1)
        {
            block_index = get_unused_block();
            if (block_index == -1)
            {
                printf("Could not find an empty block.\n");
                free(buffer);
                return -1;
            }
            set_bit(free_bitmap.bits, block_index);
            inode_table[inode_index].pointers[pointer_index] = block_index;
            write_inode_table();
            write_free_bitmap();
        }

        // write to that block
        write_blocks(block_index, 1, buffer);

        // update buf pointer, remaining amount to copy, write pointer, inode size
        buf_pointer = buf_pointer + copy_amount;
        remaining = remaining - copy_amount;
        file_descriptors[fileID].write_pointer = file_descriptors[fileID].write_pointer + copy_amount;
        inode_table[direct_inode_index].size += copy_amount;
        write_inode_table();

        if (pointer_index < NUM_POINTERS - 1)
        {
            pointer_index++;
            ind_inode_index = -1;
        }
        else
        {
            // create an indirect pointer
            ind_inode_index = get_unused_inode();
            if (ind_inode_index < 0)
            {
                printf("Could not find an empty inode for new indirect pointer.\n");
                free(buffer);
                return -1;
            }
            inode_table[inode_index].ind_pointer = ind_inode_index;
            inode_table[ind_inode_index].size = 0;
            pointer_index = 0;
            write_inode_table();
        }
    }

    // free memory and return
    free(buffer);
    return length;
}

int ssfs_remove(char *file)
{

    // printf("remove(): file name (argument) = %s\n", file);

    // check length of argument
    int len = strlen(file);
    if (len > MAX_FILENAME - 1)
        len = MAX_FILENAME - 1;

    // go through all files in root directory
    for (int i = 0; i < NUM_FILES; i++)
    {
        // check for empty slot
        if (super_block.root[i].inode != -1)
        {
            // printf("remove(): super_block.root[i].name = %s\n", super_block.root[i].name);

            // compare name of file in directory to argument
            if (strncmp(super_block.root[i].name, file, len) == 0)
            {
                // printf("remove(): closing file #%i...\n", i);

                // reset the inodes and directory entry
                reset_file_inodes(inode_table[super_block.root[i].inode]);
                super_block.root[i].inode = -1;
                memset(super_block.root[i].name, 0, MAX_FILENAME);

                // reset the file descriptor
                for (int j = 0; j < MAX_FD_ENTRY; j++)
                {
                    // check for empty slot
                    if (file_descriptors[j].inode != -1)
                    {
                        // check if arugments matches file descriptor name
                        if (strncmp(file_descriptors[j].name, file, len) == 0)
                        {
                            memset(file_descriptors[j].name, 0, MAX_FILENAME);
                            file_descriptors[j].inode = -1;
                            file_descriptors[j].write_pointer = 0;
                            file_descriptors[j].read_pointer = 0;
                        }
                    }
                }
                write_super_block();
                return 0;
            }
        }
    }
    printf("ERROR (remove): couldn't find file %s\n", file);
    return -1;
}
