/*  bmp.c - bitmap functions

	Alternate Da Watch Face Tool (adawft)
	adawft: Watch Face Tool for 'new' MO YOUNG / DA FIT binary watch face files.

	Copyright 2024 David Atkinson
	Author: David Atkinson <dav!id47k@d47.co> (remove the '!')
	License: GNU General Public License version 2 or any later version (GPL-2.0-or-later)
*/

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "types.h"
#include "bytes.h"
#include "adawft.h"
#include "bmp.h"

//----------------------------------------------------------------------------
//  RGB565 to RGB888 conversion
//----------------------------------------------------------------------------

static RGBTrip RGB565to888(u16 pixel) {	
	pixel = swap_bo_u16(pixel);				// need to reverse the source pixel
	RGBTrip output;
	output.b = (u8)((pixel & 0x001F) << 3);		// first 5 bits
	output.b |= (pixel & 0x001C) >> 3;			// add extra precision of 3 bits
	output.g = (pixel & 0x07E0) >> 3;			// first 6 bits
	output.g |= (pixel & 0x0600) >> 9;			// add extra precision of 2 bits
	output.r = (pixel & 0xF800) >> 8;			// first 5 bits
	output.r |= (pixel & 0xE000) >> 13;			// add extra precision of 3 bits
	return output;
}

static u16 RGB888to565(u8 * buf) {
    u16 output = 0;
	u8 b = buf[0];
	u8 g = buf[1];
	u8 r = buf[2];
	output |= (b & 0xF8) >> 3;            // 5 bits
    output |= (g & 0xFC) << 3;            // 6 bits
    output |= (r & 0xF8) << 8;            // 5 bits
    return output;
}

//----------------------------------------------------------------------------
//  SETBMPHEADER - Set up a BMPHeaderClassic or BMPHeaderV4 struct
//----------------------------------------------------------------------------

// Set up a BMP header. bpp must be 16 or 24.
void setBMPHeaderClassic(BMPHeaderClassic * dest, u32 width, u32 height, u8 bpp) {
	// Note: 24bpp images should only dump (dest->offset) bytes of this header, not the whole thing (don't need last 12 bytes)
	*dest = (BMPHeaderClassic){ 0 };
	dest->sig = 0x4D42;
	if(bpp == 16) {
		dest->offset = sizeof(BMPHeaderClassic);
	} else if(bpp == 24) {
		dest->offset = sizeof(BMPHeaderClassic) - 12;
	}
	dest->dibHeaderSize = 40;						// 40 for BITMAPINFOHEADER
	dest->width = (i32)width;
	dest->height = -(i32)height;
	dest->planes = 1;
	dest->bpp = bpp;
	if(bpp == 16) {
		dest->compressionType = 3;					// BI_BITFIELDS=3
		dest->bmiColors[0] = 0xF800;				
		dest->bmiColors[1] = 0x07E0;				
		dest->bmiColors[2] = 0x001F;				
	} else if(bpp == 24) {
		dest->compressionType = 0;					// BI_RGB=0
	}
	u32 rowSize = (((bpp/8) * width) + 3) & 0xFFFFFFFC;
	dest->imageDataSize = rowSize * height;
	dest->fileSize = dest->imageDataSize + dest->offset;
	dest->hres = 2835;								// 72dpi
	dest->vres = 2835;								// 72dpi
}



// Set up a BMP header. bpp must be 16, 24, or 32.
void setBMPHeaderV4(BMPHeaderV4 * dest, u32 width, u32 height, u8 bpp) {
	*dest = (BMPHeaderV4){ 0 };
	dest->sig = 0x4D42;
	dest->offset = sizeof(BMPHeaderV4);
	dest->dibHeaderSize = 108; 						// 108 for BITMAPV4HEADER
	dest->width = (i32)width;
	dest->height = -(i32)height;
	dest->planes = 1;
	dest->bpp = bpp;
	u32 rowSize = (((bpp/8) * width) + 3) & 0xFFFFFFFC;
	if(bpp == 16) {
		dest->compressionType = 3; 					// BI_BITFIELDS=3
		dest->RGBAmasks[0] = 0xF800;
		dest->RGBAmasks[1] = 0x07E0;
		dest->RGBAmasks[2] = 0x001F;	
	} else if(bpp == 32) {
		dest->compressionType = 3; 				// 
		dest->RGBAmasks[0] = 0xFF0000;			// r
		dest->RGBAmasks[1] = 0x00FF00;			// g
		dest->RGBAmasks[2] = 0x0000FF;			// b
		dest->RGBAmasks[3] = 0xFF000000;		// a     ARGB8888 (or BGRA8888)
	} else if(bpp == 24) {
		dest->compressionType = 0; 					// BI_RGB=0
	}
	dest->imageDataSize = rowSize * height;
	dest->fileSize = dest->imageDataSize + sizeof(BMPHeaderV4);
	dest->hres = 2835;								// 72dpi
	dest->vres = 2835;								// 72dpi
}


// Set up a BMP header. bpp must be 16, 24, or 32.
void setBMPHeaderV5(BMPHeaderV5 * dest, u32 width, u32 height, u8 bpp) {
	*dest = (BMPHeaderV5){ 0 };
	dest->sig = 0x4D42;
	dest->offset = sizeof(BMPHeaderV5);
	dest->dibHeaderSize = 108 + 16; 						// 
	dest->width = (i32)width;
	dest->height = -(i32)height;
	dest->planes = 1;
	dest->bpp = bpp;
	u32 rowSize = (((bpp/8) * width) + 3) & 0xFFFFFFFC;
	if(bpp == 16) {
		dest->compressionType = 3; 					// BI_BITFIELDS=3
		dest->RGBAmasks[0] = 0xF800;
		dest->RGBAmasks[1] = 0x07E0;
		dest->RGBAmasks[2] = 0x001F;	
	} else if(bpp == 32) {
		dest->compressionType = 3; 				// 
		dest->RGBAmasks[0] = 0xFF0000;			// r
		dest->RGBAmasks[1] = 0x00FF00;			// g
		dest->RGBAmasks[2] = 0x0000FF;			// b
		dest->RGBAmasks[3] = 0xFF000000;		// a     ARGB8888 (or BGRA8888)
	} else if(bpp == 24) {
		dest->compressionType = 0; 					// BI_RGB=0
	}
	dest->imageDataSize = rowSize * height;
	dest->fileSize = dest->imageDataSize + sizeof(BMPHeaderV5);
	dest->hres = 2835;								// 72dpi
	dest->vres = 2835;								// 72dpi
}
//----------------------------------------------------------------------------
//  DUMPBMP - dump binary data to bitmap file
//----------------------------------------------------------------------------

/* For 24bpp:
	for(u32 x=0; x<imgWidth; x++) {
		RGBTrip pixel = RGB565to888(get_u16(&srcPtr[2*x]));
		buf[3*x] = pixel.r;
		buf[3*x+1] = pixel.g;
		buf[3*x+2] = pixel.b;
	}
*/

int dumpBMP16(char * filename, u8 * srcData, size_t srcDataSize, u32 imgWidth, u32 imgHeight, bool basicRLE) {	
	// Check we have at least a little data available
	if(srcDataSize < 2) {
		printf("ERROR: srcDataSize < 2 bytes!\n");
		return 100;
	}

	// Check if this bitmap has the RLE encoded identifier
	u16 identifier = get_u16(&srcData[0]);
	int isRLE = (identifier == 0x2108);

	BMPHeaderV4 bmpHeader;
	setBMPHeaderV4(&bmpHeader, imgWidth, imgHeight, 16);

	// row width is equal to imageDataSize / imgHeight
	u32 destRowSize = bmpHeader.imageDataSize / imgHeight;

	u8 buf[8192];
	if(destRowSize > sizeof(buf)) {
		printf("ERROR: Image width exceeds buffer size!\n");
		return 3;
	}

	// open the dump file
	FILE * dumpFile = fopen(filename,"wb");
	if(dumpFile==NULL) {
		return 1;
	}

	// write the header 	
	size_t rval = fwrite(&bmpHeader,1,sizeof(bmpHeader),dumpFile);
	if(rval != sizeof(bmpHeader)) {
		fclose(dumpFile);
		remove(filename);
		return 2;
	}

	if(isRLE && !basicRLE) {
		// The newer RLE style has a table at the start with the offsets of each row. (RLE_LINE)
		u8 * lineEndOffset = &srcData[2];
		size_t srcIdx = (2 * imgHeight) + 2; // offset from start of RLEImage to RLEData

		// The srcDataSize must be at least get_u16(&lineEndOffset[imgHeight*2]) 
		size_t dataEnd = get_u16(&lineEndOffset[(imgHeight-1)*2]) - 1;		// This marks the last byte location, plus one.
		if(srcIdx > srcDataSize || dataEnd > srcDataSize) {
			printf("ERROR: Insufficient srcData to decode RLE image.\n");
			fclose(dumpFile);
			remove(filename);
			return 101;
		}

		// for each row
		for(u32 y=0; y<imgHeight; y++) {
			memset(buf, 0, destRowSize);
			u32 bufIdx = 0;

			//printf("line %d, srcIdx %lu, lineEndOffset %d, first color 0x%04x, first count %d\n", y, srcIdx, get_u16(&lineEndOffset[y*2]), 0, srcData[srcIdx+2]);

			
			while(srcIdx < get_u16(&lineEndOffset[y*2])) { // built in end-of-line detection
				u8 count = srcData[srcIdx + 2];
				u8 pixel0 = srcData[srcIdx + 1];
				u8 pixel1 = srcData[srcIdx + 0];

				for(int j=0; j<count; j++) {	// fill out this color
					if(bufIdx+1 >= sizeof(buf)) {
						break;		// don't write past end of buffer. only a problem with erroneous files.      TODO: CHECK THIS... and the other tests I put in here!
					}
					buf[bufIdx] = pixel0;
					buf[bufIdx+1] = pixel1;
					bufIdx += 2;
				}
				srcIdx += 3; // next block of data
			}

			rval = fwrite(buf,1,destRowSize,dumpFile);
			if(rval != destRowSize) {
				fclose(dumpFile);
				remove(filename);
				return 2;
			}
		}
	} else if(isRLE && basicRLE) {
		// This is an OLD RLE style, with no offsets at the start, and no concern for row boundaries. RLE_BASIC.
		u32 srcIdx = 2;		
		u8 pixel0 = 0;
		u8 pixel1 = 0;
		u8 count = 0;
		for(u32 y=0; y<imgHeight; y++) {
			memset(buf, 0, destRowSize);
			u32 pixelCount = 0;
			u32 i = 0;
			if(count > 0) {
				// still some pixels leftover from last row
				while(i < count && i < imgWidth) {
					buf[i*2] = pixel0;
					buf[i*2+1] = pixel1;
					i++;
					pixelCount++;
				}
			}
			while(pixelCount < imgWidth) {
				if(srcIdx+2 >= srcDataSize) {	// Check we have enough data to continue
					printf("ERROR: Insufficient srcData for RLE_BASIC image.\n");
					return 102;
				}
				count = srcData[srcIdx + 2];
				pixel0 = srcData[srcIdx + 1];
				pixel1 = srcData[srcIdx + 0];

				i = 0;
				while(i < count && pixelCount < imgWidth) {
					buf[pixelCount*2] = pixel0;
					buf[pixelCount*2+1] = pixel1;
					i++;
					pixelCount++;
				}
				srcIdx += 3;
			}
			if(count>i) {
				count -= i;		// some pixels left over
			} else {
				count = 0;		// no pixels left over
			}
			rval = fwrite(buf, 1, destRowSize, dumpFile);
			if(rval != destRowSize) {
				fclose(dumpFile);
				remove(filename);
				return 2;
			}
		}
	} else {
		// Basic RGB565 data
		if(imgHeight * imgWidth * 2 > srcDataSize) {
			printf("ERROR: Insufficient srcData for RGB565 image.\n");
			return 103;
		}
		// for each row
		const u32 srcRowSize = imgWidth * 2;
		size_t srcIdx = 0;
		for(u32 y=0; y<imgHeight; y++) {
			memset(buf, 0, destRowSize);
			u8 * srcPtr = &srcData[srcIdx];
			// for each pixel
			for(u32 x=0; x<imgWidth; x++) {
				u16 pixel = swap_bo_u16(get_u16(&srcPtr[x*2]));
				buf[2*x] = pixel & 0xFF;
				buf[2*x+1] = pixel >> 8;
			}

			rval = fwrite(buf,1,destRowSize,dumpFile);
			if(rval != destRowSize) {
				fclose(dumpFile);
				remove(filename);
				return 2;
			}
			srcIdx += srcRowSize;
		}
	}

	// close the dump file
	fclose(dumpFile);

	return 0; // SUCCESS
}

// imgToBMP

Bytes * imgToBMP(const Img * srcImg) {	
	BMPHeaderV5 bmpHeader;
	setBMPHeaderV5(&bmpHeader, srcImg->w, srcImg->h, 32);

	Img * img = cloneImg(srcImg);
	if(img == NULL) {
		printf("ERROR: running cloneImg\n");
		return NULL;
	}

	// Only works with ARGB8888 images for now, so we'll convert source image into this format
	if(srcImg->format != IF_ARGB8888) {
		img = convertImg(img, IF_ARGB8888);
		if(img == NULL) {
			printf("ERROR: running convertImg\n");
			return NULL;
		}
	}
	
	// row width is equal to imageDataSize / imgHeight
	u32 destRowSize = bmpHeader.imageDataSize / img->h;

	u8 buf[16384];
	if(destRowSize > sizeof(buf)) {
		printf("ERROR: Image width exceeds buffer size!\n");
		return NULL;
	}

	// Create some bytes to store the BMP
	Bytes * b = malloc(sizeof(Bytes) + bmpHeader.fileSize);
	b->size = bmpHeader.fileSize;
	if(b==NULL) {
		printf("ERROR: Couldn't allocate memory!\n");
		return NULL;
	}

	// write the header 	
	memcpy(b->data, &bmpHeader, sizeof(bmpHeader));
	size_t offset = sizeof(bmpHeader);
	
	// for each row
	for(u32 y=0; y < img->h; y++) {
		// copy entire row
		memcpy(&b->data[offset], &img->data[y * img->w * 4], img->w * 4);
		offset += destRowSize;
	}

	return b; // SUCCESS
}




//----------------------------------------------------------------------------
//  IMG, newIMG, deleteIMG - read bitmap file into basic RGB565 data format
//----------------------------------------------------------------------------

// Allocate Img and fill it with pixels from a bmp file. Returns NULL for failure. Delete with deleteImg.
Img * newImgFromFile(char * filename) {
    // read in the whole file
	Bytes * bytes = newBytesFromFile(filename);
	if(bytes==NULL) {
		printf("ERROR: Unable to read file.\n");
		return NULL;
	}

	if(bytes->size < BASIC_BMP_HEADER_SIZE) {
		printf("ERROR: File is too small.\n");
		deleteBytes(bytes);
		return NULL;
	}

    // process the header

	BMPHeaderClassic * h = (BMPHeaderClassic *)bytes->data;

	int fail = 0;
	if(h->sig != 0x4D42) {
		printf("ERROR: BMP file is not a bitmap.\n");
		fail = 1;
	}

	if(h->dibHeaderSize != 40 && h->dibHeaderSize != 108 && h->dibHeaderSize != 124) {
		printf("ERROR: BMP header format unrecognised.\n");
		fail = 1;
	}

	if(h->planes != 1 || h->reserved1 != 0 || h->reserved2 != 0) {
		printf("ERROR: BMP is unusual, can't read it.\n");
		fail = 1;
	}

	if(h->bpp != 16 && h->bpp != 24 && h->bpp != 32) {
		printf("ERROR: BMP must be RGB565 or RGB888 or ARGB8888.\n");
		fail = 1;
	}
	
	if(h->bpp == 16 && h->compressionType != 3) {
		printf("ERROR: BMP of 16bpp doesn't have bitfields.\n");
		fail = 1;
	}
	
	if((h->bpp == 24 || h->bpp == 32) && (h->compressionType != 0 && h->compressionType != 3)) {
		printf("ERROR: BMP of 24/32bpp must be uncompressed.\n");
		fail = 1;
	}

	if(fail) {
		deleteBytes(bytes);
		return NULL;
	}

	// Check if it's a top-down or bottom-up BMP. Normalise height to be positive.
	bool topDown = false;
	if(h->height < 0) {
		topDown = true;
		h->height = -h->height;
	}

	if(h->height < 1 || h->width < 1) {
		printf("ERROR: BMP has no dimensions!\n");
		deleteBytes(bytes);
		return NULL;
	}

	u32 rowSize = h->imageDataSize / (u32)h->height;
	if(rowSize < ((u32)h->width * 2)) {		
		// we'll have to calculate it ourselves! size of file is in b->bytes, subtract h->offset.
		h->imageDataSize = (u32)bytes->size - h->offset;
		rowSize = h->imageDataSize / (u32)h->height;
		if(rowSize < ((u32)h->width * 2)) {
			printf("ERROR: BMP imageDataSize (%u) doesn't make sense!\n", h->imageDataSize);
			deleteBytes(bytes);
			return NULL;
		}
	}

	if(h->offset + h->imageDataSize < bytes->size) {
		printf("ERROR: BMP file is too short to contain supposed data.\n");
		deleteBytes(bytes);
		return NULL;
	}

	// Allocate memory to store ImageData and data
	Img * img = malloc(sizeof(Img));
	if(img == NULL) {
		printf("ERROR: Out of memory.\n");
		deleteBytes(bytes);
		return NULL;
	}
	img->w = (u32)h->width;
	img->h = (u32)h->height;
	if(h->bpp == 16) {
		img->format = IF_ARGB8565;			// We'll read it into this format
		img->size = img->w * img->h * 2;
	} else { // bpp = 24 or 32
		img->format = IF_ARGB8888;			// We'll read it into this format
		img->size = img->w * img->h * 4;	// Size is simple to calculate when no compression
	}

	img->data = malloc(img->size);
	if(img->data == NULL) {
		printf("ERROR: Out of memory.\n");
		deleteBytes(bytes);
		deleteImg(img);
		return NULL;
	}

	// Reading the file data depends on bpp
	if(h->bpp == 16) { // RGB565
		printf("WARNING: THIS CODE FOR RGB565 IS UNTESTED!...\n");

		// check bitfields are what we expect
		if(bytes->size < sizeof(BMPHeaderClassic)) {
			printf("ERROR: BMP file is too short to contain bitfields.\n");
			deleteBytes(bytes);
			deleteImg(img);
			return NULL;
		}
		if(h->bmiColors[0] != 0xF800 || h->bmiColors[1] != 0x07E0 || h->bmiColors[2] != 0x001F) {
			printf("ERROR: BMP bitfields are not what we expect (RGB565).\n");
			deleteBytes(bytes);
			deleteImg(img);
			return NULL;
		}

		// read in data, row by row
		for(u32 y = 0; y < img->h; y++) {
			u32 row = topDown ? y : (img->h - y - 1);	// row is line in BMP file, y is line in our img
			size_t bmpOffset = h->offset + row * rowSize;
			// for each pixel in this row
			for(size_t x=0; x<img->w; x++) {
				// read the pixel 565
				u8 a = bytes->data[bmpOffset + 2*x];
				u8 b = bytes->data[bmpOffset + 2*x + 1] ;
				// set the pixel 8565, full alpha
				u8 destData[3];
				destData[0] = 0xFF;	// full alpha
				destData[1] = a;
				destData[2] = b;
				memcpy(&img->data[(y * img->w + x) * 3], destData, 3);
			}
		}

		// done!
	} else if (h->bpp == 32 && h->dibHeaderSize > 40) { 	// ARGB8888
		BMPHeaderV4 * h4 = (BMPHeaderV4 *)&h;
		// check bitfields (if they exist) are what we expect
		if(h->compressionType == 3) {
			if(h4->RGBAmasks[0] != 0xFF000000 || h4->RGBAmasks[1] != 0x00FF0000 || h4->RGBAmasks[2] != 0x0000FF00 || h4->RGBAmasks[3] != 0x000000FF) {
				printf("ERROR: BMP bitfields are not what we expect for 32-bit image (ARGB8888).\n");
				deleteBytes(bytes);
				deleteImg(img);
				return NULL;
			}
		}

	    // read in data, row by row
		for(u32 y=0; y < img->h; y++) {
			u32 row = topDown ? y : (img->h - y - 1);
			size_t bmpOffset = h->offset + row * rowSize;
			// copy the row across directly
			memcpy(&img->data[y * img->w * 4], &bytes->data[bmpOffset], img->w * 4);
		}
	} else { // RGB888
		// check bitfields (if they exist) are what we expect
		if(h->compressionType == 3) {
			if(h->bmiColors[0] != 0xFF0000 || h->bmiColors[1] != 0x00FF00 || h->bmiColors[2] != 0x0000FF) {
				printf("ERROR: BMP bitfields are not what we expect (RGB888).\n");
				deleteBytes(bytes);
				deleteImg(img);
				return NULL;
			}
		}

	    // read in data, row by row, pixel by pixel
		for(u32 y=0; y < img->h; y++) {
			u32 row = topDown ? y : (img->h - y - 1);
			size_t bmpOffset = h->offset + row * rowSize;
			for(u32 x=0; x < img->w; x++) {
				// RGB888 to ARGB8565 conversion
				u32 pixel = 0xFF; // set alpha
				pixel <<= 8;		
				pixel |= bytes->data[bmpOffset + x * 3];
				pixel <<= 8;
				pixel |= bytes->data[bmpOffset + x * 3 + 1];
				pixel <<= 8;
				pixel |= bytes->data[bmpOffset + x * 3 + 2];
				memcpy(&img->data[(y * img->w + x) * 4], &pixel, 4);
			}
		}
	}

	// Free filedata
	deleteBytes(bytes);

	// Return Img
	return img;
}

// Delete an Img. Safe to use on already deleted Img.
Img * deleteImg(Img * i) {
    if(i != NULL) {
		if(i->data != NULL) {
			free(i->data);
			i->data = NULL;
		}		
		free(i);
        i = NULL;
    }
	return i;
}

Img * cloneImg(const Img * i) {
	// Allocate memory to store ImageData and data
	Img * img = malloc(sizeof(Img));
	if(img == NULL) {
		printf("ERROR: Out of memory.\n");
		return NULL;
	}	
	img->w = i->w;
	img->h = i->h;
	img->format = i->format;
	img->size = i->size;
	img->data = malloc(img->size);
	if(img->data == NULL) {
		printf("ERROR: Out of memory.\n");
		deleteImg(img);
		return NULL;
	}
	memcpy(img->data, i->data, img->size);
	return img;
}

Img * convertImg(Img * i, ImgFormat newFormat) {
	// Convert between different image formats
	// Our converters will be ARGB8888 <> ARGB8565
	// and RLE_NEW <> ARGB8565
	// We will re-call ourselves to do ARGB8888 <> RLE_NEW

	if(newFormat == IF_ARGB8888) {
		if(i->format == IF_RLE_NEW) {
			// Convert to ARGB8565 first
			i = convertImg(i, IF_ARGB8565);
			if(i == NULL) {
				return i;
			}
		}
		if(i->format == IF_ARGB8565) {
			// increase bit depth
			Img * newImg = malloc(sizeof(Img));
			if(newImg == NULL) {
				printf("ERROR: Out of memory\n");
				deleteImg(i);
				return NULL;
			}
			newImg->w = i->w;
			newImg->h = i->h;
			newImg->format = newFormat;
			newImg->size = i->w * i->h * 4;
			newImg->data = malloc(newImg->size);
			if(newImg->data == NULL) {
				printf("ERROR: Out of memory\n");
				deleteImg(i);
				deleteImg(newImg);
				return NULL;
			}
			// Go row by row, pixel by pixel
			for(size_t y=0; y<i->h; y++) {
				for(size_t x=0; x<i->w; x++) {
					u8 * p = &i->data[i->w * y * 3 + x * 3];
					ARGB8888 * output = (ARGB8888 *)&newImg->data[(i->w * y + x) * 4];
					// Read in 3 bytes, convert to 4 bytes
					// Alpha byte is the same, RGB parts need converting from 565 to 888
					output->a = p[0];
					RGBTrip rgb = RGB565to888(get_u16(&p[1]));
					output->r = rgb.r;
					output->g = rgb.g;
					output->b = rgb.b;
				}
			}
			deleteImg(i);
			return(newImg);
		}
	}	
	if(newFormat == IF_ARGB8565) {
		if(i->format == IF_ARGB8888) {
			// reduce bit depth
			Img * newImg = malloc(sizeof(Img));
			if(newImg == NULL) {
				printf("ERROR: Out of memory\n");
				deleteImg(i);
				return NULL;
			}
			newImg->w = i->w;
			newImg->h = i->h;
			newImg->format = newFormat;
			newImg->size = i->w * i->h * 3;
			newImg->data = malloc(newImg->size);
			if(newImg->data == NULL) {
				printf("ERROR: Out of memory\n");
				deleteImg(i);
				deleteImg(newImg);
				return NULL;
			}
			// Go row by row, pixel by pixel
			for(size_t y=0; y<i->h; y++) {
				for(size_t x=0; x<i->w; x++) {
					u8 * p = &i->data[(i->w * y + x) * 4];
					u8 * output = &newImg->data[(i->w * y + x) * 3];
					// Converting ARGB8888 to ARGB8565
					// Convert 4 bytes to 3 bytes
					// Alpha byte is the same
					output[0] = p[0];
					u16 rgb565 = RGB888to565(&p[1]);
					output[1] = (rgb565 & 0xFF); 			// lo byte
					output[2] = (rgb565 >> 8);				// hi byte
				}
			}
			deleteImg(i);
			return(newImg);
		}
		if(i->format == IF_RLE_NEW) {
			// allocate memory for the new image
			Img * newImg = malloc(sizeof(Img));
			if(newImg == NULL) {
				printf("ERROR: Out of memory\n");
				deleteImg(i);
				return NULL;
			}
			newImg->w = i->w;
			newImg->h = i->h;
			newImg->format = newFormat;
			newImg->size = i->w * i->h * 3;
			newImg->data = malloc(newImg->size);
			if(newImg->data == NULL) {
				printf("ERROR: Out of memory\n");
				deleteImg(i);
				deleteImg(newImg);
				return NULL;
			}
			
			// decompress the data
			size_t bytesOut = 0;
			size_t bytesIn = 0;

			while(bytesIn < i->size) {
				u8 cmd = i->data[bytesIn]; 		// read a byte
				bytesIn++;				
				if((cmd & 0x80) != 0) { // Repeat the pixel
					size_t count = (cmd & 0x7F);
					// printf("1:%zu ", count*3);
					u8 data[3];
					data[0] = i->data[bytesIn]; 
					data[1] = i->data[bytesIn+1];
					data[2] = i->data[bytesIn+2];
					bytesIn += 3;
					for(size_t j=0; j<count; j++) {
						newImg->data[bytesOut]   = data[0]; 
						newImg->data[bytesOut+1] = data[1]; 
						newImg->data[bytesOut+2] = data[2]; 
						bytesOut += 3;
					}
				} else { // Normal pixel data
					size_t count = cmd * 3;
					//printf("0:%zu ", count);
					memcpy(&newImg->data[bytesOut], &i->data[bytesIn], count);
					bytesOut += count;
					bytesIn += count;
				}
			}
			deleteImg(i);
			return newImg;
		}
	}
	if(newFormat == IF_RLE_NEW) {
		if(i->format == IF_ARGB8888) {
			// reduce bit depth first
			i = convertImg(i, IF_ARGB8565);
			if(i == NULL) {
				return NULL;
			}
		}
		if(i->format == IF_ARGB8565) {		
			// compress it
			printf("ERROR: COMPRESSION NOT YET IMPLEMENTED\n");
			return NULL;
		}
	}
	// If we get here, it was a weird request (or a bug)
	printf("ERROR: Reached unexpected point in convertImg\n");
	deleteImg(i);
	return NULL;
}
