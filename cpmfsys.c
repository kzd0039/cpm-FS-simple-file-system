#include <stdint.h> 
#include <stdlib.h> 
#include "cpmfsys.h"
#include  <stdbool.h> 
#include <string.h> 
#include <stdio.h> 

bool FreeList[256];
int numOfDir = 32;
uint8_t dirBlockNumber = 0;
uint8_t dirBuffer[BLOCK_SIZE];
//function to allocate memory for a DirStructType (see above), and populate it, given a
//pointer to a buffer of memory holding the contents of disk block 0 (e), and an integer index
// which tells which extent from block zero (extent numbers start with 0) to use to make the
// DirStructType value to return. 
// e points to in-memory block0
DirStructType *mkDirStruct(int index,uint8_t *e) {
  DirStructType *dir;
  uint8_t *dirBuffer = e;

  // create memory space for dir
  dir = malloc(sizeof(DirStructType));

  // copy status from block0
  dir -> status = dirBuffer[index];
  // printf("%d\n", dir -> status);

  // copy name
  char blank = 32;
  int i = 0, j = 1;
  while (j < 9) {
    if (dirBuffer[index + j] == blank) {
      (dir -> name)[i] = '\0';
      break;
    }
    (dir -> name)[i++] = dirBuffer[index + j++];
  }
  // printf("%s\n", dir -> name);


  // copy extension
  i = 0, j = 9;
  while (j < 12) {
    if (dirBuffer[index + j] == blank) {
      (dir -> extension)[i] = '\0';
      break;
    }
    (dir -> extension)[i++] = dirBuffer[index + j++];
  }
  // printf("%s\n", dir -> extension);

  // copy XL, BC, XH, RC
  dir -> XL = dirBuffer[index + 12];
  dir -> BC = dirBuffer[index + 13];
  dir -> XH = dirBuffer[index + 14];
  dir -> RC = dirBuffer[index + 15];

  // copy blocks
  i = 0, j = 16;
  while (j < 32) (dir -> blocks)[i++] = dirBuffer[index + j++];

  return dir;
}

// function to write contents of a DirStructType struct back to the specified index of the extent
// in block of memory (disk block 0) pointed to by e
void writeDirStruct(DirStructType *d, uint8_t index, uint8_t *e) {
  uint8_t *dirBuffer = e;
  // write status
  dirBuffer[index] = d -> status;
  
  // write name
  char blank = 32;
  int i = 0, nameIndex = index + 1;
  while (i < strlen(d -> name)) dirBuffer[nameIndex++] = (d -> name)[i++];
  while (i++ < 9) dirBuffer[nameIndex++] = blank;

  // write extension
  i = 0;
  int extensionIndex = index + 9;
  while (i < strlen(d -> extension)) dirBuffer[extensionIndex++] = (d -> extension)[i++];
  while (i++ < 12) dirBuffer[extensionIndex++] = blank;

  // write XL, BC, XH, RC
  dirBuffer[index + 12] = d -> XL;
  dirBuffer[index + 13] = d -> BC;
  dirBuffer[index + 14] = d -> XH;
  dirBuffer[index + 15] = d -> RC;

  // write blocks
  i = 0;
  int j = 16;
  while (j < 32) dirBuffer[index + j++] = (d -> blocks)[i++];

}

// populate the FreeList global data structure. freeList[i] == true means 
// that block i of the disk is free. block zero is never free, since it holds
// the directory. freeList[i] == false means the block is in use. 
void makeFreeList() {
  int i, j;
  blockRead(dirBuffer, dirBlockNumber);

  for (i = 0; i < NUM_BLOCKS; i++) FreeList[i] = true;
  FreeList[0] = false;

  for (i = 0; i < BLOCK_SIZE; i += EXTENT_SIZE) {
    DirStructType *dir = mkDirStruct(i, dirBuffer);
    if (dir -> status == 0xe5) continue; 
    for (j = 0; j < 16; j++) {
      if ((dir -> blocks)[j] != 0) FreeList[(int) (dir -> blocks)[j]] = false;
    }
  }
}


// debugging function, print out the contents of the free list in 16 rows of 16, with each 
// row prefixed by the 2-digit hex address of the first block in that row. Denote a used
// block with a *, a free block with a . 
void printFreeList() {
  printf("FREE BLOCK LIST: (* means in-use)\n");
  int i;
  for (i = 0; i < NUM_BLOCKS; i++) { 
    if (i % 16 == 0) { 
      fprintf(stdout,"%3x: ",i); 
    }
    if (FreeList[i]) {
      fprintf(stdout, " . ");
    } else {
       fprintf(stdout, " * ");
    }
    if (i % 16 == 15) { 
      fprintf(stdout,"\n"); 
    }
  }
}

// internal function, returns -1 for illegal name or name not found
// otherwise returns extent nunber 0-31
int findExtentWithName(char *name, uint8_t *block0) {
  //check legal file_name
  if (!checkLegalName(name)) return -1;

  //split the name into file_name, ext_name
  char file_name[9], ext_name[4];
  int i = 0, index = 0;
  while (i < strlen(name) && name[i] != '.' ) file_name[index++] = name[i++];

  file_name[index] = '\0';
  i++;
  index = 0;
  while (i < strlen(name)) ext_name[index++] = name[i++];
   
  ext_name[index] = '\0';
  blockRead(dirBuffer, dirBlockNumber);
  for (i = 0; i < BLOCK_SIZE; i += EXTENT_SIZE) {
    DirStructType *dir = mkDirStruct(i, dirBuffer);
    if (dir -> status != 0xe5 && strcmp(dir -> name, file_name) == 0 && 
          strcmp(dir -> extension, ext_name) == 0) return i;
  }
  return -1;
}

// internal function, returns true for legal name (8.3 format), false for illegal
// (name or extension too long, name blank, or  illegal characters in name or extension)
bool checkLegalName(char *name) {
  int i = 0, indexOfDot = 0, numOfDots = 0;
  while (name[i] != '\0') {
    if (name[i] == '.') {
      numOfDots++;
      indexOfDot = i;
      i++;
      continue;
    }
    if (!( (name[i] >= 'a' && name[i] <= 'z') 
        || (name[i] >= 'A' && name[i] <= 'A') 
        || (name[i] >= '0' && name[i] <= '9'))) return false;
    i++;
  }
  if (numOfDots == 0) {
    if (i > 7) return false;
  } else {
    if (numOfDots > 1 || indexOfDot == 0 || indexOfDot > 8 || (i - indexOfDot) > 4) return false;
  }

  return true;
}


// print the file directory to stdout. Each filename should be printed on its own line, 
// with the file size, in base 10, following the name and extension, with one space between
// the extension and the size. If a file does not have an extension it is acceptable to print
// the dot anyway, e.g. "myfile. 234" would indicate a file whose name was myfile, with no 
// extension and a size of 234 bytes. This function returns no error codes, since it should
// never fail unless something is seriously wrong with the disk 
void cpmDir() {
  printf("DIRECTORY LISTING\n");
  blockRead(dirBuffer, dirBlockNumber);
  int i;
  for (i = 0; i < BLOCK_SIZE; i += EXTENT_SIZE) {
    DirStructType *dir = mkDirStruct(i, dirBuffer);
    if (dir -> status != 0xe5) {
      int NB = 0, j;
      for (j = 0; j < 16; j++) if ((dir -> blocks)[j] != 0) NB++;
      printf("%s.%s %d\n", dir -> name, dir -> extension, 
          1024 * (NB - 1) + 128 * dir -> RC + dir -> BC);
    }
  }
}

// error codes for next five functions (not all errors apply to all 5 functions)  
/* 
    0 -- normal completion
   -1 -- source file not found
   -2 -- invalid  filename
   -3 -- dest filename already exists 
   -4 -- insufficient disk space 
*/ 

//read directory block, 
// modify the extent for file named oldName with newName, and write to the disk
int cpmRename(char *oldName, char * newName) {
  if (strcmp(oldName, newName) == 0) return -1;
  int index = findExtentWithName(oldName, dirBuffer);
  if (index == -1) return -2;
  if (findExtentWithName(newName, dirBuffer) != -1) return -3;

  int i = 0, nameIndex = index + 1;
  while (newName[i] != '\0') {
    if (newName[i] == '.') {
      i++;
      while (nameIndex < index + 9) dirBuffer[nameIndex++] = 32;
      continue;
    }
    dirBuffer[nameIndex++] = newName[i++];
  }
  while (nameIndex < index + 12) dirBuffer[nameIndex++] = 32;
  blockWrite(dirBuffer, dirBlockNumber);
  return 0;
}

// delete the file named name, and free its disk blocks in the free list 
int cpmDelete(char *name) {
  int index = findExtentWithName(name, dirBuffer);
  if (index == -1) return -1;
  uint8_t nullBlock[1024];

  DirStructType *dir = mkDirStruct(index, dirBuffer);
  // erase contents
  int j;
  for (j = 0; j < 16; j++) {
    if ((dir -> blocks)[j] != 0) {
      blockWrite(nullBlock, (dir -> blocks)[j]);
      FreeList[(int) (dir -> blocks)[j]] = true;
    }
  }

  // erase directory
  dirBuffer[index] = 0xe5;
  blockWrite(dirBuffer, dirBlockNumber);
  return 0;
}