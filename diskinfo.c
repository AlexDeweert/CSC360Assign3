// Super block information: (0 indexed)
// Block size: 512  <- offset 8
// Block count: 5120 <-- offset 12 
// FAT starts: 1 <-- offset 16
// FAT blocks: 40 <-- offset 20
// Root directory start: 41 <-- offset 23
// Root directory blocks: 8 <-- offset 27

// FAT information:
// Free Blocks: 5071
// Reserved Blocks: 41
// Allocated Blocks: 8

#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
const int i = 1;
#define is_bigendian() ( (*(char*)&i) == 0 )

//Prototypes
void printFileSystemIdentifier( unsigned char buf[] );

int main( int argc, char** argv ) {
    //FILE *fp;
    //fp = fopen("test.img", "rb" );
    printf("arg1: %s\n", argv[1]);
    int fp = open( argv[1], O_RDWR );

    // struct stat {
    //            dev_t     st_dev;     /* ID of device containing file */
    //            ino_t     st_ino;     /* inode number */
    //            mode_t    st_mode;    /* protection */
    //            nlink_t   st_nlink;   /* number of hard links */
    //            uid_t     st_uid;     /* user ID of owner */
    //            gid_t     st_gid;     /* group ID of owner */
    //            dev_t     st_rdev;    /* device ID (if special file) */
    //            off_t     st_size;    /* total size, in bytes */
    //            blksize_t st_blksize; /* blocksize for filesystem I/O */
    //            blkcnt_t  st_blocks;  /* number of 512B blocks allocated */
    //            time_t    st_atime;   /* time of last access */
    //            time_t    st_mtime;   /* time of last modification */
    //            time_t    st_ctime;   /* time of last status change */
    //        };
    struct stat buffer;

    //On success, fstat returns 0, otherwise -1 returned
    //errnos: 
       // EACCES Search permission is denied for one of the directories in the path prefix of path.  (See also path_resolution(7).)
       // EBADF  fd is bad.
       // EFAULT Bad address.
       // ELOOP  Too many symbolic links encountered while traversing the path.
       // ENAMETOOLONG
       // path is too long.
       // ENOENT A component of path does not exist, or path is an empty string.
       // ENOMEM Out of memory (i.e., kernel memory).
       // ENOTDIR
       // A component of the path prefix of path is not a directory.
       // EOVERFLOWd refers 
       // path or fd refers to a file whose size, inode number, or number of blocks cannot be represented in, respectively, the types
       // off_t,  ino_t,  or  blkcnt_t.  This error can occur when, for example, an application compiled on a 32-bit platform without
       // -D_FILE_OFFSET_BITS=64 calls stat() on a file whose size exceeds (1<<31)-1 bytes.
    int status = fstat(fp, &buffer);
    printf("Status of buffer: %d\n", status);
    printf("Size of buffer: %zu bytes\n", (size_t)(buffer.st_size));

    // NULL indicates the OS can decide where in memory to map the file.
    // buffer.st_size: the length of the mapping
    // PROT_READ pages can be read, PROT_WRITE pages can be written
    // MAP_SHARED Share  this mapping.  Updates to the mapping are visible to other processes that map this file, and are carried through
    // to the underlying file.  The file may not actually be updated until msync(2) or munmap() is called.
    // MMAP returns a pointer to the start of the mapped file.
    void* address=mmap(NULL, buffer.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
    if (address == MAP_FAILED) {
                perror("Map source error\n");
                exit(1);
    }

    int blocksize=0;
    int blockcount=0;
    int fat_starts=0;
    int fat_blocks=0;

    size_t blocksize_offset = 8;
    size_t blocksize_width = 2;
    size_t blockcount_offset = 12;
    size_t blockcount_width = 4;
    size_t fat_starts_offset = 16;
    size_t fat_starts_width = 4;
    size_t fat_blocks_offset = 20;
    size_t fat_blocks_width = 4;

    memcpy( &blocksize, address+blocksize_offset, blocksize_width );
    memcpy( &blockcount, address+blockcount_offset, blockcount_width );
    memcpy( &fat_starts, address+fat_starts_offset, fat_starts_width );
    memcpy( &fat_blocks, address+fat_blocks_offset, fat_blocks_width );

    blocksize = htons(blocksize);
    blockcount = htons(blockcount);
    fat_starts = htons(fat_starts);
    fat_blocks = htons(fat_blocks);

    printf("Block Size: %d\n", blocksize );
    printf("Block Size HEX: 0x%08x\n", blocksize );
    printf("Block Count: %d\n", blockcount );
    printf("Block Count HEX: 0x%08x\n", blockcount );
    printf("FAT starts: %d\n", fat_starts );
    printf("FAT starts HEX: 0x%08x\n", fat_starts );
    printf("FAT blocks: %d\n", fat_blocks );
    printf("FAT blocks HEX: 0x%08x\n", fat_blocks );

    close(fp);
}
