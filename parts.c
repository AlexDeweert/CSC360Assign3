/*
	Programming Assignment 3
	CSC360 Winter 2018 Prof. J. PAN
	Student: Alexander (Lee) DEWEERT
	ID: V00855867

	***See README.txt for more info***
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
#include <math.h>
const int xx = 1;
#define is_bigendian() ( (*(char*)&xx) == 0 )
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

//Prototypes DISKPUT
void diskput( int argc, char* argv[] );

//Prototypes DISKGET
void diskget( int argc, char* argv[] );

//Prototypes DISKLIST
void disklist( int argc, char* argv[] );

//Prototypes DISKINFO
void diskinfo( int argc, char* argv[] );
void getFreeBlocks( void* );

//Globals
//These could be moved into non-globals
int free_blocks=0;
int res_blocks=0;
int alloc_blocks=0;
int eof_blocks=0;
//For disklist IF a linked file list is made
int dir_list_count=0;

//Global struct instances
struct superblock_t* superblock;
struct superblock_t* sb;
struct dir_entry_t*	dir_entry;
struct dir_entry_timedate_t* dir_entry_timedate_mod;
struct dir_entry_timedate_t* dir_entry_timedate_create;

//Main
int main( int argc, char* argv[] ) {
    #if defined(PART1)
        diskinfo(argc,argv);
	#elif defined(PART2)
		disklist(argc,argv);
	#elif defined(PART3)
		diskget(argc,argv);
	#elif defined(PART4)
		diskput(argc,argv);
    #else
        #error "Part[1234] must be defined"
    #endif
    return 0;
}

//Write a file onto a disk image
void diskput( int argc, char* argv[] ) {
	if( argc != 4 ) {
		printf("Error!! Need 4 arguments. Try ./diskput test.img file.txt /renamed_file.txt\n");
	}
	else {
	
		//Based on the command line read the specific file into a memory map.
		int host_file_fp = open( argv[2], O_RDONLY );
		int img_fp = open( argv[1], O_RDWR );
		struct stat host_file_buffer;
		struct stat img_buffer;
		fstat(host_file_fp, &host_file_buffer);
		fstat(img_fp, &img_buffer);
		void* host_file_address = mmap(NULL, host_file_buffer.st_size, PROT_READ, MAP_SHARED, host_file_fp, 0);
		void* img_file_address = mmap(NULL, img_buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, img_fp, 0);
		if (host_file_address == MAP_FAILED) { perror("Host file map source error\n"); exit(1); }
		if (img_file_address == MAP_FAILED) { perror("Image file map source error\n"); exit(1); }

		//Map disk image file to mem
		sb = img_file_address;

		//Get linux filename
		if( DEBUG ) printf( "Reading in host file: %s\n", argv[2] );
		
		//Get args filename spec'd
		if( DEBUG ) printf( "Writing to image file: %s\n", argv[1] );

		//Create host file file information
		//Calculate how many blocks are required for the file-to-copy
		uint8_t status = 0x3;
		uint32_t starting_block = 0x99;
		uint32_t block_count = ceil( (double)host_file_buffer.st_size/(double)htons(sb->block_size));
		uint32_t size = host_file_buffer.st_size;
		if( DEBUG ) { printf( "DirEntry Status: %d, Startblock %"PRIx8", Block_count: %d, Size(Bytes): %d\n",
		status,starting_block,block_count,size ); }
		
		//Find num_blocks number of 512 byte contiguous memory locations (locate in FAT)
		uint64_t low, high, i;
		uint32_t block_pointers[block_count], cur;
		int k=0, all_free=0, fat_entry=0;
		low = (uint64_t)(htonl(sb->fat_start_block)*htons(sb->block_size));
    	high = (uint64_t)( htons(sb->block_size)*htonl(sb->fat_block_count)+low );
		if( DEBUG ) printf( "fat low: %"PRIx64", fat high: %"PRIx64"\n", low, high);
		for( i = low; i < high; i+= (0x04) ) {
			memcpy( &cur, img_file_address+i, 4 );
			if( htonl( cur ) == 0 ) { block_pointers[k] = (uint32_t)fat_entry; k++; }
			if( k == block_count ) { all_free = 1; break; }
			fat_entry++;
		}
		if( !all_free ) { printf("Error: No free space.\n"); exit(1); }
		for( i = 0x0; i < block_count; i++ ) {
			if( DEBUG ) printf("block_pointers[ 0x%"PRIx32":%"PRIu32" ] = 0x%"PRIx32":%"PRIu32"\n", 
			block_pointers[i],
			block_pointers[i],
			(uint32_t)(block_pointers[i]*(htons(sb->block_size))), 
			(uint32_t)(block_pointers[i]*(htons(sb->block_size))));
		}
		
		//If found, update starting_block for new file entry
		//Change the values at each FAT entry location to [ start, next, next ... ]
		starting_block = block_pointers[0];
		uint32_t end = 0xFFFFFFFF;
		uint32_t next;
		for( k = 0; k < block_count; k++ ) {
			next = ntohl(block_pointers[k+1]);
			if( k != (block_count-1) ) memcpy( img_file_address+low+(block_pointers[k]*4), &next, 4 );
			else memcpy( img_file_address+low+(block_pointers[k]*4), &end, 4 );
		}

		uint64_t r_low = (uint64_t)( htonl(sb->root_dir_start_block)*htons(sb->block_size) );
		uint64_t r_high = (uint64_t)( r_low + htonl(sb->root_dir_block_count)*htons(sb->block_size) );
		uint8_t cur_status=0x0;
		uint8_t de_status = 0x3;
		uint32_t de_bp = ntohl( block_pointers[0] );
		uint32_t de_bc = ntohl( block_count );
		uint32_t de_size = ntohl( size );
		uint8_t de_fn[31];
		memset( de_fn, '\0', 31 );
		memcpy( de_fn, argv[3]+1, strlen(argv[3])-1 );
		for( i = r_low; i < r_high; i+=0x40 ) {
			memcpy( &cur_status, img_file_address+i, 1 );
			if( cur_status == 0x0 ) {
				memcpy( img_file_address+i, &de_status, 1 );
				memcpy( img_file_address+i+1, &de_bp, 4 );
				memcpy( img_file_address+i+5, &de_bc, 4 );
				memcpy( img_file_address+i+9, &de_size, 4 );
				memcpy( img_file_address+i+27, de_fn, 30 );
				break;
			}
		}

		//For each block in the array[num_blocks], write 512 bytes of the file.mmap to image.mmap 
		//Write the image.mmap out to disk.img
		for( k=0; k < block_count; k++ ) {
			memcpy( img_file_address+(block_pointers[k]*htons(sb->block_size)), host_file_address+(k*htons(sb->block_size)), htons(sb->block_size) );
		}

		//Unmap the mmaps
		munmap( host_file_address, host_file_buffer.st_size );
		munmap( img_file_address, img_buffer.st_size );
	}
}

//Get a file from the disk image
void diskget( int argc, char* argv[] ) {
	uint64_t i = 0x0000000000000000;	
	if( argc < 4 ) {
		printf("Error, not enough arguments!! Try ./disklist test.img /img_dir/file.txt renamed_file.txt\n");
		exit(1);
	}
	else if( argc == 4 ){
		/*MAP THE IMAGE FILE*/
		//First map the image file
		//mmap the disk.img to memory, get with "address" pointer
		int fp = open( argv[1], O_RDWR );
		struct stat buffer;
		fstat(fp, &buffer);
		void* address=mmap(NULL, buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
		if (address == MAP_FAILED) { perror("Map source error\n"); exit(1); }
		superblock = address;
		
		//Unused path parsing
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
			//printf("Token: %s\n", token);
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
				//printf("Trying to find %s...\n", path[path_index]);
				//printf("In block; iterating from %"PRIx64". Low is %"PRIx64". High is %"PRIx64"\n", i, currentDirLow, currentDirHigh);
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
					//printf("Found subdir, going to next subdir...\n");
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
					//printf("Nothing found\n");
					path_index = pathcount;//just to exit while loop					
					break;
				}
			}
		}
		
		if( !file_found ) {
			printf("File not found.\n");
			munmap(address, buffer.st_size);
			exit(1);
		}
		
		
		//Now we found the file, we're in a specific subdir (ie at block x) get the file starting block
		//Based on the file information, create an array to hold file block references; file_blocks[getNumBlocks()]

		//I think that the FAT "next" pointers are only 1 byte wide WRONG: FAT entries are 4 bytes wide, dummy.
		uint32_t file_blocks[ htonl(dir_entry->block_count) ];

		//also create an buffer array of 1 byte long hex-values; ie FF
		//The data type might be uint8_t file_hex_bytes[file_byte_size]
		uint8_t file_bytes[ (int)(htonl(dir_entry->size)) ];

		//printf("Got here\n");
		//Look in FAT for file starting block X, which contains Y, go to Y, get Y-next etc
			//For each link (starting block, Y, Y-next etc) add the value to file_blocks[ 0...num_blocks-1 ]
		uint32_t cur = htonl(dir_entry->starting_block);
		uint8_t k;
		for( k = 0x00; k < htonl(dir_entry->block_count); k+= 0x01 ) {
			//printf("Looking at: %"PRIx64"\n", (uint64_t)(htonl(superblock->fat_start_block)*(htons(superblock->block_size)) + (cur*0x4)) );
			//printf("On file block: %d", (int)k);
			file_blocks[k] = cur;
			memcpy( &cur, address+(uint64_t)(htonl(superblock->fat_start_block)*(htons(superblock->block_size)) + (cur*0x4)), 4 ); 
			cur = htonl(cur);
			//printf(", block is %"PRIx32", cur->next is %"PRIx32"\n", file_blocks[k], cur);
		}
		
		//Now that we have all the block references, we copy byte-by-byte to an array of size dir_entry->filesize
		uint64_t block_range_low;
		uint64_t block_range_high;
		//int byte_counter=0;
		uint64_t bytecount=0x00000000;
		uint32_t m;
		uint64_t b;
		//For each refrence in file_blocks
		for( m = 0x00000000; m < htonl(dir_entry->block_count); m+=0x01 ) { //go to first file block
			//Set the block_range_low = file_blocks[n]*blockSize
			//printf("M: %d, %"PRIx32"\n", (int)m, m );
			//printf("On block %d Looking at: %"PRIx64"\n", m, (uint64_t)(file_blocks[m]*htons(superblock->block_size)));
			block_range_low = (uint64_t)(file_blocks[m]*htons(superblock->block_size));
			//Set the block_range_high = block_range_low + blockSize
			block_range_high = (uint64_t)( block_range_low + htons(superblock->block_size) );
			//for all bytes in range block_range_low to block_range_high
			for( b = block_range_low; b < block_range_high; b+=0x01 ) {
				//Copy hex-byte-pair to file_hex_bytes (previously defined as large as file size)
				
				if( bytecount < htonl(dir_entry->size)) {
					memcpy( &file_bytes[bytecount], address+b, 1 );
					//if( file_bytes[bytecount] == htonl(dir_entry->size) ) {
					//	printf("REACHED EOF!... %"PRIx8"\n", file_bytes[bytecount]);
					//}
					bytecount++;

				}
				else {
					break;
				}
				//Assume that we don't need to worry about big-endian/little-endian for now
				//bytecount++
			}
			
		}

		//Open a file (with the same name as the located file) in current directory in linux
		//Write the file_hex_bytes array, byte-by-byte, to the open file
		//Finally, close the file.
		FILE* write;
		write = fopen(argv[3], "wb");
		int ggg = 0;
		while( ggg < bytecount ) {
			fwrite( &file_bytes[ggg], 1, sizeof(file_bytes[ggg]), write );
			ggg++;
		}
		
		int j;
		for( j = 0; j < pathcount; j++) free( path[j] );
		free(path);
	}
	else{
		printf("Error, too many arguments!! Try ./disklist test.img /img_dir/file.txt renamed_file.txt\n");
		exit(1); 
	}
}

//List all file in root directory
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
		superblock = address;

		//The current directory byte range
		uint64_t currentDirLow;
		uint64_t currentDirHigh;
		uint64_t i = 0x0000000000000000;	
		//Here we check which subdirectory to go to
		//If arg 3 is just '/' we calculate root
		if( !strcmp( argv[2],"/" ) ) {
			//currentDirLow = getRootDirStarts(address)*getBlocksize(address);
			//currentDirHigh = currentDirLow + getRootDirBlocks(address)*getBlocksize(address);
			currentDirLow = (uint64_t)( htonl(superblock->root_dir_start_block)*htons(superblock->block_size) );
			currentDirHigh = (uint64_t)( currentDirLow + htonl(superblock->root_dir_block_count)*htons(superblock->block_size) );
		}
		
		//TODO implement subdirectory search. We might need a loop which calculates
		//the current directory block for the next subdirectory name, then updates
		//the currentDirHigh and currentDirLow values when found (do after part3 and 4 if time)
		else {
			printf("Error, subdirectories other than \"root\" are not yet implemented\n");
			exit(1);
		}

		//HERE WE ACTUALLY LIST THE DIRECTORY CONTENTS
		//(prior to this we're just searching for the correct location to list)
		for( i = currentDirLow; i < currentDirHigh; i+=0x40 ) {
			dir_entry = (address+i);
			dir_entry_timedate_mod = (address+i+0x00000014);
			dir_entry->modify_time = (*dir_entry_timedate_mod);
			dir_entry_timedate_create = (address+i+0x0000000D);
			dir_entry->create_time = (*dir_entry_timedate_create);

			struct dir_entry_timedate_t temp = dir_entry->modify_time;

			//TODO Implement ability to specify a directory listing
			if( dir_entry->status == 0x3 ) {
				printf("F %10d %30s %d/%02d/%02d %02d:%02d:%02d\n", 
				(int)( htonl(dir_entry->size) ), 
				(char*)dir_entry->filename, 
				(int)htons((temp.year)),
				(int)(temp.month), 
				(int)(temp.day),
				(int)(temp.hour), 
				(int)(temp.minute), 
				(int)(temp.second));
			}
		}
		close(fp);
		munmap(address, buffer.st_size);
	}
	else {
		printf("Error, too many arguments! Try: ./disklist test.img /sub_dir\n");
	}
}

//Get disk image information
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
	munmap(address, buffer.st_size );
}

void getFreeBlocks( void* address ) {

	uint64_t b = 0x0000000000000000;
	uint32_t entry_len = 0x00000004;
    uint32_t current_entry = 0x00000000;
    //if( DEBUG ) printf("SIZEOF int%zu\n", sizeof(int) );
    //if( DEBUG ) printf("First FAT block: %ld\n", first_FAT_block);

    uint64_t range_low = (uint64_t)( htons(superblock->block_size)*htonl(superblock->fat_start_block) );
    uint64_t range_high = (uint64_t)( htons(superblock->block_size)*htonl(superblock->fat_block_count)+range_low );

	if( DEBUG ) printf("range_low %"PRIx64" range_high %"PRIx64"\n", range_low, range_high);

    for( b = range_low; b < range_high; b+=entry_len ) {

        memcpy( &current_entry, (address+b), entry_len );
        if( !is_bigendian() ) current_entry = htonl(current_entry);
        if( current_entry == 0 ) {
			free_blocks+=1;
		}
        else if ( current_entry == 1 ){
			res_blocks+=1;
        	if( DEBUG ) { 
			printf("Reading %"PRIx64"", b);
			printf("  Current entry %"PRIx32"\n", current_entry);
			printf("res_blocks++\n");}
		}
		else {
			alloc_blocks+=1;
			if( DEBUG ) {
			printf("Reading %"PRIx64"", b);
			printf("  Current entry %"PRIx32"\n", current_entry);
			printf("alloc_blocks++\n");}
		}
    }
}
