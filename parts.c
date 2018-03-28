/*
--A directory entry size has nothing to do with the contents, its the numblocks of dir * blocksize


--Re packed structures: Just need to create the struct, the point the structure
to the correct location in the memory mapping ie;
 struct superblock_t* sb;
 sb = struct superblock_t* sb;
 printf( %x\n), ntohl(sb->file_system_block_count);

 //What happens if we dont have the padding?
 ie struct{ char, int } 8 bytes
	//We get 3 bytes of padding here (between char & int)

 	struct{ char, long} 16 bytes
	//We get 7 bytes of padding here (between char & long)

	if the correct val in superblock for blockcount is 0x1900
	without pack we end up with 0x19000000

	
--MAP SHARED (in mmap)
If we modify something, copy_on_write, if we modify it the original data
remains the same (handy if we're modifying the test image).


--in FAT ffff ffff is end of file, but also that block has data.

*/

// Block size: 512  <- offset 8
// Block count: 5120 <-- offset 12 
// FAT starts: 1 <-- offset 16
// FAT blocks: 40 <-- offset 20
// Root directory start: 41 <-- offset 24
// Root directory blocks: 8 <-- offset 28

// FAT information:
// Free Blocks: 5071
// Reserved Blocks: 41
// Allocated Blocks: 8

/*
Block Size: 512
Block Count: 6400
FAT starts: 2
FAT blocks: 50
Root directory start: 53
Root directory blocks: 8
*/

/*
FAT information:
Free Blocks: 5071
Reserved Blocks: 41
Allocated Blocks: 8
*/

#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <byteswap.h>
const int i = 1;
#define is_bigendian() ( (*(char*)&i) == 0 )
#define DEBUG 0

//Superblock struct
struct __attribute__((__packed__)) superblock_t {
	uint8_t fs_id[8];
	uint16_t block_size;
	uint32_t file_system_count;
	uint32_t fat_start_block;
	uint32_t fat_block_count;
	uint32_t root_dir_start_block;
	uint32_t root_dir_block_count;
};
//Time and date
struct __attribute__((__packed__)) dir_entry_timedate_t {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
};
//Directory entry
struct __attribute__((__packed__)) dir_entry_t {
	uint8_t status;
	uint32_t starting_block;
	uint32_t block_count;
	uint32_t size;
	struct dir_entry_timedate_t modify_time;
	struct dir_entry_timedate_t create_time;
	uint8_t filename[31];
	uint8_t unused[6];
};


//Prototypes DISKGET
void diskget( int argc, char* argv[] );

//Prototypes DISKLIST
//void disklist( int argc, char* argv[] );
uint8_t getDirectoryEntryStatus( void* address, int offset );
uint32_t getDirectoryEntryStartingBlock( void* address, int offset );
uint32_t getDirectoryEntryNumBlocks( void* address, int offset );
uint32_t getDirectoryEntryFileSize( void* address, int offset );
//void populateDirectoryEntry_dateTimeStructure( void*, int, dir_entry_timedate_t* );
void populateDirectoryEntry_timedateStructure_create( void* address, int offset, struct dir_entry_timedate_t* ttemp );
void populateDirectoryEntry_timedateStructure_mod( void* address, int offset, struct dir_entry_timedate_t* ttemp );
void populateFilename( void* address, int offset, struct dir_entry_t* temp );

//Prototypes DISKINFO
void diskinfo( int argc, char* argv[] );
void getFreeBlocks( void* );

//Globals
//These are FAT variables
//These could be moved into non-globals
int free_blocks=0;
int res_blocks=0;
int alloc_blocks=0;
int eof_blocks=0;
//For disklist IF a linked file list is made
int dir_list_count=0;


struct superblock_t* superblock;
struct dir_entry_t*	dir_entry;
struct dir_entry_timedate_t* dir_entry_timedate_mod;
struct dir_entry_timedate_t* dir_entry_timedate_create;

int main( int argc, char* argv[] ) {
    #if defined(PART1)
        diskinfo(argc,argv);
	#elif defined(PART3)
		diskget(argc,argv);
    #else
        #error "Part[1234] must be defined"
    #endif
    return 0;
}

//************* DISK GET ****************
void diskget( int argc, char* argv[] ) {
	uint64_t i = 0x0000000000000000;	
	if( argc < 4 ) {
		printf("Error, not enough arguments!! Try ./disklist test.img /img_dir/file.txt renamed_file.txt\n");
		exit(1);
	}
	else if( argc == 4 ){ //program name, disk.img, subdir/filename, renamed_filename
	
		

		/*MAP THE IMAGE FILE*/
		//First map the image file
		//mmap the disk.img to memory, get with "address" pointer
		int fp = open( argv[1], O_RDWR );
		struct stat buffer;
		fstat(fp, &buffer);
		void* address=mmap(NULL, buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
		if (address == MAP_FAILED) { perror("Map source error\n"); exit(1); }
		superblock = address;
		
		/*PARSE IN THE PATH*/
		//Parse argv[2] into an array of subdir names, or filenames, path[parse_count]
		char** path;
		char delim[2] = "/";
		int pathcount = 0;
		char* token = strtok( argv[2], delim );
		while( token != NULL ) {
			if( pathcount == 0 ) {
				path = (char**) malloc(sizeof(char*));
			}
			else {
				path = (char**) realloc( path,pathcount );
			}	
			printf("Token: %s\n", token);
			path[pathcount] = (char*)malloc( strlen(token)+1 );
			//path[pathcount] = '\0';
			//printf("Reallocating, pathcount: %d\n", pathcount);
			strcpy( path[pathcount], token );
			token = strtok(NULL, delim);
			pathcount++;
		}

		/*SCAN THROUGH DIRECTORY BLOCKS TO FIND FILENAME*/
		//While !file_found
		int file_found = 0;
		//int atByteAddress = 0;
		char cur_entry_name[31];//the current filename to compare to path[path_index]
		//int cur_entry_starting_block=0;//need this to spec low and high range for iteration
		uint64_t currentDirLow;
		uint64_t currentDirHigh;
		//A temp file struct to compare the filename we want to copy
		//struct dir_entry_t* temp = malloc( sizeof(struct dir_entry_t) );
		int path_index = 0; //root is 0, /path1 is 1, etc.
		memset( cur_entry_name, '\0', 31 );

		//Only need to loop as many times as there are path name directories
		//since filenames can look like directories (ie Makefile) we loop at most pathcount # of times
		//Moreover, if file found no need to loop
		//Moreover, if path_index == pathcount-1, this means that the end has been reached
		//and the file hasn't been found in the current subdir
		while( path_index < pathcount && !file_found ) {

			//Find path[0..i] "filename" or "subdir" name in current block
			//Set the directory byte range for searching ability
			//Here we check which subdirectory to go to
			//Start at the root (so path index is 0)
			if( path_index == 0 ) {
				//Start RootDir low range: 27136, End RootDir high range: 31232
				currentDirLow = (uint64_t)( htonl(superblock->root_dir_start_block)*htons(superblock->block_size) );
				//currentDirLow = getRootDirStarts(address)*getBlocksize(address);
				currentDirHigh = (uint64_t)( currentDirLow + htonl(superblock->root_dir_block_count)*htons(superblock->block_size) );
				//currentDirHigh = currentDirLow + getRootDirBlocks(address)*getBlocksize(address);
			}
			
			//We have the directory byte range, now we must scan through, populating
			//each entry in the range as we did in disklist, after every population,
			//we compare the filename. If it's a match, set file_found = true, then start copying process.
			//if it's NOT a match (we're only doing root for now), then we exit with "File not found"

			/*CREATE FILE INFO STRUCTS FOR COMPARISONS AND "NEXT" INFO*/
			for( i = currentDirLow; i < currentDirHigh; i+=0x00000040 ) {
				printf("Trying to find %s...\n", path[path_index]);
				printf("In block; iterating from %"PRIx64". Low is %"PRIx64". High is %"PRIx64"\n", i, currentDirLow, currentDirHigh);
				if( DEBUG ) printf("[Diskget-main()] Start of entry (in hex): %"PRIx64"\n", i);

				//Assign dir entry struct to the address with offset i
				dir_entry = (address+i);
				dir_entry_timedate_mod = (address+i+0x00000014);
				dir_entry->modify_time = (*dir_entry_timedate_mod);
				dir_entry_timedate_create = (address+i+0x0000000D);
				dir_entry->create_time = (*dir_entry_timedate_create);
				
				//Found the file (If current entry is a file, ie hex 0x3 and filenames match)
				if( dir_entry->status == (uint8_t)0x3 && !strcmp( path[path_index], (char*)dir_entry->filename ) ) {
					file_found = 1;//true
					break;
				}

				//Otherwise Found Directory (if status is 0x5 and path token matches)
				else if( dir_entry->status == (uint8_t)0x5 && !strcmp( path[path_index], (char*)dir_entry->filename ) ) {
					printf("Found subdir, going to next subdir...\n");
					/*UPDATE THE BYTE RANGE HERE, CRITICAL
					Can be done from the file (now dir) struct information*/
					//only increment path index if subdir actually found
					//from current dir
					path_index++;
					break;
				}
				//Or we got to the end of the block and nothing found
				//TODO ensure the last file in directory block is REALLY being read
				else if( i == currentDirHigh-0x00000040 ){
					printf("Nothing found\n");
					path_index = pathcount;//just to exit while loop					
					break;
				}
			}
		}
		
		if( file_found ) {
			printf("FILE FOUND (see info below)\n");
			printf("Diskget struct status: 0x%"PRIx8"\n", dir_entry->status);
			printf("Diskget struct starting_block: 0x%"PRIx32"\n", htonl(dir_entry->starting_block) );
			printf("Diskget struct num_blocks: 0x%"PRIx32"\n", htonl(dir_entry->block_count) );
			printf("Diskget struct file_size (hex): 0x%"PRIx32"\n", htonl(dir_entry->size) );
			printf("Diskget struct file_size (bytes): %d\n", (int)( htonl(dir_entry->size) ) );
			printf("Diskget struct filename: %s\n", (char*)dir_entry->filename );
			struct dir_entry_timedate_t temp = dir_entry->modify_time;
			struct dir_entry_timedate_t ttemp = dir_entry->create_time;
			//(*dir_entry_timedate_create) = dir_entry->create_time;
			printf("Diskget struct mod_time: %d\n", (int)htons((temp.year)));
			printf("Diskget struct create_time: %d\n", (int)htons((ttemp.year)));

		}
		else {
			printf("File not found\n");
			exit(1);
		}
		
		/*
		//Now we found the file, we're in a specific subdir (ie at block x) get the file starting block
		//Based on the file information, create an array to hold file block references; file_blocks[getNumBlocks()]
		//TODO I think that the FAT "next" pointers are only 1 byte wide
		//but its possible that this might need to be revised at some point (ie if we're not getting the correct results)
		unsigned int file_blocks[ (*temp).block_count ];

		//also create an buffer array of 1 byte long hex-values; ie FF
		//The data type might be uint8_t file_hex_bytes[file_byte_size]
		uint8_t file_hex_bytes[ (*temp).size+1 ];
		
		//Look in FAT for file starting block X, which contains Y, go to Y, get Y-next etc
			//For each link (starting block, Y, Y-next etc) add the value to file_blocks[ 0...num_blocks-1 ]
		unsigned int cur = (unsigned int)(*temp).starting_block;

		//TODO Fix this because it's segfaulting
		file_blocks[0] = cur;
		for( i = 1; i < (*temp).block_count; i++ ) {
			memcpy( &cur, address+getFATstarts(address)+(4*cur), 4);
			file_blocks[i] = cur;
			printf("block is %u\n", file_blocks[i]);
		}

		int block_range_low;
		int block_range_high;
		//int byte_counter=0;
		int bytecount=0;
		//For each refrence in file_blocks
		for( i = 0; i < (*temp).block_count; i++ ) { //go to first file block
			//Set the block_range_low = file_blocks[n]*blockSize
			block_range_low = file_blocks[i]*getBlocksize(address);
			//Set the block_range_high = block_range_low + blockSize
			block_range_high = block_range_low + getBlocksize(address);
			//for all bytes in range block_range_low to block_range_high
			for( i = block_range_low; i < block_range_high; i++ ) {
				//Copy hex-byte-pair to file_hex_bytes (previously defined as large as file size)
				memcpy( &file_hex_bytes[i], address+i, 1 );
				bytecount++;
				//Assume that we don't need to worry about big-endian/little-endian for now
				//bytecount++
			}
			
		}

					
		//Open a file (with the same name as the located file) in current directory in linux
		//Write the file_hex_bytes array, byte-by-byte, to the open file
		//Finally, close the file.
		
		*/
		int j;
		for( j = 0; j < pathcount; j++) free( path[j] );
		free(path);
	}
	else{
		printf("Error, too many arguments!! Try ./disklist test.img /img_dir/file.txt renamed_file.txt\n");
		exit(1); 
	}
		
}

//************* DISK LIST ****************

/*
void disklist( int argc, char* argv[] ) {


	if( argc < 3 ) {
		printf("Error, not enough arguments!! Try ./disklist test.img /directory\n");
		exit(1);
	}
	else if( argc == 3 ){
		int fp = open( argv[1], O_RDWR );
		struct stat buffer;
		int status = fstat(fp, &buffer);
		if( DEBUG ) printf("Status of buffer: %d\n", status);
		if( DEBUG ) printf("Size of buffer: %zu bytes\n", (size_t)(buffer.st_size));
		void* address=mmap(NULL, buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
		if (address == MAP_FAILED) {
					perror("Map source error\n");
					exit(1);
		}

		//The current directory byte range
		int currentDirLow;
		int currentDirHigh;
		int i = 0;
		//Here we check which subdirectory to go to
		//If arg 3 is just '/' we calculate root
		if( !strcmp( argv[2],"/" ) ) {
			currentDirLow = getRootDirStarts(address)*getBlocksize(address);
			currentDirHigh = currentDirLow + getRootDirBlocks(address)*getBlocksize(address);
			//Start RootDir low range: 27136, End RootDir high range: 31232
			printf("RootDir low: %ld\n", getRootDirStarts(address)*getBlocksize(address) );
			printf("RootDir high %ld\n", getRootDirStarts(address)*getBlocksize(address) + 
									   getRootDirBlocks(address)*getBlocksize(address));

		}
		//Otherwise the subdirectory was specified by name
		//If that's the case then we have to search for the starting block
		//of the subdirectory in question.
		// 1) Get subdirectory name (from arg)
		//		   1.1) Note that if arg3 is /subdir1/subdir2/... we only need to check root for subdir1 and go there
		//				if it's actually present. Otherwise we gracefully exit. Similarly for subdir2...subdirN
		//
		// 2) Search through all entries in the root (or current) directory for current parsed arg (ie /subdir1/subdir2/...)
		//	  Side note here, we can't know ahead of time if the subdir actually exists in the Filesystem.
		//    This can be solved efficiently (in execution) with a nice graph and search algo
		//	  For the sake of time (actually getting this done) maybe we can just do a brute force
		//	  search every time instead of something elegant. Ie; scan root (if found go to subdir) repeat
		//	  until the very last subdirectory is linearly scanned, if no match return error and exit.
		//		2.1) If the subdirectory name is a match, also ensure it's actually a dir, not a file
		//			2.1.1) If it's a file, error and exit
		//			2.1.2) If it's a DIR then set "dirLow = getSubdirectoryStarts(address)*getBlocksize(address)"
		//				   and iterate as you would in the root directory

		//TODO implement subdirectory search. We might need a loop which calculates
		//the current directory block for the next subdirectory name, then updates
		//the currentDirHigh and currentDirLow values when found (do after part3 and 4 if time)
		else {
			printf("Error, subdirectories other than \"root\" are not yet implemented\n");
			exit(1);
		}

		//HERE WE ACTUALLY LIST THE DIRECTORY CONTENTS
		//(prior to this we're just searching for the correct location to list)

		//This packed struct might be useful later on
		//for writing a file to disk; could be turned into
		//a function for writing any number of files
		struct dir_entry_t* temp = malloc( sizeof(struct dir_entry_t) );
		//Create_time
		struct dir_entry_timedate_t* ttemp = malloc( sizeof(struct dir_entry_timedate_t) );
		Mod_time
		struct dir_entry_timedate_t* tttemp = malloc( sizeof(struct dir_entry_timedate_t) );

		for( i = currentDirLow; i < currentDirHigh; i+=64 ) {
			if( DEBUG ) printf("[Disklist-main()] Start of entry (in bytes): %d\n", i);
			(*temp).status = getDirectoryEntryStatus(address,i);
			(*temp).starting_block = getDirectoryEntryStartingBlock(address,i);
			(*temp).block_count = getDirectoryEntryNumBlocks(address,i);
			(*temp).size = getDirectoryEntryFileSize(address,i);
			//Here we populate a temporary timedate struct...
			populateDirectoryEntry_timedateStructure_create(address,i,ttemp);
			populateDirectoryEntry_timedateStructure_mod(address,i,tttemp);
			//And assign the temporary dir entry struct create_time attribute
			//to the temporary timedate object.
			//RECALL we're just writing over the same memory
			//space, so if you need MULTIPLE copies of this, we'll
			//have to malloc up more objects and store them
			(*temp).create_time = (*ttemp);
			(*temp).modify_time = (*tttemp);
			//Memset filename to nulls first
			//Same issue as above though, to expand this we might
			//need to malloc up more structs and init all of their
			//filename array attributes to null chars.
			memset( (*temp).filename, '\0', 31 );
			populateFilename(address,i,temp);
			if( DEBUG ) printf("Temp struct status: 0x%02x\n", (*temp).status);
			if( DEBUG ) printf("Temp struct starting_block: 0x%08x\n", (*temp).starting_block);
			if( DEBUG ) printf("Temp struct num_blocks: 0x%08x\n", (*temp).block_count);
			if( DEBUG ) printf("Temp struct file_size (hex): 0x%08x\n", (*temp).size);
			if( DEBUG ) printf("Temp struct file_size (bytes): %d\n", (*temp).size);
			if( DEBUG )	printf("Create year: 0x%04x\n", (*temp).create_time.year);
			if( DEBUG ) printf("Create month: 0x%04x\n", (*temp).create_time.month);
			if( DEBUG ) printf("Create day: 0x%04x\n", (*temp).create_time.day);
			if( DEBUG ) printf("Create hour: 0x%04x\n", (*temp).create_time.hour);
			if( DEBUG ) printf("Create minute: 0x%04x\n", (*temp).create_time.minute);
			if( DEBUG ) printf("Create second: 0x%04x\n", (*temp).create_time.second);
			if( DEBUG ) printf("Mod year: 0x%04x\n", (*temp).modify_time.year);
			if( DEBUG )	printf("Mod month: 0x%04x\n", (*temp).modify_time.month);
			if( DEBUG )	printf("Mod day: 0x%04x\n", (*temp).modify_time.day);
			if( DEBUG )	printf("Mod hour: 0x%04x\n", (*temp).modify_time.hour);
			if( DEBUG )	printf("Mod minute: 0x%04x\n", (*temp).modify_time.minute);
			if( DEBUG )	printf("Mod second: 0x%04x\n", (*temp).modify_time.second);
			if( DEBUG )	printf("Temp struct filename: %s\n", (*temp).filename );
			
			//TODO Implement ability to specify a directory listing
			//Clearly will need to add a new ARGUMENT check for disklist(argv)
			//But also, need to have a reference to all available directories
			//This might involve a tree structure of some kind
			//Consider a directory with UP TO 128 directory entries
			//Note that the ROOT directory has 8 blocks.
			//Every block has 512 bytes, 64 bytes per entry is 8 entries per block
			//So the ROOT directory can have up to 64 entries (8block*8entries=64)
			//However, we can add directories with arbitrarily more
			//blocks.

			//IDEA: When someone calls disklist /sub_dir, note the name of the sub_dir
			//Conduct the disklist checks and populate all the necessary info, but more than that
			//We'll have to POSSIBLY conduct a recursive directory creation
			//There might be an easier way though: If we know a MAX NUMBER OF ENTRIES (N) per directory
			//then each "directory" block at a specific address, can reference at most (N) number of
			//other directories, or (N) number of files. So each directory can be represented by an array
			//of struct pointers, pointing to directory entries (per inode scheme).
			if( (*temp).status == 0x3 ) {
				printf("F%10d %30s %d/%02d/%02d %02d:%02d:%02d\n", (*temp).size, (*temp).filename, 
				(*temp).modify_time.year,(*temp).modify_time.month, (*temp).modify_time.day,
				(*temp).modify_time.hour, (*temp).modify_time.minute, (*temp).modify_time.second );
			}
		}

		free( temp );
		free( ttemp);
		free( tttemp );
		close(fp);
	}
	else {
		printf("Error, too many arguments! Try: ./disklist test.img /sub_dir\n");
	}
}
*/

//DIRECTORY ENTRY COPY FILENAME
void populateFilename( void* address, int offset, struct dir_entry_t* temp ) {
	memcpy( &(temp->filename), address+offset+27, 30 );
}

//DIRECTORY ENTRY MODIFICATION TIME
void populateDirectoryEntry_timedateStructure_mod( void* address, int offset, struct dir_entry_timedate_t* ttemp ) {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	memcpy( &year, address+offset+20, 2 );
	memcpy( &month, address+offset+22, 1 );
	memcpy( &day, address+offset+23, 1 );
	memcpy( &hour, address+offset+24, 1 );
	memcpy( &minute, address+offset+25, 1 );
	memcpy( &second, address+offset+26, 1 );
	if( !is_bigendian() ) year = htons( year );
	(*ttemp).year = year;
	(*ttemp).month = month;
	(*ttemp).day = day;
	(*ttemp).hour = hour;
	(*ttemp).minute = minute;
	(*ttemp).second = second;
}
//DIRECTORY ENTRY CREATE TIME
void populateDirectoryEntry_timedateStructure_create( void* address, int offset, struct dir_entry_timedate_t* ttemp ) {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	memcpy( &year, address+offset+13, 2 );
	memcpy( &month, address+offset+15, 1 );
	memcpy( &day, address+offset+16, 1 );
	memcpy( &hour, address+offset+17, 1 );
	memcpy( &minute, address+offset+18, 1 );
	memcpy( &second, address+offset+19, 1 );
	if( !is_bigendian() ) year = htons( year );
	(*ttemp).year = year;
	(*ttemp).month = month;
	(*ttemp).day = day;
	(*ttemp).hour = hour;
	(*ttemp).minute = minute;
	(*ttemp).second = second;
}

//DIRECTORY ENTRY FILE SIZE (IN BYTES)
uint32_t getDirectoryEntryFileSize( void* address, int offset ) {
	uint32_t file_size;
	memcpy( &file_size, address+offset+9, 4);
	if( !is_bigendian() ) file_size = htonl( file_size );
	return file_size;
}

//DIRECTORY ENTRY NUM BLOCKS
uint32_t getDirectoryEntryNumBlocks( void* address, int offset ) {
	uint32_t num_blocks;
	//try 3 bytes in for starting block
	memcpy( &num_blocks, address+offset+5, 4);
	if( !is_bigendian() ) num_blocks = htonl( num_blocks );
	return num_blocks;
}

//DIRECTORY ENTRY STARTING BLOCK
uint32_t getDirectoryEntryStartingBlock( void* address, int offset ) {
	uint32_t starting_block;
	//try 3 bytes in for starting block
	memcpy( &starting_block, address+offset+1, 4);
	if( !is_bigendian() ) starting_block = htonl( starting_block );
	return starting_block;
}


//Return exactly 8 bit integer (1 byte status)
uint8_t getDirectoryEntryStatus( void* address, int offset ) {
	//TODO create functions to retrieve specific data from directory entries
	
	//00000011 value of the status "bitmask" denoting a "file" status

	//This is the bitmask applied to
	//FILE or NOT FILE

	//Only ONE of bit 1, or bit 2, can be on, not both.
	//either 0101 or 0011
	//        Dir    File
	//       0111    0111
	//and	 0101    0011 
	//xor    0010    0100 <-- 2 or 4

	uint8_t status_bits;
	if( DEBUG ) printf("[getDirectoryStatus()] Reading from: %04x\n", offset);
	memcpy( &status_bits, address+offset, 1 );
	//if( !is_bigendian() ) status_bits = htons( status_bits );
	//don't need to do htons etc. on single bytes.
	if( DEBUG ) printf("Status found to be: 0x%02x\n", status_bits);
	return status_bits;
}


//************* DISK INFO ****************
void diskinfo( int argc, char* argv[] ) {
    int fp = open( argv[1], O_RDWR );
    struct stat buffer;
    int status = fstat(fp, &buffer);
    if( DEBUG ) printf("Status of buffer: %d\n", status);
    if( DEBUG ) printf("Size of buffer: %zu bytes\n", (size_t)(buffer.st_size));
    void* address=mmap(NULL, buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
    if (address == MAP_FAILED) {
                perror("Map source error\n");
                exit(1);
    }
	superblock = address;
   
    printf("Super block information:\n");
    printf("Block size: %"PRIu16"\n", htons(superblock->block_size));
    printf("Block count: %"PRIu32"\n", htonl(superblock->file_system_count) );
    printf("FAT starts: %"PRIu32"\n", htonl(superblock->fat_start_block) );
    printf("FAT blocks: %"PRIu32"\n", htonl(superblock->fat_block_count) );
    printf("Root directory start: %"PRIu32"\n", htonl(superblock->root_dir_start_block) );
    printf("Root directory blocks: %"PRIu32"\n", htonl(superblock->root_dir_block_count) );
    printf("\nFAT Information:\n");
    getFreeBlocks(address);

    printf("Free Blocks: %d\n", free_blocks);
    printf("Reserved Blocks: %d\n", res_blocks);
    printf("Allocated Blocks: %d\n", alloc_blocks);
    if( DEBUG) printf("EOF Blocks: %d\n", eof_blocks);

    close(fp);
}

void getFreeBlocks( void* address ) {

    uint64_t first_FAT_block = (uint64_t)( htons(superblock->block_size)*htonl(superblock->fat_start_block) );
	uint64_t i = 0x0000000000000000;
	uint32_t entry_len = 0x00000004;
    uint32_t current_entry = 0x00000000;
    if( DEBUG ) printf("SIZEOF int%zu\n", sizeof(int) );
    if( DEBUG ) printf("First FAT block: %ld\n", first_FAT_block);

    uint64_t range_low = first_FAT_block;
    uint64_t range_high = (uint64_t)( htons(superblock->block_size)*htonl(superblock->fat_block_count)+first_FAT_block );

	if( DEBUG ) printf("range_low %"PRIx64" range_high %"PRIx64"\n", range_low, range_high);

    for( i = range_low; i < range_high; i+=entry_len ) {

        memcpy( &current_entry, (address+i), entry_len );
        if( !is_bigendian() ) current_entry = htonl(current_entry);
        if( current_entry == 0x00000000 ) {
			free_blocks++;
		}
        else if ( current_entry == 0x00000001 ){
			res_blocks++;
        	if( DEBUG ) { 
			printf("Reading %"PRIx64"", i);
			printf("  Current entry %"PRIx32"\n", current_entry);
			printf("res_blocks++\n");}
		}
		else {
			alloc_blocks++;
			if( DEBUG ) {
			printf("Reading %"PRIx64"", i);
			printf("  Current entry %"PRIx32"\n", current_entry);
			printf("alloc_blocks++\n");}
		}
    }
}
