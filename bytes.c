// bytes
// a struct and wrapper around malloc/free that stores size

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "types.h"
#include "bytes.h"


//----------------------------------------------------------------------------
//  NEWBYTESFROMFILE - read entire file into memory into a Bytes struct
//----------------------------------------------------------------------------

Bytes * newBytesFromFile(const char * fileName) {
	// Open the binary input file
    FILE * f = fopen(fileName, "rb");
    if(f==NULL) {
		printf("ERROR: Failed to open input file: '%s'\n", fileName);
		return NULL;
    }

	// Check file size
    fseek(f,0,SEEK_END);
	long ftr = ftell(f);
	fseek(f,0,SEEK_SET);
	size_t fileSize = (size_t)ftr;

	// Allocate buffer
	Bytes * b = (Bytes *)malloc(sizeof(Bytes)+fileSize);
	if(b == NULL) {
		printf("ERROR: Unable to allocate enough memory to open file.\n");
		fclose(f);
		return NULL;
	}

	b->size = fileSize;	
 	// Read whole file
	if(fread(b->data, 1, fileSize, f) != fileSize) {
		printf("ERROR: Read failed.\n");
		fclose(f);
		free(b);
		return NULL;
	}

	// Now file is loaded, close the file
	fclose(f);

	// Return the allocated memory filled with the file data
	return b;
}


//----------------------------------------------------------------------------
//  NEWBYTESFROMMEMORY - clone bytes from memory into a Bytes struct
//----------------------------------------------------------------------------

Bytes * newBytesFromMemory(const u8 * data, size_t size) {
	// Allocate buffer
	Bytes * b = (Bytes *)malloc(sizeof(Bytes)+size);
	if(b == NULL) {
		printf("ERROR: Unable to allocate enough memory.\n");
		return NULL;
	}

	b->size = size;	
	memcpy(b->data, data, size);	

	// Return the allocated memory filled with the data
	return b;
}


//----------------------------------------------------------------------------
//  DELETEBYTES - delete data allocated using newBytesFromFile
//----------------------------------------------------------------------------

Bytes * deleteBytes(Bytes * b) {
	if(b != NULL) {
		free(b);
		b = NULL;
	}
	return b;
}

//----------------------------------------------------------------------------
//  SAVEBYTESTOFILE - save blob to file
//----------------------------------------------------------------------------

int saveBytesToFile(const Bytes * b, const char * fileName) {
	// open the dump file
	FILE * dumpFile = fopen(fileName, "wb");
	if(dumpFile == NULL) {
		return 1;	// FAILED
	}
	int idx = 0;
	size_t length = b->size;

	// write the data to the dump file
	while(length > 0) {
		size_t bytesToWrite = 4096;
		if(length < bytesToWrite) {
			bytesToWrite = length;
		}

		size_t rval = fwrite(&b->data[idx],1,bytesToWrite,dumpFile);
		if(rval != bytesToWrite) {
			fclose(dumpFile);
			remove(fileName);
			return 2;	// FAILED
		}
		idx += bytesToWrite;
		length -= bytesToWrite;
	}

	// close the dump file
	fclose(dumpFile);

	return 0; // SUCCESS
}