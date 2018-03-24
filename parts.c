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

//Prototypes DISKLIST
void disklist( int argc, char* argv[] );

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

	//Start RootDir low range: 27136, End RootDir high range: 31232
	printf("RootDir low: %ld\n", getRootDirStarts(address)*getBlocksize(address) );
	printf("RootDir high %ld\n", getRootDirStarts(address)*getBlocksize(address) + 
							   getRootDirBlocks(address)*getBlocksize(address));

	int rootDirLow = getRootDirStarts(address)*getBlocksize(address);
	int rootDirHigh = getRootDirStarts(address)*getBlocksize(address) + getRootDirBlocks(address)*getBlocksize(address);
	
	int i = 0;
	short status = 0;
	long filesize = 0;
	unsigned int filename = 0;
	int file_mod_time = 0;

	for( i = rootDirLow; i < rootDirLow + 64; i+=64 ) {
		
	}

	close(fp);
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

short getDirectoryStatus( void* address ) {
	//TODO create functions to retrieve specific data from directory entries
}

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
        current_entry = htonl(current_entry);
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
