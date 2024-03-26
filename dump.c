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
#include "strutil.h"

// get format string
const char * dumpFormatStr(Format f) {
	switch(f) {
		case FMT_BIN: return "bin";
		case FMT_RAW: return "raw";
		case FMT_BMP: return "bmp";
	}
	return "err";
}

// determine image size from compressed data -- this doesn't include the size of the header
static size_t determineImageSize(u8 * srcData, const size_t height) {
	size_t headerSize = height * 4;
	const u8 * lastHeaderEntryPtr = &srcData[headerSize - 4];
	size_t lastOffset = get_u16(lastHeaderEntryPtr) + ((get_u16(&lastHeaderEntryPtr[2]) & 0x001F) << 16);		// extra 5 bits
	size_t lastSize = get_u16(&lastHeaderEntryPtr[2]) >> 5;
	size_t imageSize = lastOffset + lastSize;
	return (imageSize - headerSize);
}

// dump raw compressed image data
static int dumpImageBin(const char * filename, u8 * srcData, const size_t height) {
	size_t headerSize = height * 4;
	size_t imageSize = determineImageSize(srcData, height);
	dprintf(1, "Dumping BIN %s ... ", filename);	
	
	int r = dumpBlob(filename, srcData, imageSize + headerSize);
	if(r!=0) {
		dprintf(0, "ERROR: dumpImage failed (%d)\n", r);
		return 1;
	}
	dprintf(1, "OK\n");
	return 0;
}

// dump raw decompressed image data
static int dumpImageRaw(const char * filename, u8 * srcData, const size_t width, const size_t height) {
	size_t headerSize = height * 4;
	size_t imageSize = determineImageSize(srcData, height);
	dprintf(1, "Dumping RAW %s ... ", filename);	

	Img srcImg;
	srcImg.w = width;
	srcImg.h = height;
	srcImg.format = IF_RLE_NEW;
	srcImg.data = &srcData[headerSize];
	srcImg.size = imageSize;

	Img * img = cloneImg(&srcImg);

	// convert it to a raw format
	img = convertImg(img, IF_ARGB8565);
	if(img==NULL) {
		dprintf(0, "ERROR: Failed to convertImg in dumpImageRaw\n");
		deleteImg(img);
	}
	
	int r = dumpBlob(filename, img->data, img->size);
	if(r != 0) {
		dprintf(0, "ERROR: Filed to save RAW file!\n");		
		img = deleteImg(img);
		return 1;
	}
	
	dprintf(1, "OK.\n");
	img = deleteImg(img);
	return 0;
}

// dump an image as a windows bmp
static int dumpImageBMP(const char * filename, u8 * srcData, const size_t width, const size_t height) {
	size_t headerSize = height * 4;
	size_t imageSize = determineImageSize(srcData, height);
	dprintf(1, "Dumping BMP %s ... ", filename);

	Img srcImg;
	srcImg.w = width;
	srcImg.h = height;
	srcImg.format = IF_RLE_NEW;
	srcImg.data = &srcData[headerSize];
	srcImg.size = imageSize;

	Img * img = cloneImg(&srcImg);

	// now we can save it
	Bytes * b = imgToBMP(img);
	img = deleteImg(img);
	if(b == NULL) {
		dprintf(0, "ERROR: Failed to convert image to BMP!\n");
		return 1;	// ERROR
	}
	int r = saveBytesToFile(b, filename);
	if(r != 0) {
		dprintf(0, "ERROR: Filed to save BMP file!\n");		
	}
	b = deleteBytes(b);

	dprintf(1, "OK.\n");
	return 0;
}

// dump an image in the requested format
int dumpImage(const char * filename, u8 * srcData, const size_t width, const size_t height, const Format format) {
	if(format == FMT_BIN) {
		return dumpImageBin(filename, srcData, height);
	} else if(format == FMT_RAW) {
		return dumpImageRaw(filename, srcData, width, height);
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
