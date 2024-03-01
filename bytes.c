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

Bytes * newBytesFromFile(char * fileName) {
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
//  DELETEBYTES - delete data allocated using newBytesFromFile
//----------------------------------------------------------------------------

Bytes * deleteBytes(Bytes * b) {
	if(b != NULL) {
		free(b);
		b = NULL;
	}
	return b;
}
