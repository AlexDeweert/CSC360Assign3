Programming Assignment P3 README.txt
CSC 360 Winter 2018 Prof. J. PAN
Student: Alexander (Lee) DEWEERT
ID: V00855767

This is an information file regarding the use of, expected outcome, and operation of the programs in this project.

*****************************************WARNING*********************************************
After extracting the project, Please run "make" to execute Makefile commands.
Following that, the required assignment programs ./diskinfo, ./disklist, ./diskget, ./diskput
should work as required in the assignment specifications.
Also please note, that extensive input sanitizing hasn't been conducted, so the program
assumes good input as specified below.
*********************************************************************************************

1) DISKINFO

1.1) Use
The user types ./diskinfo [image] to obtain information about a FAT based disk image file.
For example, "./diskinfo test.img".

1.2) Expected outcome
One should expect to see something similar to the following:

	Super block information:
	Block size: 512
	Block count: 6400
	FAT starts: 2
	FAT blocks: 50
	Root directory start: 53
	Root directory blocks: 8

	FAT Information:
	Free Blocks: 6192
	Reserved Blocks: 50
	Allocated Blocks: 158

1.3) Operation
The program utilizes the command line arguments to specify an input disk image.
The disk image is expected to be in a particular FAT format, but it flexible on blocksize,
FAT start location, root start location.

A memory map of the disk image is created using mmap(). After the map is created the program
then assigns a global compacted "superblock" struct the address of the memory map.
This assumes that the superblock is always located at address with zero offset.

After the superblock has been populated with the disk image superblock information, information
variables are populated by calling memcpy() for each information variable, to each variable
in the superblock which describes the disk information.

The superblock information is printed out.

Armed with all of the superblock information, the FAT information can be obtained by iterating
over the FAT table location at an offset of address + FAT start location, and every 4 byte entry
is tabulated. The results are printed out.


2) DISKLIST

2.1) Use
The user types ./disklist [image] [directory path] to list all files in a specific directory.
With this version of the program only the root directory has been implemented. A future version
may include subdirectory ability but for the sake of time, the feature is not available.
For example, "./disklist test.img /".

2.2) Expected outcome
One should expect to see something similar to the following:

	F        735                      mkfile.cc 2005/11/15 12:00:00
	F       2560                        foo.txt 2005/11/15 12:00:00
	F       3940                    disk.img.gz 2009/08/04 21:11:13

2.3) Operation
The program utilizes memory mapping to map a disk image into memory using mmap(). Once the map is on memory
a superblock struct is assigned to the address pointer with a zero offset, which populates the suberblock struct
with the information necessary to be able to iterate through the disk image to retrieve the necessary information.

Once the root directory location is known, a range of low and high byte addresses is calulated. The program then
iterates through the root directory 64 bytes at a time. A directory entry is 64 bytes long, so this is simply a matter
of utilizing another global compact dir_entry_t struct and assigning to the address + current iteration location.
The directory entry struct is populated with all of the data at that location.

At each iteration step, the directory entry struct information is printed in the required format.
The printing only takes place if the directory entry status is not 0x0, which indicates no file.

3) DISKGET

3.1) Use
The user types ./diskget [image] [image directory path][image filename] [host filename] to copy an existing file from the
FAT disk image to the current host directory in which diskget is being ran from.
For example, "./diskget test.img /foo.txt foo_copy.txt".

3.2) Expected outcome
There will be no visible outcome unless the specified image file does not exist.
If the file does not exist the user will see a "File not found." message, and the program will exit.
If the file does exist, then a copy of the file will appear in the current directory.
The copied file data information should reflect the same size in bytes.

One should expect to see the following after a file is successfully copied.
(user runs "./diskget test.img /foo.txt foo_copy.txt" then "./disklist test.img /" then "ls -lah | ll")
	
[After diskget and disklist]
	F        735                      mkfile.cc 2005/11/15 12:00:00
	F       2560                        foo.txt 2005/11/15 12:00:00
	F       3940                    disk.img.gz 2009/08/04 21:11:13

[After ls lah | ll]
	-rw-rw-r-- 1 vagrant vagrant    2560 Apr  2 18:39 foo_copy.txt

Notice the byte size of both files is 2560.

3.3) Operation
The progam memory maps the image file as in DISKLIST and DISKINFO, but now also opens a file pointer on
the host machie in the current directory where the copied file will be written.

The root directory is iterated through searching for entries with a valid file status (0x3) and for
a matching filename which had been specified in argument 2 of the command line input. If not found
then the program will indicated "File not found." and exit. Otherwise, the matching file information
is copied into a directory entry structure.

The directory entry structure is then used to actually locate the target file (on the disk image) blocks within
the disk image memory mapping. An array of "block pointer references" is obtained from the File Allocation Table
based on the very first directory entry struct "starting block". The block pointer reference array is then iterated
through, and every 512 bytes from each block is copied into a file byte array which is the same size as the target file.

Once the file byte array is fully populated, it is then written byte by byte into the file that we had opened on the
host machine current directory.

4) DISKPUT

4.1) Use
The user types ./diskput [image] [current host directory filename] [target image filename] to copy an existing
file on the host machine to the image file root directory.

4.2) Expected outcome
There is no visible command line result on a successful copy. However, if the file cannot be copied due to size
constraints (not enough blocks in the FAT) the program will indicate as such and exit. Otherwise, a new file entry
will be made on the image and can be viewed with the DISKLIST command. In addition, DISKINFO should display updated
disk stats in terms of free and allocated blocks.

For example if the user types "./diskput test.img dex.txt /dex_copy.txt" and then "./disklist test.img /"
the expected outcome should be:

	F        735                      mkfile.cc 2005/11/15 12:00:00
	F       2560                        foo.txt 2005/11/15 12:00:00
	F       1222                   dex_copy.txt 2005/11/15 12:00:00
	F       3940                    disk.img.gz 2009/08/04 21:11:13

If the user wants to verify that the program successfully copies the host file to the disk image correctly, the user
should utilize DISKGET to get the copied file, then conduct a comparison with the original host file, and the previously
copied over disk image file. For example, if the user now types "./diskget test.img /dex_copy.txt dex_copy_copy.txt"
and then "diff dex.txt dex_copy_copy.txt" there should be no outcome (since diff only has outcome if the files are different).

4.3) Operation
We now utilize two memory maps. One for the disk image, and one for the host file to be copied (HF).
The program first calculates how many blocks are required for the HF with filesize/blocksize rounded up.
Then the program scans the disk image FAT for the required number of blocks, not necessarily contiguous.
The free FAT blocks "entry numbers" are put into a reference array for later use.

Once the free FAT blocks are found, the root directory entry is scanned for a free spot and updated
with all of the relevant file information such as size, name, number of blocks, and starting block.
The File Allocation Table references are then update with the "next" values of the reference blocks.
That is, the starting block is the 0th entry, but it's not copied into FAT. The next block reference 
entry is copied into the FAT entry location which the 0th entry references.

This works because the FAT essentially maps it's "entry" value to a real location on disk. So for example
if the current file has FAT entries on 3 contiguous 4 byte entries which start at the 1st, 2nd, and 3rd
FAT entries, then this means that the actual bytes of the file occupy 1*512, 2*512, and 3*512 address locations
on the disk image.

The final step is to simply memcpy() to the disk image at the correct FAT reference block locations, from the
memory mapped HF. This operation mem copies entire 512 byte chunks based on the kth block reference.

5) FINAL NOTES
This program will work on both Little Endian and Big Endian machines. This is done by first checking if the high and low
bytes within a hexedecimal 2 byte value are reversed or not, and then calling htonl, htons, ntohs, or ntohl in the correct
locations when it is required.

Path parsing had been completed for the diskget portion of the assignment, but was not fully implemented
into being able to search for other directories besides ROOT. This could be easily added as a bonus feature,
but due to end of semester time constraints, this feature was not added but it may be later on.

Alex Deweert, Mon. April 2nd, 2018 12:12 pm
(Aspiring, tired, BSc Computer Science Undergraduate at UVIC)
