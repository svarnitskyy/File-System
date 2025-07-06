/*
    implementation of API
*/

#include "def.h"

pthread_mutex_t mutex_for_fs_stat;

// initialize file system - should be called as the first thing before accessing this file system
int RSFS_init()
{

    // initialize data blocks
    for (int i = 0; i < NUM_DBLOCKS; i++)
    {
        void *block = malloc(BLOCK_SIZE); // a data block is allocated from memory
        if (block == NULL)
        {
            printf("[init] fails to init data_blocks\n");
            return -1;
        }
        data_blocks[i] = block;
    }

    // initialize bitmaps
    for (int i = 0; i < NUM_DBLOCKS; i++)
        data_bitmap[i] = 0;
    pthread_mutex_init(&data_bitmap_mutex, NULL);
    for (int i = 0; i < NUM_INODES; i++)
        inode_bitmap[i] = 0;
    pthread_mutex_init(&inode_bitmap_mutex, NULL);

    // initialize inodes
    for (int i = 0; i < NUM_INODES; i++)
    {
        inodes[i].length = 0;
        for (int j = 0; j < NUM_POINTER; j++)
            inodes[i].block[j] = -1; // pointer value -1 means the pointer is not used
        inodes[i].num_current_reader = 0;
        pthread_mutex_init(&inodes[i].rw_mutex, NULL);
        pthread_mutex_init(&inodes[i].read_mutex, NULL);
    }
    pthread_mutex_init(&inodes_mutex, NULL);

    // initialize open file table
    for (int i = 0; i < NUM_OPEN_FILE; i++)
    {
        struct open_file_entry entry = open_file_table[i];
        entry.used = 0; // each entry is not used initially
        pthread_mutex_init(&entry.entry_mutex, NULL);
        entry.position = 0;
        entry.access_flag = -1;
    }
    pthread_mutex_init(&open_file_table_mutex, NULL);

    // initialize root directory
    root_dir.head = root_dir.tail = NULL;

    // initialize mutex_for_fs_stat
    pthread_mutex_init(&mutex_for_fs_stat, NULL);

    // return 0 means success
    return 0;
}

// create file
// if file does not exist, create the file and return 0;
// if file_name already exists, return -1;
// otherwise, return -2.
int RSFS_create(char *file_name)
{

    // search root_dir for dir_entry matching provided file_name
    struct dir_entry *dir_entry = search_dir(file_name);

    if (dir_entry)
    { // already exists
        printf("[create] file (%s) already exists.\n", file_name);
        return -1;
    }
    else
    {

        if (DEBUG)
            printf("[create] file (%s) does not exist.\n", file_name);

        // construct and insert a new dir_entry with given file_name
        dir_entry = insert_dir(file_name);
        if (DEBUG)
            printf("[create] insert a dir_entry with file_name:%s.\n", dir_entry->name);

        // access inode-bitmap to get a free inode
        int inode_number = allocate_inode();
        if (inode_number < 0)
        {
            printf("[create] fail to allocate an inode.\n");
            return -2;
        }
        if (DEBUG)
            printf("[create] allocate inode with number:%d.\n", inode_number);

        // save inode-number to dir-entry
        dir_entry->inode_number = inode_number;

        return 0;
    }
}

// open a file with RSFS_RDONLY or RSFS_RDWR flags
// When flag=RSFS_RDONLY:
//   if the file is currently opened with RSFS_RDWR (by a process/thread)=> the caller should be blocked (wait);
//   otherwise, the file is opened and the descriptor (i.e., index of the open_file_entry in the open_file_table) is returned
// When flag=RSFS_RDWR:
//   if the file is currently opened with RSFS_RDWR (by a process/thread) or RSFS_RDONLY (by one or multiple processes/threads)
//       => the caller should be blocked (i.e. wait);
//   otherwise, the file is opened and the desrcriptor is returned
int RSFS_open(char *file_name, int access_flag)
{

    // check to make sure access_flag is either RSFS_RDONLY or RSFS_RDWR
    if (access_flag == RSFS_RDONLY || access_flag == RSFS_RDWR)
    {

        pthread_mutex_lock(&open_file_table_mutex);
        // find dir_entry matching file_name
        struct dir_entry *dir_entry = search_dir(file_name);
        // find the corresponding inode
        struct inode *inode_entry = &inodes[dir_entry->inode_number];
        struct open_file_entry currentFile;
        for (int i = 0; i < NUM_OPEN_FILE; i++)
        {
            if (open_file_table[i].dir_entry == dir_entry)
            {
                currentFile = open_file_table[i];
            }
        }

        pthread_mutex_unlock(&open_file_table_mutex);
        // base on the requested access_flag and the current "open" status of this file to block the caller if needed
        if (access_flag == RSFS_RDONLY)
        { // read only
            if (currentFile.access_flag == RSFS_RDWR)
            {
                pthread_mutex_lock(&inode_entry->read_mutex);
                inode_entry->num_current_reader++;
                if(inode_entry->num_current_reader == 1){
                    pthread_mutex_lock(&inode_entry->rw_mutex);
                }
                pthread_mutex_unlock(&inode_entry->read_mutex);
                // pthread_mutex_lock(&inode_entry->rw_mutex);
            }
            // find an unused open-file-entry in open-file-table and fill the fields of the entry properly
            int fd = allocate_open_file_entry(access_flag, dir_entry);
            // return the index of the open-file-entry in open-file-table as file descriptor
            return fd;
        }
        else
        { // read and write
            if (currentFile.access_flag == RSFS_RDWR || currentFile.access_flag == RSFS_RDONLY)
            {
                pthread_mutex_lock(&inode_entry->rw_mutex);
            }
            // find an unused open-file-entry in open-file-table and fill the fields of the entry properly
            int fd = allocate_open_file_entry(access_flag, dir_entry);
            // return the index of the open-file-entry in open-file-table as file descriptor
            return fd;
        }
    }
    return -1;
}

// append the content in buf to the end of the file of descriptor fd
int RSFS_append(int fd, void *buf, int size)
{
    // check the sanity of the arguments: fd should be in [0,NUM_OPEN_FILE] and size>0.
    if (fd >= 0 && fd <= NUM_OPEN_FILE && size > 0)
    {
        // get the open file entry corresponding to fd
        pthread_mutex_lock(&open_file_table_mutex);
        struct open_file_entry *fileEntry = &open_file_table[fd];

        // check if the file is opened with RSFS_RDWR mode; otherwise return -1
        if (fileEntry->access_flag = RSFS_RDWR)
        {
            // save how total size before writing
            int temp = size;

            // get the current position
            int currentPosition = fileEntry->position;

            // get the corresponding directory entry
            struct dir_entry *dir_entry = fileEntry->dir_entry;

            // get the corresponding inode
            struct inode *inode = &inodes[dir_entry->inode_number];

            // append the content in buf to the data blocks of the file from the end of the file
            while (size > 0)
            {
                // calculate the index of the data block and the offset within the block
                int blockIndex = currentPosition / BLOCK_SIZE;
                int offset = currentPosition % BLOCK_SIZE;

                // check if a new block needs to be allocated
                if (inode->block[blockIndex] == -1)
                {
                    int block_number = allocate_data_block();
                    if (block_number == -1)
                    {
                        return -1; // failed to allocate a new data block
                    }
                    inode->block[blockIndex] = block_number;
                }

                // calculate the number of bytes to write to the current block
                int bytesToWrite;
                if (size < BLOCK_SIZE - offset)
                {
                    bytesToWrite = size;
                }
                else
                {
                    bytesToWrite = BLOCK_SIZE - offset;
                }
                // copy the content from the buffer to the current data block
                memcpy(data_blocks[inode->block[blockIndex]] + offset, buf, bytesToWrite);

                // update current position, buffer pointer, and remaining size
                currentPosition += bytesToWrite;
                buf += bytesToWrite;
                size -= bytesToWrite;
            }

            // update inode length
            inode->length = currentPosition;

            // update the current position in the open file entry
            fileEntry->position = currentPosition;

            pthread_mutex_unlock(&open_file_table_mutex);
            // return the number of bytes appended to the file
            return temp;
        }
        else
        {
            return -1;
        }
    }
}

// update current position of the file (which is in the open_file_entry) to offset
int RSFS_fseek(int fd, int offset)
{

    // check sanity test of fd
    if (fd < 0 || fd >= NUM_OPEN_FILE)
    {
        return -1;
    }

    // get the correspondng open file entry
    pthread_mutex_lock(&open_file_table_mutex);
    struct open_file_entry *file = &open_file_table[fd];

    // get the current position

    int position = file->position;

    // get the corresponding dir entry

    struct dir_entry *dir_entry = file->dir_entry;

    // get the corresponding inode and file length

    struct inode *inode = &inodes[dir_entry->inode_number];

    int fileLength = inode->length;

    // check if argument offset is not within 0...length, do not proceed and return current position

    if (offset < 0 || offset > fileLength)
    {
        return file->position;
    }

    // update the current position to offset, and return the new current position

    file->position = offset;

    // return the current poisiton
    pthread_mutex_unlock(&open_file_table_mutex);
    return offset;
}

// read from file from the current position for up to size bytes
int RSFS_read(int fd, void *buf, int size)
{

    // sanity test of fd and size

    if (fd >= 0 && fd <= NUM_OPEN_FILE && size > 0)
    {
        pthread_mutex_lock(&open_file_table_mutex);

        // get the corresponding open file entry
        struct open_file_entry *fileEntry = &open_file_table[fd];

        // get the current position
        int currentPosition = fileEntry->position;
        int initialPosition = fileEntry->position;

        // get the corresponding directory entry
        struct dir_entry *dir_entry = fileEntry->dir_entry;

        // get the corresponding inode
        struct inode *inode = &inodes[dir_entry->inode_number];

        // calculate the remaining bytes to read
        int bytesRemaining = inode->length - currentPosition;
        if (bytesRemaining <= 0)
        {
            return 0; 
        }

        // check to make sure no reading past file size
        if (size > bytesRemaining)
        {
            size = bytesRemaining;
        }

        // read the content of the file from current position for up to size bytes
        for (int i = 0; i < size; i++)
        {
            // calculate the index of the data block and the offset within the block
            int blockIndex = currentPosition / BLOCK_SIZE;
            int offset = currentPosition % BLOCK_SIZE;

            // retrieve the data block corresponding to the current position
            int blockNumber = inode->block[blockIndex];

            // check if the block is allocated and within the file length
            if (blockNumber == -1 || blockIndex >= NUM_POINTER)
            {
                break; // end of file reached
            }

            // copy data from the block to the buffer
            memcpy((char *)buf + i, data_blocks[blockNumber] + offset, 1);

            // move to the next position
            currentPosition++;
        }

        // update the current position in open file entry
        fileEntry->position = currentPosition;

        pthread_mutex_unlock(&open_file_table_mutex);

        // return the actual number of bytes read
        return currentPosition - initialPosition;
    }
    else
    {
        return -1;
    }
}

// close file: return 0 if succeed
int RSFS_close(int fd)
{

    // sanity test of fd and whence
    if (fd >= 0 && fd <= NUM_OPEN_FILE)
    {
        // get the corresponding open file entry
        struct open_file_entry fileEntry = open_file_table[fd];
        // get the corresponding dir entry
        struct dir_entry *dir = fileEntry.dir_entry;
        // get the corresponding inode
        struct inode *inode = &inodes[dir->inode_number];
        // depending on the way that the file was open (RSFS_RDONLY or RSFS_RDWR), update the corresponding mutex and/or count
        if (fileEntry.access_flag == RSFS_RDONLY)
        {
            // release this open file entry in the open file table
            pthread_mutex_lock(&inode->read_mutex);
            inode->num_current_reader--;
            if(inode->num_current_reader == 0){
                pthread_mutex_unlock(&inode->rw_mutex);
            }
            pthread_mutex_unlock(&inode->read_mutex);
        }
        else
        {
            // release this open file entry in the open file table
            pthread_mutex_unlock(&inode->rw_mutex);
        }
        free_open_file_entry(fd);
        return 0;
    }

    return -1;
}

// delete file
int RSFS_delete(char *file_name)
{

    // find the corresponding dir_entry
    struct dir_entry *dir_entry = search_dir(file_name);
    if (dir_entry == NULL)
    {
        // directory entry not found
        return -1;
    }

    //find the corresponding inode

    int inode_number = dir_entry->inode_number;
    struct inode *inode = &inodes[inode_number];
    //free the inode in inode-bitmap
    free_inode(inode_number);

    return delete_dir(file_name);
}

// print status of the file system
void RSFS_stat()
{

    pthread_mutex_lock(&mutex_for_fs_stat);

    printf("\nCurrent status of the file system:\n\n %16s%10s%10s\n", "File Name", "Length", "iNode #");

    // list files
    struct dir_entry *dir_entry = root_dir.head;
    while (dir_entry != NULL)
    {

        int inode_number = dir_entry->inode_number;
        struct inode *inode = &inodes[inode_number];

        printf("%16s%10d%10d\n", dir_entry->name, inode->length, inode_number);
        dir_entry = dir_entry->next;
    }

    // data blocks
    int db_used = 0;
    for (int i = 0; i < NUM_DBLOCKS; i++)
        db_used += data_bitmap[i];
    printf("\nTotal Data Blocks: %4d,  Used: %d,  Unused: %d\n", NUM_DBLOCKS, db_used, NUM_DBLOCKS - db_used);

    // inodes
    int inodes_used = 0;
    for (int i = 0; i < NUM_INODES; i++)
        inodes_used += inode_bitmap[i];
    printf("Total iNode Blocks: %3d,  Used: %d,  Unused: %d\n", NUM_INODES, inodes_used, NUM_INODES - inodes_used);

    // open files
    int of_num = 0;
    for (int i = 0; i < NUM_OPEN_FILE; i++)
        of_num += open_file_table[i].used;
    printf("Total Opened Files: %3d\n\n", of_num);

    pthread_mutex_unlock(&mutex_for_fs_stat);
}

// write the content of size (bytes) in buf to the file (of descripter fd) from current position for up to size bytes
int RSFS_write(int fd, void *buf, int size)
{
    return 0; // placeholder
}

// cut the content from the current position for up to size (bytes) from the file of descriptor fd
int RSFS_cut(int fd, int size)
{
    return 0; // placeholder
}
