// dump.c
// dump image data to file

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "types.h"
#include "bytes.h"
#include "bmp.h"
#include "dump.h"

// dump raw compressed image data
static int dumpImageRaw(const char * filename, u8 * srcData, const size_t height) {
	// Calculate the size of the data when the image header is offsets + sizes
	size_t lastHeaderEntry = (height * 4) - 4;
	const u8 * lastHeaderEntryPtr = &srcData[lastHeaderEntry];
	u16 lastOffset = get_u16(lastHeaderEntryPtr);		// Not sure how these offsets work yet
	u16 lastSize = get_u16(&lastHeaderEntryPtr[2]);
	// The size is stored as a multiple of 32!. This suggests the lowest five bits may be used for something else.
	if((lastSize&0x001F) != 0) {
		printf("ERROR: Image size has unexpected data in the bottom 5 bits! 0x%02X\n", lastSize&0x001F);
		return 1;
	}
	size_t imageSize = lastOffset + (lastSize / 32);
	printf("Dumping %s (%zu bytes) ... ", filename, imageSize);	
	
	int r = dumpBlob(filename, srcData, imageSize);
	if(r!=0) {
		printf("ERROR: dumpImage failed (%d)\n", r);
		return 1;
	}
	printf("OK\n");
	return 0;
}

// dump semi-raw decompressed image data
static int dumpImageSemi(const char * filename, u8 * srcData, const size_t width, const size_t height) {
	// Calculate the size of the data when the image header is offsets + sizes
	size_t headerSize = height * 4;
	const u8 * lastHeaderEntryPtr = &srcData[headerSize - 4];
	u16 lastOffset = get_u16(lastHeaderEntryPtr);
	u16 lastSize = get_u16(&lastHeaderEntryPtr[2]);
	
	// The size is stored as a multiple of 32!. This suggests the lowest five bits may be used for something else.
	if((lastSize&0x001F) != 0) {
		printf("ERROR: Image size has unexpected data in the bottom 5 bits! 0x%02X\n", lastSize&0x001F);
		return 1;
	}
	size_t imageSize = lastOffset + (lastSize / 32) - headerSize;

	printf("Dumping %s into semi-RAW format ... ", filename);	

	Img srcImg;
	srcImg.w = width;
	srcImg.h = height;
	srcImg.format = IF_RLE_NEW;
	srcImg.data = &srcData[headerSize];
	srcImg.size = imageSize;

	Img * img = cloneImg(&srcImg);

	// convert it to a semi-raw format
	img = convertImg(img, IF_ARGB8565);
	if(img==NULL) {
		printf("ERROR: Failed to convertImg in dumpImageSemi\n");
		deleteImg(img);
	}
	
	int r = dumpBlob(filename, img->data, img->size);
	if(r != 0) {
		printf("ERROR: Filed to save semi-RAW file!\n");		
		img = deleteImg(img);
		return 1;
	}
	
	printf("OK.\n");
	img = deleteImg(img);
	return 0;
}

// dump an image as a windows bmp
static int dumpImageBMP(const char * filename, u8 * srcData, const size_t width, const size_t height) {
	// Calculate the size of the data when the image header is offsets + sizes
	size_t headerSize = height * 4;
	u8 * lastHeaderEntryPtr = &srcData[headerSize - 4];
	u16 lastOffset = get_u16(lastHeaderEntryPtr);
	u16 lastSize = get_u16(&lastHeaderEntryPtr[2]);
	
	// The size is stored as a multiple of 32!. This suggests the lowest five bits may be used for something else.
	if((lastSize&0x001F) != 0) {
		printf("ERROR: Image size has unexpected data in the bottom 5 bits! 0x%02X\n", lastSize&0x001F);
		return 1;
	}
	size_t imageSize = lastOffset + (lastSize / 32) - headerSize;

	printf("Dumping %s into BMP format ... ", filename);	

	Img srcImg;
	srcImg.w = width;
	srcImg.h = height;
	srcImg.format = IF_RLE_NEW;
	srcImg.data = &srcData[headerSize];
	srcImg.size = imageSize ;

	Img * img = cloneImg(&srcImg);

	// now we can save it
	Bytes * b = imgToBMP(img);
	img = deleteImg(img);
	if(b == NULL) {
		printf("ERROR: Failed to convert image to BMP!\n");
		return 1;	// ERROR
	}
	int r = saveBytesToFile(b, filename);
	if(r != 0) {
		printf("ERROR: Filed to save BMP file!\n");		
	}
	b = deleteBytes(b);

	printf("OK.\n");
	return 0;
}

// dump an image in the requested format
int dumpImage(const char * filename, u8 * srcData, const size_t width, const size_t height, const Format format) {
	if(format == FMT_RAW) {
		return dumpImageRaw(filename, srcData, height);
	} else if(format == FMT_SEMI) {
		return dumpImageSemi(filename, srcData, width, height);
	} else { // format == FMT_BMP
		return dumpImageBMP(filename, srcData, width, height);
	}
}

// dump binary data to file
int dumpBlob(const char * fileName, const u8 * srcData, size_t length) {
	// open the dump file
	FILE * dumpFile = fopen(fileName,"wb");
	if(dumpFile==NULL) {
		return 1;	// FAILED
	}
	int idx = 0;

	// write the data to the dump file
	while(length > 0) {
		size_t bytesToWrite = 4096;
		if(length < bytesToWrite) {
			bytesToWrite = length;
		}

		size_t rval = fwrite(&srcData[idx],1,bytesToWrite,dumpFile);
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
