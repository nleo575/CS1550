/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.

	Modified by: Nicolas Leo
	University of Pittsburgh CS 1550
	Project 4
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) \
+ (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	
										//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * 
		sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1)\
				 + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	
										//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * 
		sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

struct cs1550_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

#define DISK_SIZE 5 // 5MB
#define TOTAL_BLOCKS ((DISK_SIZE<<20)/BLOCK_SIZE) // 10,240 blocks
// 1280 chars to represent 10,240 blocks
#define MAX_BITMAP_ENTRIES (TOTAL_BLOCKS>>3)
#define BLOCKS_FOR_BITMAP (MAX_BITMAP_ENTRIES/BLOCK_SIZE + \
		((MAX_BITMAP_ENTRIES%BLOCK_SIZE) > 0 ? 1:0))
unsigned char bitmap[MAX_BITMAP_ENTRIES];
char directory[MAX_FILENAME+1];
char filename[MAX_FILENAME+1];
char extension[MAX_EXTENSION+1];
cs1550_root_directory root;
cs1550_directory_entry de;
FILE *disk;
int num_dir = 0;
int fileindex = 0;
int directory_num = 0;

/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not. 
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
   
	//is path the root dir?
	if (strcmp(path, "/") == 0) 
	{
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	} 

	// Zero out the strings
	directory[0] = '\0'; filename[0] = '\0';extension[0] = '\0';

	// Parse the path into usable strings
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	// Verify that directory is of the correct length
	int len;
	if((len = strlen(directory)) == 0 || len > MAX_FILENAME) 
		return -ENOENT;

	disk = fopen(".disk", "rb");
	fread(&root, 1, BLOCK_SIZE, disk);
	num_dir = root.nDirectories;

	// Return no entry if no directories made yet
	if(num_dir == 0) return -ENOENT; 

	//Check if name is subdirectory
	if(strlen(filename) == 0 && strlen(extension) == 0)
	{
		int i,	// Loop counter
			max = num_dir;
		for(i = 0; i < max; i++)
		{ 
			if(strcmp(root.directories[i].dname, directory) == 0)
			{
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				fclose(disk);
				return 0;//no error
			}
		}
	}
	//Check if name is a regular file
	else if(strlen(filename)  <= MAX_FILENAME && 
			strlen(extension) <= MAX_EXTENSION)
	{
		long nStartBlock = -1;
		int i,			// Loop counter
			max = num_dir;
		for(i = 0; i < max; i++)
			if(strcmp(root.directories[i].dname, directory) == 0)
			{
				nStartBlock = root.directories[i].nStartBlock;
				// for write()
				directory_num = i;
				break;
			}

		if(nStartBlock > -1)
		{
			fseek(disk, BLOCK_SIZE*nStartBlock, SEEK_SET);
			fread(&de, 1, BLOCK_SIZE, disk);
			max = de.nFiles;
			for(i = 0; i < max; i++)
			{ 
				if(strcmp(de.files[i].fname, filename) == 0 && 
				   strcmp(de.files[i].fext, extension) == 0)
				{
					//regular file, probably want to be read & write
					stbuf->st_mode = S_IFREG | 0666; 
					stbuf->st_nlink = 1; //file links
					stbuf->st_size = de.files[i].fsize;
					// for write()
					fileindex = i;
					fclose(disk);
					return 0; // no error						
				}
			}
		}
	}

	fclose(disk);
	return -ENOENT; //Else return that path doesn't exist
}

/* 
 * Called whenever the contents of a directory are desired. Could be from an 
 * 'ls' or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	//the filler function allows us to add entries to the listing
	//read the fuse.h file for a description (in the ../include dir)
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	// Zero out the string
	directory[0] = '\0'; 
	char subdirectory[24]; subdirectory[0]='\0';

	// Parse the path into usable strings
	sscanf(path, "/%[^/]/%s", directory, subdirectory);

	// Verify that directory length is > 0 && < 9 && no subdirectory
	if(strlen(subdirectory)>0 || 
		strlen(directory) > MAX_FILENAME) return -ENOENT;

	// Check reading the root directory
	if (strcmp(path, "/") == 0)
	{
		disk = fopen(".disk", "rb");
		fread(&root, 1, BLOCK_SIZE, disk);
		num_dir = root.nDirectories;
		if(num_dir == 0)
			{ fclose(disk); return 0; }// Immediately return if root is empty

		// Print out all directories
		int i,	// Loop counter
			max = num_dir;		
		for(i = 0; i < max; i++)
			filler(buf, root.directories[i].dname, NULL, 0);

		fclose(disk);
		return 0;
	}

	disk = fopen(".disk", "rb");
	fread(&root, 1, BLOCK_SIZE, disk);

	// Check if the directory exists
	int max = root.nDirectories;
	int i;
	for(i = 0; i < max; i++)
	{ 
		if(strcmp(root.directories[i].dname, directory) == 0)
		{
			cs1550_directory_entry dir;
			fseek(disk, (i*BLOCK_SIZE), SEEK_CUR);
			fread(&dir, 1, BLOCK_SIZE, disk);
			char fullname[14]; fullname[0]='\0';
			max = dir.nFiles;
			for(i = 0; i < max; i++)
			{
				strcpy(fullname,dir.files[i].fname);
				strcat(fullname, ".");
				strcat(fullname, dir.files[i].fext);
				filler(buf, fullname, NULL, 0);
				fullname[0]='\0';
			}

			fclose(disk);
			return 0;
		}
	}

	fclose(disk);
	return -ENOENT; // Directory doesn't exist
}

/* 
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;

	if (strcmp(path, "/") == 0) return -EEXIST; //is path the root dir?

	// Zero out the string
	directory[0] = '\0'; 
	char subdirectory[24]; subdirectory[0]='\0';

	// Parse the path into usable strings
	sscanf(path, "/%[^/]/%s", directory, subdirectory);
	// Verify that directory length is > 0
	if(directory[0] == '\0' || strlen(subdirectory)>0) return -EPERM;
	
	// Verify that the directory isn't actually a file
	int len = strlen(directory);
	int i;
	for(i = 0; i< len; i++) if(directory[i] == '.') return -EPERM;

	// Verify that directory length < max file name (8 chars)
	if (strlen(directory) > MAX_FILENAME) return -ENAMETOOLONG;

	
	disk = fopen(".disk", "r+b");
	fread(&root, 1, BLOCK_SIZE, disk);
	num_dir = root.nDirectories;
	// If creating the first directory
	if(num_dir == 0)
	{
		fseek(disk, -(BLOCK_SIZE*BLOCKS_FOR_BITMAP), SEEK_END);
		fread(bitmap, 0, MAX_BITMAP_ENTRIES, disk);
		bitmap[0] = 0xc0; // Mark first 2 block as in use
		bitmap[MAX_BITMAP_ENTRIES - 1] = (unsigned char) 0xff >> 
											(8- BLOCKS_FOR_BITMAP);

		root.nDirectories = 1;
		strcpy(root.directories[0].dname, directory);
		root.directories[0].nStartBlock = 1;
		cs1550_directory_entry dir;
		dir.nFiles = 0;

		// Write root, directory, and bitmap to .disk
		fseek(disk, 0L, SEEK_SET);
		fwrite(&root, 1, BLOCK_SIZE, disk);		
		fwrite(&dir,  1, BLOCK_SIZE, disk);
		fseek(disk, -(BLOCK_SIZE*BLOCKS_FOR_BITMAP), SEEK_END);
		fwrite(bitmap, 1, MAX_BITMAP_ENTRIES, disk);
		fclose(disk);
		return 0;
	}

	//Check if name is subdirectory
	int max = num_dir;
	for(i = 0; i < max; i++)
	{ 
		if(strcmp(root.directories[i].dname, directory) == 0)
		{
			fclose(disk);
			return -EEXIST;//Directory already exists
		}
	}

	// Check if max directories already made
	if(max == MAX_DIRS_IN_ROOT){fclose(disk); return -EPERM;}

	// Verified that directory doesn't exist, now check for next available 
	// block. Keep directories right after the root followed by the files
	max = root.nDirectories = root.nDirectories++;
	strcpy(root.directories[max].dname, directory);
	root.directories[max].nStartBlock = max;

	cs1550_directory_entry dir;
	dir.nFiles = 0;

	// Write the root & new directory
	fseek(disk, 0L, SEEK_SET);
	fwrite(&root, 1, BLOCK_SIZE, disk);
	fseek(disk,(max - 1)*BLOCK_SIZE, SEEK_CUR);
	fwrite(&dir, 1, BLOCK_SIZE, disk);

	// Get & then update the bitmap
	fseek(disk, -(BLOCK_SIZE*BLOCKS_FOR_BITMAP), SEEK_END);
	fread(bitmap, 1, MAX_BITMAP_ENTRIES, disk);

	// Calculate the offset & bitwise OR the new bit in
	bitmap[max/8] = (0xc0 >> (max%8)) | bitmap[max/8];

	// Update the bitmap on .disk the close the .disk
	fseek(disk, -(BLOCK_SIZE*BLOCKS_FOR_BITMAP), SEEK_END);
	fwrite(bitmap, 1, MAX_BITMAP_ENTRIES, disk);
	fclose(disk);

	return 0;
}

/* 
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/* 
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) mode;
	(void) dev;



	// Zero out the strings
	directory[0] = '\0'; filename[0] = '\0';extension[0] = '\0';

	// Parse the path into usable strings
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	// Verify that file is in 8.3 format
	int len = 0;
	// Trying to add a file to root
	if((len= strlen(filename)) == 0) return -EPERM; 

	// Error file name too long
	if(len>MAX_FILENAME || strlen(extension)> MAX_EXTENSION ||
		strlen(directory)>MAX_FILENAME) return -ENAMETOOLONG;

	disk = fopen(".disk", "r+b");
	fread(&root, 1, BLOCK_SIZE, disk);
	num_dir = root.nDirectories;

	// If no folders exist, error
	if(num_dir == 0){fclose(disk); return -ENOENT;}

	// Check if the directory exists
	int max = num_dir;
	int i;
	for(i = 0; i < max; i++)
		if(strcmp(root.directories[i].dname, directory) == 0) break;

	if (i == max){fclose(disk); return -ENOENT;}// Directory not found

	int dirnum = i;

	// Else the directory is valid so seek to directory & read its contents
	cs1550_directory_entry dir;
	fseek(disk,i*BLOCK_SIZE, SEEK_CUR); // Resume reading from end of 1st block
	fread(&dir, 1, BLOCK_SIZE, disk);

	// Check if the file exits
	if(dir.nFiles > 0)
	{
		max = dir.nFiles;
		for(i = 0; i < max; i++)
			if(strcmp(dir.files[i].fname, filename) == 0 && 
				strcmp(dir.files[i].fext,extension))
				{
					fclose(disk); return -EEXIST;
				} 
	}

	// The file doesn't exist. Check if the directory is full
	if(dir.nFiles == MAX_FILES_IN_DIR) { fclose(disk); return -EPERM;}

	// Get the bitmap & check for the next available block
	fseek(disk, -(BLOCK_SIZE*BLOCKS_FOR_BITMAP), SEEK_END);
	fread(bitmap, 1, MAX_BITMAP_ENTRIES, disk);

	long nstart = 0;
	i = MAX_DIRS_IN_ROOT/8;

	//Special checks for first blocks
	if(bitmap[i]!=0xff && (MAX_FILES_IN_DIR%8) < 7)
	{	
		int j = 6 - (MAX_FILES_IN_DIR%8); 
		for(;j > -1; j--)
			if( ((bitmap[i]>>j)&0x01) == 0) 
			{
				bitmap[i] = bitmap[i] | (0x01<< j); // update bitmap 
				
				// Update the bitmap on .disk
				fseek(disk, -MAX_BITMAP_ENTRIES, SEEK_CUR);
				fwrite(bitmap, 1, MAX_BITMAP_ENTRIES, disk);

				nstart= MAX_DIRS_IN_ROOT + (7 - (MAX_FILES_IN_DIR%8) - j);
				//update directory
				i = dir.nFiles;
				dir.nFiles = i + 1;
				strcpy(dir.files[i].fname, filename);
				strcpy(dir.files[i].fext, extension);
				dir.files[i].fsize = 0;
				dir.files[i].nStartBlock = nstart;

				// Write updated directory to the disk
				fseek(disk,(dirnum + 1)*BLOCK_SIZE, SEEK_SET);
				fwrite(&dir, 1, BLOCK_SIZE, disk);
				fclose(disk);
				return 0;
			}
	}

	// Otherwise start at the end of the file bits & keep going until a an 
	// empty or semi-empty byte is found
	i++;
	for(;i<MAX_BITMAP_ENTRIES; i++)
		if(bitmap[i]!= 0xff)
		{
			int j = 7; 
			for(;j > -1; j--)
				if( ((bitmap[i]>>j)&0x01) == 0) 
				{
					bitmap[i] = bitmap[i] | (0x01<< j); // update bitmap 
					
					// Update the bitmap on .disk
					fseek(disk, -MAX_BITMAP_ENTRIES, SEEK_CUR);
					fwrite(bitmap, 1, MAX_BITMAP_ENTRIES, disk);

					nstart= i*8 + (7 - j);
					//update directory
					i = dir.nFiles;
					dir.nFiles = i + 1;
					strcpy(dir.files[i].fname, filename);
					strcpy(dir.files[i].fext, extension);
					dir.files[i].fsize = 0;
					dir.files[i].nStartBlock = nstart;

					// Write updated directory to the disk
					fseek(disk,(dirnum + 1)*BLOCK_SIZE, SEEK_SET);
					fwrite(&dir, 1, BLOCK_SIZE, disk);
					fclose(disk);
					return 0;
				}
		}

	// Otherwise entire .disk is full
	fclose(disk);
	return -EPERM;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/* 
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	struct stat stbuf; memset(&stbuf, 0, sizeof(struct stat));

	// Zero out the strings
	directory[0] = '\0'; filename[0] = '\0';extension[0] = '\0';

	// Parse the path into usable strings
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	printf("READ() Dir=%s, fname=%s, ext=%s\n",directory, filename, 
		extension);

	if(strlen(extension) == 0) { return -EISDIR;}

	//check to make sure path exists
	if(cs1550_getattr(path,&stbuf) != 0) return 0;
	// directory_num & fileindex variable updated by cs1550_getattr()

	//check that size is > 0
	if(size == 0) return 0;

	//check that offset is <= to the file size
	if (offset > stbuf.st_size) return 0;

	//read in data
	//set size and return, or error
	disk = fopen(".disk", "rb");
	// Get the directory from .disk
	directory_num++; // Index starts from 0, add 1 to get position on .disk
	fseek(disk, directory_num*BLOCK_SIZE, SEEK_CUR);
	fread(&de, 1, BLOCK_SIZE, disk);

	long filestart = de.files[fileindex].nStartBlock;
	// Seek to the beginning of the file
	fseek(disk, filestart*BLOCK_SIZE + offset, SEEK_SET); 
	fwrite(buf, 1, size, disk);	

	fclose(disk);
	return size;
}

/* 
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size, 
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	struct stat stbuf; memset(&stbuf, 0, sizeof(struct stat));

	//check to make sure path exists
	if(cs1550_getattr(path,&stbuf) != 0) return 0;
	// directory_num & fileindex variable updated by cs1550_getattr()

	//check that size is > 0
	if(size == 0) return 0;

	//check that offset is <= to the file size
	if (offset > stbuf.st_size) return -EFBIG;

	//write data
	//set size (should be same as input) and return, or error

	disk = fopen(".disk", "r+b");
	// Get the directory from .disk
	directory_num++; // Index starts from 0, add 1 to get position on .disk
	fseek(disk, directory_num*BLOCK_SIZE, SEEK_CUR);
	fread(&de, 1, BLOCK_SIZE, disk);
	size_t oldsize = de.files[fileindex].fsize;
	size_t newsize = oldsize + size;
	
	long filestart = de.files[fileindex].nStartBlock;

	// Calculate the number of blocks currently used
	int curr_blocks = oldsize/BLOCK_SIZE + (oldsize%BLOCK_SIZE) > 0 ? 1:0;
	int new_blocks  = newsize/BLOCK_SIZE + (newsize%BLOCK_SIZE) > 0 ? 1:0;


	// Verify that new data will fit in current block.
	if(curr_blocks == new_blocks)
	{
		// Update the directory entry on disk
		de.files[fileindex].fsize = newsize;
		fseek(disk, -BLOCK_SIZE, SEEK_CUR);
		fwrite(&de, 1, BLOCK_SIZE, disk);	

		// Seek to the beginning of the file
		fseek(disk, filestart*BLOCK_SIZE + oldsize, SEEK_SET); 
		fwrite(buf, 1, size, disk);	
	} 
	else // Data won't fit. Need to find new set of contiguous free space
	{
		// First check if there are free blocks directly after the end of the
		// current set of allocated blocks. If there aren't, will need to 
		// copy all of the data to a new start block and then append

		// calculate the offset of where the current data allocation ends
		int offset = (filestart+oldsize)%8;
		int index  = (filestart+oldsize)/8;
		int blockdiff = new_blocks - curr_blocks;
		int count = 0;
		int goal = blockdiff;

		int new_start = 0;


		// Find the new blocks in the bitmap
		for(; index< MAX_BITMAP_ENTRIES; index++)
		{
			for(; offset<8; offset++)
			{
				if(((bitmap[index]>>(7-offset)) & 0x01) == 0)
				{
					count++;
					if(count == goal)
					{
						new_start = index*8 + offset + 1 - new_blocks;
						break;
					}
				}
				else{ count = 0; goal = new_blocks;} 
			}
			offset = 0;
		}

		if(new_start == 0) // No blocks available to make the request
		{
			fclose(disk);
			return -EFBIG;
		}

		// update bits in bitmap accordingly
		if (new_start == filestart)
		{
			offset = (filestart+oldsize)%8;
			index  = (filestart+oldsize)/8;
			count = 0;
			goal = blockdiff;
		}
		else
		{
			offset = filestart%8;
			index  = filestart/8;
			count = 0;
			goal = curr_blocks;
			unsigned char mask  = 0;
			// Free the old bits 
			for(; index< MAX_BITMAP_ENTRIES; index++)
			{
				for(; offset<8; offset++)
				{
					switch(offset) 
					{
						case 0: mask = 0x7f; break;
						case 1: mask = 0xbf; break;
						case 2: mask = 0xdf; break;
						case 3: mask = 0xef; break;

						case 4: mask = 0xf7; break;
						case 5: mask = 0xfb; break;
						case 6: mask = 0xfd; break;
						case 7: mask = 0xfe; break;
					}
					bitmap[index] = mask & bitmap[index];
					count++;
					if(count == goal) break;
				}
				offset = 0;
			}

			offset = new_start%8;
			index  = new_start/8;
			count = 0;
			goal = newsize;

			de.files[fileindex].nStartBlock = new_start;
		}

		// Mark the new block in the bitmap
		for(; index< MAX_BITMAP_ENTRIES; index++)
		{
			for(; offset<8; offset++)
			{
				bitmap[index] = 0x01 <<(7-offset) | bitmap[index];
				count++;
				if(count == goal) break;
			}
			offset = 0;
		}

		// Save bitmap back to the .disk
		fseek(disk, -(BLOCK_SIZE*BLOCKS_FOR_BITMAP), SEEK_END);
		fread(bitmap, 0, MAX_BITMAP_ENTRIES, disk);


		// Update the directory entry on disk
		de.files[fileindex].fsize = newsize;
		fseek(disk, directory_num*BLOCK_SIZE, SEEK_SET);
		fwrite(&de, 1, BLOCK_SIZE, disk);	

		// Write
		if (new_start == filestart)
		{
			fseek(disk, filestart*BLOCK_SIZE + oldsize, SEEK_SET); 
			fwrite(buf, 1, size, disk);	
		}
		else
		{
			char buf2[MAX_DATA_IN_BLOCK];
			int i;
			for(i = 0; i<curr_blocks; i++)
			{
				fseek(disk, (filestart+i)*BLOCK_SIZE, SEEK_SET); 
				fread(&buf2, 0L, BLOCK_SIZE, disk);

				fseek(disk, (new_start+i)*BLOCK_SIZE, SEEK_SET);
				fwrite(buf2, 1, BLOCK_SIZE, disk);
			}

			fseek(disk, new_start*BLOCK_SIZE + oldsize, SEEK_SET); 
			fwrite(buf, 1, size, disk);	

		}
	}

	fclose(disk);
	return size;
}

/*****************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 ****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or 
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/* 
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but 
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file 
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir		= cs1550_mkdir,
	.rmdir 		= cs1550_rmdir,
    .read		= cs1550_read,
    .write		= cs1550_write,
	.mknod		= cs1550_mknod,
	.unlink 	= cs1550_unlink,
	.truncate 	= cs1550_truncate,
	.flush 		= cs1550_flush,
	.open		= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
