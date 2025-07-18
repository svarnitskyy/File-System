## File System

An in-memory file system developed on top of Linux, implementing core file system API functions like open, append, read, seek, close, and delete.

## Project Overview
This project involves developing a highly simplified in-memory file system, built on Linux. The project focuses on implementing key file system API functions, with major data structures and low-level operations provided.

## Features
* **Basic API Functions:**
    * `RSFS_open()`: Opens a file with specified access flags (read-only or read-write).
    * `RSFS_append()`: Appends data to the end of a file.
    * `RSFS_read()`: Reads data from a file starting at its current position.
    * `RSFS_fseek()`: Changes the current position within a file.
    * `RSFS_close()`: Closes an open file.
    * `RSFS_delete()`: Deletes a file.
* **Provided Functions:**
    * `RSFS_init()`: Initializes the file system.
    * `RSFS_create()`: Creates an empty file with the given name.
    * `RSFS_stat()`: Displays the current state of the file system (for debugging).
* **In-Memory Data Structures:**
    * **Data Blocks:** Allocated from main memory (heap) and its size is specified by constant `BLOCK_SIZE` (32 bytes by default). The pointers to all the blocks are recorded in the `void *data_blocks [NUM_DBLOCKS]` array. Managed by `int data_bitmap[NUM_DBLOCKS]`.
    * **Inodes:** Defined as "struct inode", with fields including `int block[NUM_POINTER]` (array of block-numbers), `int length` (length of file in bytes), `int num_current_reader`, `pthread_mutex_t rw_mutex`, and `pthread_mutex_t read_mutex`. Managed by `struct inode inodes[NUM_INODES]` and `inode_bitmap[NUM_INODES]`.
    * **Directory Entries:** `struct dir_entry` records file name and inode number. The root directory (`struct root_dir`) is organized as a linked list of directory entries.
    * **Open File Table:** An array of `struct open_file_entry`, each tracking `int used` status, `struct dir_entry *dir_entry` pointer, `int access_flag`, `int position`, and `pthread_mutex_t entry_mutex`.

## How It Works
The RSFS operates entirely in main memory for simplicity.
* **File System Initialization:** `RSFS_init()` initializes the data blocks, the bitmaps for data blocks and inodes, the open file table, and the root directory.
* **File Creation:** `RSFS_create()` searches the root directory for a matching file name; if such an entry exists, it returns -1. Otherwise, it calls `insert_dir()` to construct and insert a new directory entry, and `allocate_inode()` to get a free and initialized inode, recording the inode number to the directory entry.
* **File Opening:** `RSFS_open()` handles concurrency using reader/writer locks:
    * If the file was already opened with flag `RSFS_RDWR` by a writer, the caller is blocked until the file is closed by the writer.
    * If the file was already opened with flag `RSFS_RDONLY` by one/multiple readers: if the current caller wants to open with `RSFS_RDONLY`, the file can be opened; if the current caller wants to open with `RSFS_RDWR`, the caller is blocked until the file is closed by all readers.
    * An unused open file entry is found, initialized, and its index returned as the file descriptor (fd).
* **File Operations (Append, Read):** These functions manipulate the file's data blocks and update its length based on the current position tracked in the open file entry.
* **File Seeking:** `RSFS_fseek()` changes the `position` of the file with file descriptor `fd` to `offset`, but only if the offset is within the range of the file.
* **File Closing:** `RSFS_close()` checks the sanity of arguments and frees the open file entry.
* **File Deletion:** `RSFS_delete()` frees the data blocks, inode, and directory entry of the file.

## Provided Code Package Contents
* `Makefile`: For compiling (`make`) and cleaning (`make clean`).
* `def.h`: Definitions of global constants, main data structures, and API functions.
* `dir.c`: Root directory declaration and low-level directory entry operations (search, insert, delete).
* `inode.c`: Inode declarations, inode bitmap, and low-level inode allocation/freeing.
* `open_file_table.c`: Open file table declaration and low-level open file entry allocation/freeing.
* `data_block.c`: Data block declarations, data bitmap, and low-level data block allocation/freeing.
* `api.c`: File for implementing the API functions.
* `application.c`: Sample application (testing) code, where more tests can be added.
* `sample_output.txt`: Sample outputs from the provided application code.

## Hardware Used
This project is developed on top of Linux and is primarily a software implementation, so no specific hardware other than a Linux-like system is directly used or required for compilation and execution.
