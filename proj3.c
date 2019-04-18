#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct
{
	unsigned char	BS_jmpBoot[3];
	unsigned char	oem[8];

	unsigned short	BPB_BytesPerSector; //sector_size
	unsigned char	BPB_SectorsPerCluster;
	unsigned short	BPB_RsvdSecCnt; //reserved sectors
	unsigned char	BPB_NumberofFATS;
	unsigned short	BPB_RootEntCnt; //root_dir_entries
	unsigned short	BPB_TotalSectorsShort;
	unsigned char	BPB_MediaDescriptor;
	unsigned short	BPB_FATSz16; //fat_size_sectors
	unsigned short	BPB_SectorsPerTrack;
	unsigned short	BPB_NumberOfHeads;
	unsigned int	BPB_HiddenSectors;
	unsigned int	BPB_TotalSectorsLong;


	unsigned int	BPB_FATSz32;
	unsigned short	BPB_ExtFlags;
	unsigned short	BPB_FSVer;
	unsigned int	BPB_RootCluster;


	unsigned short	BPB_FSInfo;
	unsigned short	BPB_BkBootSec;
	unsigned char	BPB_Reserved[12];
	unsigned char	BS_DrvNum;
	unsigned char	BS_Reserved1;
	unsigned char	BS_BootSig;
	unsigned int	BS_VolID; 
	unsigned char	BS_VolLab[11];
	unsigned char	BS_FilSysType[8];
} __attribute__((packed)) FAT32BootBlock;

typedef struct
{
	unsigned char LDIR_Ord; //1
	unsigned char LDIR_Name1[10]; //10
	unsigned char LDIR_Attr; //1
	unsigned char LDIR_Type; //1
	unsigned char LDIR_Chksum; //1
	unsigned char LDIR_Name2[12]; //12
	unsigned short LDIR_FstClusLO; //2
	unsigned int LDIR_Name3; //4

								// size in bytes
	unsigned char name[11];		// 11 bytes
	unsigned char attr;			// 1 byte
	unsigned char ntres;		// 1 byte 
	unsigned char crtTimeTenth;	// 1 byte
	unsigned short crtTime;		// 2 bytes
	unsigned short crtDate;		// 2 bytes
	unsigned short lstAccDate;	// 2 bytes
	unsigned short fstClusHI;	// 2 bytes
	unsigned short wrtTime;		// 2 bytes
	unsigned short wrtDate;		// 2 bytes
	unsigned short fstClusLO;	// 2 bytes
	unsigned int fileSize;		// 4 bytes
} __attribute__((packed)) Dir;


typedef struct
{
	unsigned int 	LeadSig;		// 4
	unsigned char  	Reserved1[480]; // 480
	unsigned int 	StrucSig;	    // 4
	unsigned int 	FreeCount;		// 4 
	unsigned int 	NextFree;		// 4
	unsigned char 	Reserved2[12];	// 12
	unsigned int 	TrailSig;		// 4
} __attribute__((packed)) FSI;


typedef struct
{
	unsigned int current_cluster_number;
	unsigned char name[100];

	char currentpath[50][100];
	int current_cluster_path[50];
	int current;

} __attribute__((packed)) Environment;



/**************
GLOBAL VARIABLE/STRUCT DECLARATIONS
*************/
FILE *imgFile;
FAT32BootBlock BootBlock;
FSI fsi;
Environment ENV;
int FirstDataSector; 
Dir Directory;


/**************
FUNCTION DECLARATIONS
*************/
void tokenize(char *, char **);

void getInfo(long);				// info. offset gets passed in
void ls(int);					// ls 
void lsDIRNAME(int, char*);		//ls DIRNAME
int changeDir(int, char*);		// cd DIRNAME
int sizeOf(int, char*);			// size DIRNAME

void addtoPath(int, char*);		//adds to the environment
void printEnv(void);			//prints Environmetn (for debugging)
void removefromPath(void);		//removes directory from enviornment path

int find_empty_cluster();		// used by creat, mkdir, etc.

void createFile(int, char*);	// creat FILENAME
void makeDir(int, char*);		// mkdir DIRNAME
Dir zeroOutDir();
void removeFile(int, char*);			// rm FILENAME
void push_onto_stack(int *, int *, int);
void pop_from_stack(int *, int *);
void removeDir(int, char*);			// rmdir DIRNAME
void openFile(char *, char*);			// open FILENAME MODE
void closeFile(FILE);			// close FILENAME
void readFile(FILE);			// read FILENAME OFFSET SIZE
void writeFile(FILE);			// write FILENAME OFFSET SIZE STRING

int getFirstSectorofCluster(int cluster_number);


int main()
{
	char cmd[100 + 1]; // hard coded max number of commands to 100
    char* parms[10 + 1]; // hard coded max number of parameters to 10
    int cmdCount = 0;
	long offset = 0;


	// need to open image file
	imgFile = fopen("./fat32.img", "rb+");
	fread(&BootBlock, sizeof(FAT32BootBlock), 1, imgFile);
	// /media/COP4610-PROJ2/A19D-05C2 // location of mounted img

	//Calculate FirstDataSector
	FirstDataSector	= BootBlock.BPB_RsvdSecCnt	 + 
		(BootBlock.BPB_NumberofFATS* BootBlock.BPB_FATSz32);


	//SET environment stuff to root information
	ENV.current = 0;
	char myname[1];
	strcpy(myname, "/");
	
	addtoPath(BootBlock.BPB_RootCluster, myname);

	
    while(1)
	{
		cmdCount++; 				// track how many user commands entered

		char * usr = getenv("USER");
		char * machine = getenv("MACHINE");
		char * pwd = getenv("PWD");

		//Print current environment information
		printf("%s@%s:", usr, machine);
		int d;
		for (d = 0; d < ENV.current; d++)
		{
			printf("%s/", d == 0 ? "" : ENV.currentpath[d]);
		} 
		printf("> ");

		
        fgets(cmd, sizeof(cmd), stdin); // accept user input
		

        if (cmd[strlen(cmd) - 1] == '\n') 	// remove newlines... 
										// without this strcmp wont work right
            cmd[strlen(cmd) - 1] = '\0';

        // parse parms out of user input. assuming space separated
        tokenize(cmd, parms);
		
		
		if (strcmp(parms[0], "exit") == 0)
		{ 	
			// kill program
			// Safely close the program and free up any allocated resources.
			
			return 0;
		}		
		else if (strcmp(parms[0], "info") == 0)
		{ 	
			offset = BootBlock.BPB_BytesPerSector * 
								BootBlock.BPB_FSInfo;
			getInfo(offset);
		}
		else if (strcmp(parms[0], "ls") == 0)
		{ 	

			if (parms[1] != NULL && strcmp(parms[1], ".") != 0)
			{
				if (strcmp(parms[1], "..") == 0)
				{
					if(ENV.current - 2 == 0)
						ls(BootBlock.BPB_RootCluster);
					else
						ls(ENV.current_cluster_path[ENV.current - 2]);
				}
				else	
					lsDIRNAME(ENV.current_cluster_number, parms[1]);
			}
			else
				ls(ENV.current_cluster_number);
		}	
		else if (strcmp(parms[0], "cd") == 0)
		{ 
			if(parms[1] == NULL)
			{
				printf("Error: missing argument\n");
			}
			else
			{

				if (strcmp(parms[1], ".") == 0)
				{
					//do nothing
					continue;
				}
				else if (strcmp(parms[1], "..") == 0)
				{
					//delete directory at end of our path
					removefromPath();
				}
				else
				{
					
					int newclusternumber =
						changeDir(ENV.current_cluster_number, parms[1]);

					if (newclusternumber != ENV.current_cluster_number)
						addtoPath(changeDir(ENV.current_cluster_number,
								parms[1]), parms[1]);
				}
			}
		}
		else if (strcmp(parms[0], "size") == 0)
		{ 
			if(parms[1] == NULL)
			{
				printf("Error: missing argument\n");
			}
			else
			{
				printf("Size of %s: ", parms[1]);
				printf("%d\n", sizeOf(ENV.current_cluster_number, parms[1]));
			}	
		}
		else if (strcmp(parms[0], "creat") == 0)
		{ 
			if(parms[1] != NULL)
				createFile(ENV.current_cluster_number, parms[1]);
			else
				printf("Error: Missing argument\n");
		}	
		else if (strcmp(parms[0], "mkdir") == 0)
		{ 
			if(parms[1] != NULL)
				makeDir(ENV.current_cluster_number, parms[1]);
			else
				printf("Error: Missing argument\n");
		}
		else if (strcmp(parms[0], "rm") == 0)
		{ 
			if(parms[1] != NULL)
			{
				removeFile(ENV.current_cluster_number, parms[1]);
			}
			else
			{
				printf("Error: missing argument\n");	
			}
		}
		else if (strcmp(parms[0], "rmdir") == 0)
		{ 
			
			if(parms[1] != NULL)
			{
				removeDir(ENV.current_cluster_number, parms[1]);
			}
			else
			{
				printf("Error: missing argument\n");
			}	
		}
		else if (strcmp(parms[0], "open") == 0)
		{ 
				// handle open
			//printf("open entered \n");
			
			
			
			// make sure parms[2] is only (r, w, rw, or wr) 
			// before trying to open the file
			if(strcmp(parms[2], "r") == 0 || strcmp(parms[2], "w") == 0 || 
				strcmp(parms[2], "rw") == 0 || strcmp(parms[2], "wr") == 0)
				openFile(parms[1], parms[2]); // pass in file name and mode
			else
				printf("Invalid file mode provided.\n");
			
			
			
			// 3 parameters
			
			/*
			Opens a file named FILENAME in the current working directory. A file
			can only be read from or written to if it is opened first. You will
			need to maintain a table of opened files and add FILENAME to it when
			open is called MODE is a string and is only valid if it is one of
			the following:
				r = read-only
				w = write-only
				rw = read and write
				wr = write and read
			Print an error if the file is already opened, if the file does not
			exist, or an invalid mode is used.
			*/
				
			//	if (file is open)
			//		printf("Error: File already open\n");
			//	else if (file does not exist)
			//		printf("Error: File does not exist\n");
			//	else if (invalid mode used)
			//		printf("Error: Invalid mode\n");
	
		}
		else if (strcmp(parms[0], "close") == 0)
		{ 
			// handle close
			//printf("close entered \n");
		
			// 2 parameters
			
			/*
			Closes a file named FILENAME. 
			Needs to remove the file entry from the open file table.
			Print an error if the file is not opened, or if the file does not
			exist.
			*/

			//	if (file is not open)
			//		printf("Error: File is not open\n");
			//	else if (file does not exist)
			//		printf("Error: File does not exist\n");
		}		
		else if (strcmp(parms[0], "read") == 0)
		{ 	
			// handle read
			
			// 4 parameters
		
			/*
			Read the data from a file in the current working directory with the
			name FILENAME. Start reading from the file at OFFSET bytes and stop
			after reading SIZE bytes. If the OFFSET+SIZE is larger than the size
			of the file, just read size-OFFSET bytes starting at OFFSET. Print
			an error if FILENAME does not exist, if FILENAME is a directory, if
			the file is not opened for reading, or if OFFSET is larger than the
			size of the file.
			*/

			// 	if (file does not exist)
			// 		printf("Error: File does not exist\n");
			// 	else if (FILENAME is not a file)
			// 		printf("Error: Not a file\n");
			// 	else if (file not opened for reading)
			// 		printf("Error: File not open for reading\n");
			// 	else if (OFFSET > sizeOf(FILENAME))
			// 		printf("Error: Offset out of bounds\n");
		}		
		else if (strcmp(parms[0], "write") == 0)
		{ 
			// handle write
		
			// 5 parameters
			
			/*
			Writes to a file in the current working directory with the
			name FILENAME. Start writing at OFFSETbytes and stop after writing
			SIZE bytes. If OFFSET+SIZE is larger than the size of the file, you
			will need to extend the length of the file to at least hold the data
			being written. You will write STRING at this position. If STRING is
			larger than SIZE, write only the first SIZEcharacters of STRING. 
			If STRING is smaller than SIZE, write the remaining characters as
			ASCII 0 (null characters). Print an error if FILENAME does not
			exist, if FILENAME is a directory, or if the file is not opened
			for writing.
			*/

			//	if (file does not exist)
			//		printf("Error: File does not exist\n");
			//	else if (FILENAME is not a file)
			//		printf("Error: Not a file\n");
			//	else if (file not open for writing)
			//		printf("Error: File not open for writing\n");
		}		

	}

    return 0;
}


/* tokenize parses user input and 
	stores the arguments in array
	name parms
*/
void tokenize(char * cmd, char ** parms)
{       
	int i;
    for (i = 0; i < 10; i++)
	{ 	// hard coded max number of parameters to 10
        if (parms == NULL)
			break;

		parms[i] = strsep(&cmd, " ");
    }
}


/* addtoPath
	adds a cluster number and its name
	to the current runtime path to be printed
*/
void addtoPath(int current_cluster, char * name)
{

	ENV.current_cluster_number = current_cluster;
	strcpy((char *)ENV.name, name);
	strcpy(ENV.currentpath[ENV.current], name);

	ENV.current_cluster_path[ENV.current] = current_cluster;

	ENV.current++;
}


/* Removes the name and cluster number for
	the current, runtime pathname
	decrements "ENV.current" 
*/
void removefromPath(void)
{
	if (ENV.current > 1)
	{
		ENV.current_cluster_number = ENV.current_cluster_path[ENV.current-2];
		strcpy((char *)ENV.name, ENV.currentpath[ENV.current-2]);


		ENV.current--;
	}
}


/* printEnv()
	prints all environment data and paths,
	used mainly for debugging
*/
void printEnv(void)
{
	printf("Current Cluster Number: %d\n", ENV.current_cluster_number);
	printf("Current Directory Name: %s\n", ENV.name);

	int i, j;
	for (i = 0; i < ENV.current; i++)
	{
		printf("%s", ENV.currentpath[i]);
	}
	printf("\n");
	for (j = 0; j < ENV.current; j++)
	{
		printf("%d > ", ENV.current_cluster_path[j]);
	}
	printf("\n");
}


/* returns the first sector value of 
	cluster number parameter
	*/
int getFirstSectorofCluster(int cluster_number)
{
	return (((cluster_number - 2)*BootBlock.BPB_SectorsPerCluster)
			 + FirstDataSector) * BootBlock.BPB_BytesPerSector;
}


/* getInfo()
parses boot sector and prints data
 */
void getInfo(long offset)
{

	fseek(imgFile, offset, SEEK_SET);
	fread(&fsi, sizeof(FSI), 1, imgFile);

	printf("Bytes per sector: %d\n", BootBlock.BPB_BytesPerSector);
	printf("Sectors per cluster: %d\n", BootBlock.BPB_SectorsPerCluster);
	printf("Total sectors: %d\n", BootBlock.BPB_TotalSectorsLong);
	printf("Number of FATs: %d\n", BootBlock.BPB_NumberofFATS);
	printf("Sectors per FAT: %d\n", BootBlock.BPB_FATSz32);
	printf("Number of heads: %d\n", BootBlock.BPB_NumberOfHeads);
	printf("Number of hidden sectors: %d\n", BootBlock.BPB_HiddenSectors);
	printf("Number of free sectors: %d\n", fsi.FreeCount);
}


/* ls alone
	will print the contents of the current directory
	by iterating through each entry and printing its name
*/
void ls(int current_cluster_number)
{
	Dir testDir;
	int i = 0;	
	int clusternum;

	while (1)
	{
		i = 0;
		while (i*sizeof(testDir) < BootBlock.BPB_BytesPerSector)
		{
			int offset = getFirstSectorofCluster(current_cluster_number) 
								+ i*sizeof(testDir);

			fseek(imgFile, offset, SEEK_SET);
			fread(&testDir, sizeof(Dir), 1, imgFile);

			if (strcmp((char *)testDir.name, "") != 0)
				printf("%s\n", testDir.name); 
			i++; 
		} 
	
		fseek(imgFile, 0x4000 + (4*current_cluster_number), SEEK_SET);
		fread(&clusternum, sizeof(int), 1, imgFile);

		if (clusternum == 0x0FFFFFF8 || 
			clusternum == 0x0FFFFFFF || 
			clusternum == 0x00000000)
			break;
		else
		{
			current_cluster_number = clusternum;
		}
	}
}


/* ls DIRNAME
	calls cd and ls
	calls changeDir to find the cluster number of DIRNAME
	then calls ls on the cluster number returned
*/
void lsDIRNAME(int current_cluster_number, char * DIRNAME)
{
	//save current info
	int cluster_returned = changeDir(current_cluster_number, DIRNAME);
	if (cluster_returned != current_cluster_number)
		ls(cluster_returned);
	//calls cd
}


/* cd DIRNAME
	operates similarly to ls, will iterate until we find
	the DIRNAME we are seeking and returns the cluster_number
	associated with it
*/
int changeDir(int current_dir_cluster_num, char * DIRNAME)
{
	Dir testDir;
	int save_cluster_num = current_dir_cluster_num;
	int i = 0;	
	int clusternum;

	while (1)
	{
		i = 0;
		while (i*sizeof(testDir) < BootBlock.BPB_BytesPerSector)
		{
			int offset = getFirstSectorofCluster(current_dir_cluster_num)
						 + i*sizeof(testDir);

			fseek(imgFile, offset, SEEK_SET);
			fread(&testDir, sizeof(Dir), 1, imgFile);
	
			int isDir = testDir.attr&0x10;
			if (strstr((char *)testDir.name, DIRNAME) != NULL  && isDir == 0x10)
			{
				return testDir.fstClusHI * 0x100 + testDir.fstClusLO;
			}
			i++;
		}
		
		fseek(imgFile, 0x4000 + (4*current_dir_cluster_num), SEEK_SET);
		fread(&clusternum, sizeof(int), 1, imgFile);

		if (clusternum == 0x0FFFFFF8 || 
			clusternum == 0x0FFFFFFF || 
			clusternum == 0x00000000)
		{
			printf("Error: Not a directory\n");
			break;
		}
		else
		{
			current_dir_cluster_num = clusternum;
		}
	}
	return save_cluster_num;
}


/* size DIRNAME
	operates  similar to cd/ls functions,
	simply iterates through the current directory
	until we find the one we are seeking and returns
	that DIRNAME/FILENAME size 
*/
int sizeOf(int current_dir_cluster_num, char * DIRNAME)
{
	Dir testDir;
	int save_cluster_num = current_dir_cluster_num;
	int i = 0;	
	int clusternum;

	while (1)
	{
		i = 0;
		while (i*sizeof(testDir) < BootBlock.BPB_BytesPerSector)
		{
			int offset = getFirstSectorofCluster(current_dir_cluster_num)
						 + i*sizeof(testDir);

			fseek(imgFile, offset, SEEK_SET);
			fread(&testDir, sizeof(Dir), 1, imgFile);

			if (strstr((char *)testDir.name, DIRNAME) != NULL) 
			{
				return testDir.fileSize;
			}
			i++;
		}
		
		fseek(imgFile, 0x4000 + (4*current_dir_cluster_num), SEEK_SET);
		fread(&clusternum, sizeof(int), 1, imgFile);

		if (clusternum == 0x0FFFFFF8 
			|| clusternum == 0x0FFFFFFF 
			|| clusternum == 0x00000000)
		{
			printf("Error: Not a directory\n");
			break;
		}
		else
		{
			current_dir_cluster_num = clusternum;
		}
	}
	return 0;
}


/* find_empty_cluster() returns the index of
	the FAT table at which we found the next
	empty cluster, indexed by 0x4000 + (4*i)
*/
int find_empty_cluster()
{
	int i;

	for (i = 0; 0x4000+(4*i) < FirstDataSector 
			*BootBlock.BPB_BytesPerSector; i++)
	{
		int clusternum;
		fseek(imgFile, 0x4000 + (4*i), SEEK_SET);
		fread(&clusternum, sizeof(int), 1, imgFile);

		if (clusternum == 0x00000000)
		{
			return i;
		}
	}
	return -1;
}


/* mkdir DIRNAME
 iterates through current cluster to find space
 if no space is found, calls, find_empty_cluster
 and attempts to allocate a new directory entry

 bugs: 	does not properly assign FAT table entries
 		for entries after the first directory created
 		. and .. entries are not created but are
 		handled elsewhere in the code and still work
 		and can be used
 */
void makeDir(int current_dir_cluster_num, char * DIRNAME)
{
	Dir testDir;
	int pass_cluster = current_dir_cluster_num;
	int i = 0;	
	int clusternum;
	int foundSpace;
	int found_at_offset;

	while (1)
	{
		foundSpace = 0;
		i = 0;
		while (i*sizeof(testDir) < BootBlock.BPB_BytesPerSector)
		{

			int offset = getFirstSectorofCluster(current_dir_cluster_num)
						 + i*sizeof(testDir);

			fseek(imgFile, offset, SEEK_SET);
			fread(&testDir, sizeof(Dir), 1, imgFile);

			//check if this is empty
			if (testDir.name[0] == 0xE5 || testDir.name[0] == 0x00)
			{
				foundSpace = 1;
				found_at_offset = offset;
				int found_at_cluster_num = current_dir_cluster_num;
				break;
			}

			i++;
		}

		if(foundSpace == 1)
			break;

		//if we make it to this point, we found no empty data entries in this cluster
		//so we check where the next cluster is. 
		
		fseek(imgFile, 0x4000 + (4*current_dir_cluster_num), SEEK_SET);
		fread(&clusternum, sizeof(int), 1, imgFile);

		if (clusternum == 0x0FFFFFF8 || 
			clusternum == 0x0FFFFFFF || 
			clusternum == 0x00000000)
		{
			foundSpace = 0;
			break;
		}
		else
		{
			//the cluster continues, so we go on to check
			current_dir_cluster_num = clusternum;
		}
	}

	if(foundSpace == 1)
	{
		//allocate it!
		Dir newDir;

		strcpy(newDir.name, DIRNAME);
		newDir.attr = 0x10;

		newDir.fileSize = 0;

		int empty = find_empty_cluster();
		newDir.fstClusHI = empty / 0x100;
		newDir.fstClusLO = empty % 0x100;

		fseek(imgFile, found_at_offset, SEEK_SET);
		fwrite(&newDir, 1, sizeof(Dir), imgFile);
		foundSpace = 1;

	}
	else //did not find space
	{
		Dir addDir;
		int i = find_empty_cluster();

		int setthis = 0x0FFFFFF8; //FAT[i] = 0FFFFFFF8

		if(i != -1)
		{
			fseek(imgFile, 0x4000 + (4*i), SEEK_SET);
			fwrite(&setthis, 1, sizeof(int), imgFile);
			
			fseek(imgFile, 0x4000 + (4*pass_cluster), SEEK_SET);
			fwrite(&i, 1, sizeof(int), imgFile);

			int new_offset = getFirstSectorofCluster(i);
				
			strcpy(addDir.name, DIRNAME);
			addDir.attr = 0x10;
			addDir.fileSize = 0;

			int new_empty = find_empty_cluster();
			addDir.fstClusHI = new_empty / 0x100;
			addDir.fstClusLO = new_empty % 0x100;

			fseek(imgFile, new_offset, SEEK_SET);
			fwrite(&addDir, 1, sizeof(Dir), imgFile);
		}
	}
}

/* creat FILENAME
operates exactly like mkdir except does not allocate
the necessary directory information.

bugs: 	same thing for mkdir, however . and .. entries 
		are no longer a factor since we create files here
*/
void createFile(int current_dir_cluster_num, char * DIRNAME)
{

	Dir testDir;
	int pass_cluster = current_dir_cluster_num;

	int i = 0;	
	int clusternum;
	int foundSpace;
	int found_at_offset;

	while (1)
	{
		foundSpace = 0;
		i = 0;
		while (i*sizeof(testDir) < BootBlock.BPB_BytesPerSector)
		{

			int offset = getFirstSectorofCluster(current_dir_cluster_num)
						 + i*sizeof(testDir);

			fseek(imgFile, offset, SEEK_SET);
			fread(&testDir, sizeof(Dir), 1, imgFile);

			if (testDir.name[0] == 0xE5 || testDir.name[0] == 0x00)
			{

				foundSpace = 1;
				found_at_offset = offset;
				int found_at_cluster_num = current_dir_cluster_num;
				break;
			}
			i++;
		}

		if(foundSpace == 1)
			break;

		fseek(imgFile, 0x4000 + (4*current_dir_cluster_num), SEEK_SET);
		fread(&clusternum, sizeof(int), 1, imgFile);

		if (clusternum == 0x0FFFFFF8 || 
			clusternum == 0x0FFFFFFF || 
			clusternum == 0x00000000)
		{

			foundSpace = 0;
			break;
		}
		else
		{
			//the cluster continues, so we go on to check
			current_dir_cluster_num = clusternum;
		}
	}
	

	if(foundSpace == 1)
	{
		//allocate it!
		Dir newDir;

		strcpy(newDir.name, DIRNAME);
		newDir.attr = 0;

		newDir.fileSize = 0;

		int empty = find_empty_cluster();
		newDir.fstClusHI = empty / 0x100;
		newDir.fstClusLO = empty % 0x100;
		

		fseek(imgFile, found_at_offset, SEEK_SET);
		fwrite(&newDir, 1, sizeof(Dir), imgFile);
		foundSpace = 1;
	}
	else
	{
		Dir addDir;
		Dir um;
		int i = find_empty_cluster();

		int setthis = 0x0FFFFFF8;
		if(i != -1)
		{
		
			fseek(imgFile, 0x4000 + (4*i), SEEK_SET);
			fwrite(&setthis, 1, sizeof(int), imgFile);

			fseek(imgFile, 0x4000 + (4*pass_cluster), SEEK_SET);
			fwrite(&i, 1, sizeof(int), imgFile);
	
			int new_offset = getFirstSectorofCluster(i);
	
			strcpy(addDir.name, DIRNAME);
			addDir.attr = 0;
			addDir.fileSize = 0;

			int new_empty = find_empty_cluster();
			addDir.fstClusHI = new_empty / 0x100;
			addDir.fstClusLO = new_empty % 0x100;

			fseek(imgFile, new_offset, SEEK_SET);
			fwrite(&addDir, 1, sizeof(Dir), imgFile);
		}
	}

}

// rm FILENAME
void removeFile(int current_dir_cluster_num, char * DIRNAME)
{
	int startSize = 0;
	int * size = &startSize;
	int * stack = malloc(1000 * sizeof(int));

	Dir testDir;

	int save_cluster_num = current_dir_cluster_num;

	int i = 0;	
	int clusternum = -1;
	int firstCluster;
	int saveIndex;

	while (1)
	{
		i = 0;
		while (i*sizeof(Dir) < BootBlock.BPB_BytesPerSector)
		{

			int offset = getFirstSectorofCluster(current_dir_cluster_num)
						 + i*sizeof(Dir);

			fseek(imgFile, offset, SEEK_SET);
			fread(&testDir, sizeof(Dir), 1, imgFile);

			if (strstr((char *)testDir.name, DIRNAME) != NULL)
			{
				firstCluster = (testDir.fstClusHI * 0x100) + testDir.fstClusLO;
				break;
			}
			i++;
		}

		if (clusternum == 0x0FFFFFF8 ||
			clusternum == 0x0FFFFFFF)
		{
			printf("Error: Not a file\n");
			break;
		}
		else
		{
			i = 0;
			while (clusternum != 0)
			{
				int offset = i * sizeof(int);

				fseek(imgFile,
				getFirstSectorofCluster(firstCluster) + offset, SEEK_SET);

				fread(&clusternum, 1, sizeof(int), imgFile);

				push_onto_stack(stack, size, clusternum);
			}
			
			while (*size != 0)
			{
				int x = 0x00000000;
				fseek(imgFile, getFirstSectorofCluster(stack[0]), SEEK_SET);

				fwrite(&x, sizeOf(current_dir_cluster_num, DIRNAME) /
						   sizeof(int), sizeof(int), imgFile);

				pop_from_stack(stack, size);
			}

			int x = 0x00000000;
			int offset = 0x1E0;

			fseek(imgFile,
			getFirstSectorofCluster(save_cluster_num) + offset, SEEK_SET);

			fwrite(&x, 1, sizeof(int), imgFile);
			break;
		}
	}

	free(stack);
}

void push_onto_stack(int * s, int * size, int value)
{
	(*size)++;

	int i;
	for (i = *size - 1; i > 0; i--)
		s[i] = s[i - 1];
	
	s[0] = value; 
}

void pop_from_stack(int * s, int * size)
{
	int i;
	for (i = 0; i < *size - 1; i++)
		s[i] = s[i + 1];

	(*size)--;
}


// rmdir DIRNAME
void removeDir(int current_dir_cluster_num, char * DIRNAME)
{
	Dir testDir;

	int save_cluster_num = current_dir_cluster_num;

	int i;
	int clusternum;
	int firstCluster;
	int saveIndex;
	int OFFSET = 166;

	while (1)
	{
		i = 0;
		while (i*sizeof(testDir) < BootBlock.BPB_BytesPerSector)
		{

			int offset = getFirstSectorofCluster(current_dir_cluster_num)
						 + i*sizeof(testDir);

			fseek(imgFile, offset, SEEK_SET);
			fread(&testDir, sizeof(Dir), 1, imgFile);

			if (strstr((char *)testDir.name, DIRNAME) != NULL &&
				testDir.attr == 0x10)
			{
				firstCluster = (testDir.fstClusHI * 0x100) + testDir.fstClusLO;
				break;
			}

			i++;
		}

		fseek(imgFile, getFirstSectorofCluster(firstCluster) + 0x40, SEEK_SET);
		fread(&clusternum, sizeof(int), 1, imgFile);

		if (clusternum == 0x0FFFFFF8 ||
			clusternum == 0x0FFFFFFF)
		{
			printf("Error: Not a directory\n");
			break;
		}
		else if (clusternum != 0x00)
		{
			printf("Error: Directory not empty\n");
			break;
		}
		else
		{
			current_dir_cluster_num = clusternum;
			testDir = zeroOutDir();

			fseek(imgFile, getFirstSectorofCluster(firstCluster), SEEK_SET); 
			fwrite(&testDir, 2, sizeof(Dir), imgFile);

			int x = 0x00000000;
			int offset = 0x1E0;

			fseek(imgFile,
			getFirstSectorofCluster(save_cluster_num) + offset, SEEK_SET);
			fwrite(&x, 1, sizeof(int), imgFile);
			break;
		}
	}
}

Dir zeroOutDir()
{
	Dir testDir;

	testDir.LDIR_Ord = '\0';
	testDir.LDIR_Name1[0] = 0;
	testDir.LDIR_Attr = '\0';
	testDir.LDIR_Type = '\0';
	testDir.LDIR_Chksum = '\0';
	testDir.LDIR_Name2[0] = 0;
	testDir.LDIR_FstClusLO = 0;
	testDir.LDIR_Name3 = 0;

	testDir.name[0] = 0;
	testDir.attr = '\0';
	testDir.ntres = '\0';
	testDir.crtTimeTenth = '\0';
	testDir.crtTime = 0;
	testDir.crtDate = 0;
	testDir.lstAccDate = 0;
	testDir.fstClusHI = 0;
	testDir.wrtTime = 0;
	testDir.wrtDate = 0;
	testDir.fstClusLO = 0;
	testDir.fileSize = 0;
	
	return testDir;
}

// open FILENAME MODE
void openFile(char* fileName, char* fileMode)
{
	
	//int tempFirstClusterNumber = 0;
	
	// need an array/linked list of structs (of struct openFileInfo) 
		//that will maintain all currently opened files
	// openFileInfo[15] should be handling this...
	
	
	// need to load Directory struct with info from fileName......
	// seek/fread?
	long tempOffset = 0;
	
	// need to calculate proper offset here
	tempOffset = 400; // ? unsure of proper offset..
	
	
	// need to do this in a loop for each file?
	fseek(imgFile, tempOffset, SEEK_SET);
	fread(&Directory, sizeof(Dir), 1, imgFile);

	
	// check if fileName is present in current directory
	// if(fileName not in current dir)	// check if fileName is present in current directory
		// printf("File does not exist in current directory!\n");
		// return;

	if(Directory.attr&0x10 == 0x00){ // this means is NOT a directory 
		// check if fileName is not a directory (DIR_Attr&0x10 == 0x00)
				
		// then this is valid filename and we should open it!
		printf("Not a directory.\n");
		
		
		if(Directory.attr&0x01 == 0x01){ // if fileName is READ_ONLY	
			//printf("File is read only.\n");
			if(strcmp(fileMode, "w") == 0 || strcmp(fileMode, "rw") == 0 
				|| strcmp(fileMode, "wr") == 0) 
				// if read only and user trying to open with 
				// write perms, print error
				printf("Error: Trying to open READ ONLY file with write permissions.\n");
				
		}
		//else // fileName is NOT READ_ONLY
		// do we have to check any other permission 
			// conditions before openening? Don't think so.. 
		//
			// need to get first cluster number of fileName from its Directory Entry
			
			// need to check if fileNames first cluster number is already present in 
				//array/linked list of currently open files
			//if(fileNames first_cluster_number found in array)
				//printf("File is already opened.\n");
			//else
				//if(Directory.attr&0x01 == 0x01) // if fileName is READ_ONLY	
					//if(strcmp(fileMode, "r") == 0) // if fileMode 
						//is also READ_ONLY
						//append array/linked list with fileNames
							//first_cluster_number and fileMode

				//else if(Directory.attr&0x01 != 0x01) 
					// if fileName is NOT READ_ONLY	
					//append array/linked list with fileNames 
						//first_cluster_number and fileMode
				
				//else // otherwise file is READ_ONLY and trying to be 
					//opened in RW/WR/W mode, is a directory, or doesnt exist so print error
					// printf("Error opening file.\n");
		//printf("File isn't read only\n");
	
	
	}
//	else
	//	printf("File is a directory\n"); 
	
	/*
		Part 10: open FILENAME MODE
		Opens a file named FILENAME in the current working directory. 
			A file can only be read from or
		written to if it is opened first. You will need to maintain a 
			table of opened files and add FILENAME to
		it when open is called
		MODE is a string and is only valid if it is one of the following:
		• r – read-only
		• w – write-only
		• rw – read and write
		• wr – write and read
		Print an error if the file is already opened, if the file does 
		not exist, or an invalid mode is used.
	*/


}

// close FILENAME
void closeFile(FILE f)
{
	
	// shouldnt be too bad when openFile is working. will need to search 
		//through the table of opened files 
	// (thats created by openFile and if f is found remove it from the 
		//table, else print error message saying file not open 
	/*
	Check if FILENAME is in current directory (to get the first_cluster_number)
	•No need to check if FILENAME are opened in right mode (you did that
		while opening remember)?
	•Check if linkedlist/array has the first_cluster_number of FILENAME in it.
	•Delete that entry 
	*/
	
	// if(f is in current dir)
		// get first_cluster_number
	// check array of structs openFileInfo and see if there is match for f 
		//in this array
	// if (match)
		// delete that_entry;
	// else
		// print error message printf("File not open\n");

}

// read FILENAME OFFSET SIZE
void readFile(FILE f)
{
	// need to print all data in file as a string
	
	// need to make char array dynamically of size sector_size
		// fread data from cluster into this array 
		// puts (char array)
	
	
	// then go to first cluster num 
		// verify it is opened (and file mode is valid (r, w, rw, wr)
		// read whole cluster and print it out
		// then print data in next cluster. continue this until EOF
	
	// need to handle odd edge cases...
		// like offset wont always be perfectly divisible by sector size
		
	
	// calculate offset, divide this by sector size and set
		//quotient = temp variable "n"
		// this will be the "nth" cluster that we wil read from
	// while(n != 0){
	//	cluster_number = FAT32[cluster_number];
	//	n = n - 1;
	// }
	
	// if(cluster_number == 0x0000000 || cluster_number == 0xFFFFFFF8)
		// printf("Error");
	// else 
		// we are in nth cluster (NEED TO READ THIS IN!)
		// fseek(imgFile, nthStartingPoint + (offset%sector_size), SEEK_SET);
		//fread(&Dir, sizeof(Dir), 1, imgFile);
		// print it out..
		// handle even more special edge cases here.. 
			//(such as OFFSET < fileName size)	
}

// write FILENAME OFFSET SIZE STRING
void writeFile(FILE f)
{

	// start off same as readFile 
	// will have to go the nth cluster and set offset%sector_size
	// fwrite string into file f

	// many annoying edge cases to handle..
	// like removing clusters/allocating clusters..
	
		
}



