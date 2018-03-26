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
const int i = 1;
#define is_bigendian() ( (*(char*)&i) == 0 )
#define DEBUG 0


//Structs
/*typedef struct DirectoryEntry {
	struct DirectoryEntry* next;
	uint8_t status;
	long size;
	unsigned int name;
	int mod_time;
} DirectoryEntry;*/

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



//Prototypes DISKLIST
void disklist( int argc, char* argv[] );
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
short getBlocksize( void* );
long getBlockcount( void* );
long getFATstarts( void* );
long getFATblocks( void* );
long getRootDirStarts( void* ); 
long getRootDirBlocks( void* );
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

int main( int argc, char* argv[] ) {
    #if defined(PART1)
        diskinfo(argc,argv);
	#elif defined(PART2)
		disklist(argc,argv);
    #else
        #error "Part[1234] must be defined"
    #endif
    return 0;
}

void disklist( int argc, char* argv[] ) {


	if( argc < 3 ) {
		printf("Error: Not enough arguments!! Try ./disklist test.img /directory\n");
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

		/*This packed struct might be useful later on
		for writing a file to disk; could be turned into
		a function for writing any number of files*/
		struct dir_entry_t* temp = malloc( sizeof(struct dir_entry_t) );
		/*Create_time*/
		struct dir_entry_timedate_t* ttemp = malloc( sizeof(struct dir_entry_timedate_t) );
		/*Mod_time*/
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
   
    printf("Super block information:\n");
    printf("Block size: %hu\n", getBlocksize(address) );
    if( DEBUG ) printf("Block Size HEX: 0x%08x\n", getBlocksize(address) );
    printf("Block count: %ld\n", getBlockcount(address) );
    if( DEBUG ) printf("Block Count HEX: 0x%08lx\n", getBlockcount(address) );
    printf("FAT starts: %ld\n", getFATstarts(address) );
    if( DEBUG ) printf("FAT starts HEX: 0x%08lx\n", getFATstarts(address) );
    printf("FAT blocks: %ld\n", getFATblocks(address) );
    if( DEBUG ) printf("FAT blocks HEX: 0x%08lx\n", getFATblocks(address) );
    printf("Root directory start: %ld\n", getRootDirStarts(address) );
    if( DEBUG ) printf("Root directory start HEX: 0x%08lx\n", getRootDirStarts(address) );
    printf("Root directory blocks: %ld\n", getRootDirBlocks(address) );
    if( DEBUG ) printf("Root directory blocks HEX: 0x%08lx\n", getRootDirBlocks(address) );
    printf("\nFAT Information:\n");
    getFreeBlocks(address);

    printf("Free Blocks: %d\n", free_blocks);
    printf("Reserved Blocks: %d\n", res_blocks);
    printf("Allocated Blocks: %d\n", alloc_blocks);
    if( DEBUG) printf("EOF Blocks: %d\n", eof_blocks);

    close(fp);
}

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


//TODO long formatting for i likely not necessary here
void getFreeBlocks( void* address ) {

    /* Here we have transition from end of FAT to allocated files
    byte 26624 is the last string of zeroes inthe last FAT row.
    00067e0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
    00067f0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
    0006800: 090a 0969 6620 2820 6172 6776 5b33 5d20  ...if ( argv[3]
    */

    //26624 0x6800 might be where some files are allocated
    long first_FAT_block = getBlocksize(address)*getFATstarts(address);
    long i = 0;
    unsigned int current_entry = 0;
    //Note that sizeof int is 4 bytes, ie 0x11223344
    if( DEBUG ) printf("SIZEOF int%zu\n", sizeof(int) );
    
    if( DEBUG ) printf("First FAT block: %ld\n", first_FAT_block);

     //4 entries per line
    //FAT table is 50 blocks long (512*50 blocks)
    //FAT starts at byte 1024 (0x400) to byte 25600 (0x6400)
    //Range of FAT table 1024 to 26624 **CONFIRMED
    
    //Each entry is 4 bytes long.
    //There are 4 entires per 16 bytes
    long range_low = first_FAT_block;
    long range_high = getBlocksize(address)*getFATblocks(address) + first_FAT_block;

    if( DEBUG ) printf("getFreeBlocks FAT range_low is: %ld\n", range_low);
    if( DEBUG ) printf("getFreeBlocks FAT range_high is: %ld\n", range_high);

    for( i = range_low; i < range_high; i+=4 ) {

        memcpy( &current_entry, (address+i), 4 );
        if( !is_bigendian() ) current_entry = htonl(current_entry);
        //if( i >= range_low && i < range_low + 256 ) 
        //printf("%04lx Reading 0x%08x: %d\n", i, current_entry, current_entry);
        if( current_entry == 0x00000000 ) free_blocks++;
        else if ( current_entry == 0x00000001 ) res_blocks++;
        else if ( current_entry > 0x00000002 &&
                  current_entry < 0xFFFFFF00 ) alloc_blocks++;
        else if ( current_entry == 0xFFFFFFFF ) eof_blocks++;
    }
}

short getBlocksize( void* address ) {
    short blocksize=0;
    size_t blocksize_offset = 8;
    size_t blocksize_width = 2;
    memcpy( &blocksize, address+blocksize_offset, blocksize_width );
    if( !is_bigendian() ) blocksize = htons(blocksize);
    return blocksize;
}

long getBlockcount( void* address ) { 
    long blockcount=0;
    size_t blockcount_offset = 12;
    size_t blockcount_width = 4;
    memcpy( &blockcount, address+blockcount_offset, blockcount_width );
    if( !is_bigendian() ) blockcount = htons(blockcount);
    return blockcount;
}
long getFATstarts( void* address ) { 
    long fat_starts=0;
    size_t fat_starts_offset = 16;
    size_t fat_starts_width = 4;
    memcpy( &fat_starts, address+fat_starts_offset, fat_starts_width );
    if( !is_bigendian() ) fat_starts = htons(fat_starts);
    return fat_starts;
}
long getFATblocks( void* address ) { 
    long fat_blocks=0;
    size_t fat_blocks_offset = 20;
    size_t fat_blocks_width = 4;
    memcpy( &fat_blocks, address+fat_blocks_offset, fat_blocks_width );
    if( !is_bigendian() ) fat_blocks = htons(fat_blocks);
    return fat_blocks;
}
long getRootDirStarts( void* address ) { 
    long root_dir_starts=0;
    size_t root_dir_starts_offset = 24;
    size_t root_dir_starts_width = 4;
    memcpy( &root_dir_starts, address+root_dir_starts_offset, root_dir_starts_width );
    if( !is_bigendian() ) root_dir_starts = htons(root_dir_starts);
    return root_dir_starts;
}
long getRootDirBlocks( void* address ) { 
    long root_dir_blocks=0;
    size_t root_dir_blocks_offset = 28;
    size_t root_dir_blocks_width = 4;
    memcpy( &root_dir_blocks, address+root_dir_blocks_offset, root_dir_blocks_width );
    if( !is_bigendian() ) root_dir_blocks = htons(root_dir_blocks);
    return root_dir_blocks;
}
