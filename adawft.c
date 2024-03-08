/*  adawft.c

	Alternate Da Watch Face Tool (adawft)
	adawft: Watch Face Tool for 'new' MO YOUNG / DA FIT binary watch face files.
	Typically obtained by DA FIT app from api.moyoung.com in the /new/ system

	Designed for the following watches:
		GTS3		MOY-VSW4-2.0.1		240x296

	First byte of file indicates api_ver for this 'new' type

	File type (not directly specified in binary file) we call type N (for 'new').

	Copyright 2024 David Atkinson
	Author: David Atkinson <dav!id47k@d47.co> (remove the '!')
	License: GNU General Public License version 2 or any later version (GPL-2.0-or-later)

*/

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>		// for mkdir()
#include <assert.h>

#include "face_new.h"
#include "adawft.h"
#include "bytes.h"
#include "bmp.h"
#include "strutil.h"


//----------------------------------------------------------------------------
//  DATA READING AND BYTE ORDER
//----------------------------------------------------------------------------

static int systemIsLittleEndian() {				// return 0 for big endian, 1 for little endian.
    volatile uint32_t i=0x01234567;
    return (*((volatile uint8_t*)(&i))) == 0x67;
}

inline void set_u16(u8 * p, u16 v) {
    p[0] = v&0xFF;
    p[1] = v>>8;
}

//----------------------------------------------------------------------------
//  API_VER_INFO - information about each API level
//----------------------------------------------------------------------------

typedef struct _ApiVerInfo {
	u8 apiVer;
	const char * description;
} ApiVerInfo;

static const ApiVerInfo API_VER_INFO[] = {
	{  2, "HHMM only" },
	{  4, "HHMM, weekday name" },
	{ 10, "Analog HMS hands" },
	{ 13, "HHMM, weekday name, DD" },
	{ 15, "HHMM, weekday name, DD, MM, steps" },
	{ 18, "HHMM or Analog HMS hands, DD, weekday name, bpm, kcal, battery, steps." },
	{ 20, "Same as 18 plus ??" },
	{ 29, "HHMM, bpm, ?, weather" },
	{ 35, "Analog HMS hands, weekday name, DD, bpm, ?, ?" },
};


//----------------------------------------------------------------------------
//  DUMPBLOB - dump binary data to file
//----------------------------------------------------------------------------

static int dumpBlob(char * fileName, u8 * srcData, size_t length) {
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


//----------------------------------------------------------------------------
//  DUMPIMAGE - dump binary data to file
//----------------------------------------------------------------------------

typedef enum _Format {
	FMT_RAW = 0,
	FMT_SEMI = 1,
	FMT_BMP = 2,
} Format;

static int dumpImageRaw(char * filename, u8 * srcData, size_t height) {
	// Calculate the size of the data when the image header is offsets + sizes
	size_t lastHeaderEntry = (height * 4) - 4;
	u8 * lastHeaderEntryPtr = &srcData[lastHeaderEntry];
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


static int dumpImageSemi(char * filename, u8 * srcData, size_t width, size_t height) {
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

// Dump a bitmap of the image
static int dumpImageSemiOld(char * filename, u8 * srcData, size_t height) {
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
	printf("Dumping %s in semi-raw format (%zu bytes) ... ", filename, imageSize);	

	u8 * outputData = malloc(1024*1024);
	if(outputData == NULL) {
		printf("ERROR: FAILED TO ALLOCATE MEMORY\n");
		return 1;
	}
	size_t outputPos = 0;

	for(size_t r = 0; r<height; r++) {
		u16 dataOffset = get_u16(&srcData[r*4]);
		u8 * dataPtr = &srcData[dataOffset];
		u16 rowSize = get_u16(&srcData[r*4+2]) / 32;
		printf("  row: %3zu  dataOffset: 0x%06X  rowSize: 0x%03X (%4u)... ", r, dataOffset, rowSize, rowSize);

		size_t bytesOut = 0;
		size_t bytesIn = 0;
		while(bytesIn < rowSize) {
			u8 cmd = dataPtr[0]; 		// read a byte
			dataPtr++;
			bytesIn++;
			
			if((cmd & 0x80) != 0) { // Repeat the pixel
				size_t count = (cmd & 0x7F);
				printf("1:%zu ", count*3);
				u8 data[3];
				data[0] = *dataPtr; 
				dataPtr++;
				data[1] = *dataPtr; 
				dataPtr++;
				data[2] = *dataPtr; 
				dataPtr++;
				bytesIn += 3;
				for(size_t j=0; j<count; j++) {
					outputData[outputPos] = data[0]; 
					outputPos++;
					outputData[outputPos] = data[1]; 
					outputPos++;
					outputData[outputPos] = data[2]; 
					outputPos++;
					bytesOut += 3;
				}
			} else { // Normal pixel data
				size_t count = cmd * 3;
				printf("0:%zu ", count);
				memcpy(&outputData[outputPos], dataPtr, count);
				outputPos += count;
				dataPtr += count;
				bytesOut += count;
				bytesIn += count;
			}			
		}
		printf("(%zd bytes)\n", bytesOut);		
	}
	// save to file
	int r = dumpBlob(filename, outputData, outputPos);

	// free allocated memory
	free(outputData);

	if(r!=0) {
		printf("ERROR: dumpImage failed (%d)\n", r);
		return 0;
	} else {
		printf("%zu bytes OK\n", outputPos);
		return 0;
	}
}

// Dump a bitmap of the image
static int dumpImageBMP(char * filename, u8 * srcData, size_t width, size_t height) {
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

static int dumpImage(char * filename, u8 * srcData, size_t width, size_t height, Format format) {
	if(format == FMT_RAW) {
		return dumpImageRaw(filename, srcData, height);
	} else if(format == FMT_SEMI) {
		return dumpImageSemi(filename, srcData, width, height);
	} else { // format == FMT_BMP
		return dumpImageBMP(filename, srcData, width, height);
	}
}

static int debugImage(u8 * srcData, size_t height) {
	// Calculate the size of the data when the image header is offsets + sizes
	size_t lastHeaderEntry = (height * 4) - 4;
	u8 * lastHeaderEntryPtr = &srcData[lastHeaderEntry];
	u16 lastOffset = get_u16(lastHeaderEntryPtr);		// Not sure how these offsets work yet
	u16 lastSize = get_u16(&lastHeaderEntryPtr[2]);
	// The size is stored as a multiple of 32!. This suggests the lowest five bits may be used for something else.
	if((lastSize&0x001F) != 0) {
		printf("ERROR: Image size has unexpected data in the bottom 5 bits! 0x%02X\n", lastSize&0x001F);
	}
	size_t imageSize = lastOffset + (lastSize / 32);
	
	printf("Image size: 0x%06zX (%zu)\n", imageSize, imageSize);
	// Print the header rows	
	size_t offset = 0;
	for(offset=0; offset<(height*4); offset+=4) {
		u16 dataOffset = get_u16(&srcData[offset]);
		u16 dataSize = get_u16(&srcData[offset+2]) / 32;
		printf("  @ 0x%06zX  row: %3zu  rowOffset: 0x%06X  rowSize: 0x%03X (%4u)\n", offset, offset/4, dataOffset, dataSize, dataSize);
	}

	// Print the data for each row
	return 0;
}

//----------------------------------------------------------------------------
//  PRINT OFFSET, WIDTH, HEIGHT ARRAY TO STRING
//----------------------------------------------------------------------------

void printOwh(OffsetWidthHeight * owh, size_t count, const char * name, char * dstBuf, size_t dstBufSize) {
	for(size_t i=0; i<count; i++) {
		snprintf(&dstBuf[strlen(dstBuf)], dstBufSize - strlen(dstBuf), 
			"%s[%zu]    0x%08X, %3u, %3u\n", name, i, owh[i].offset, owh[i].width, owh[i].height);
	}
}

//----------------------------------------------------------------------------
//  MAIN
//----------------------------------------------------------------------------



int main(int argc, char * argv[]) {
	char * fileName = "";
	char * folderName = "dump";
	Format format = FMT_BMP;
	bool dump = false;
	bool showHelp = false;
	bool fileNameSet = false;

	// display basic program header
    printf("\n%s\n\n","adawft: Alternate Da Watch Face Tool for MO YOUNG / DA FIT binary watch face files.");
    
	// check byte order
	if(!systemIsLittleEndian()) {
		printf("Sorry, this system is big-endian, and this program has only been designed for little-endian systems.\n");
		return 1;
	}

	// find executable name
	char * basename = "adawft";
	if(argc>0) {
		// find the name of the executable. not perfect but it's only used for display and no messy ifdefs.
		char * b = strrchr(argv[0],'\\');
		if(!b) {
			b = strrchr(argv[0],'/');
		}
		basename = b ? b+1 : argv[0];
	}
		
	// read command-line parameters
	for(int i=1; i<argc; i++) {
		if(streq(argv[i], "--raw")) {
			format = FMT_RAW;
		} else if(streq(argv[i], "--semiraw")) {
			format = FMT_SEMI;
		} else if(streqn(argv[i], "--dump", 6)) {
			dump = true;
			if(strlen(argv[i]) >= 8 && argv[i][6] == '=') {
				folderName = &argv[i][7];
			}
		} else if(streqn(argv[i], "--help", 6)) {
			showHelp = true;
		} else if(streqn(argv[i], "--", 2)) {
			printf("ERROR: Unknown option: %s\n", argv[i]);
			showHelp = true;
		} else {
			// must be fileName
			if(!fileNameSet) {
				fileName = argv[i];
				fileNameSet = true;
			} else {
				printf("WARNING: Ignored unknown parameter: %s\n", argv[i]);
			}
		}
	}

	// display help
    if(argc<2 || showHelp) {
		printf("Usage:   %s [OPTIONS] FILENAME\n\n",basename);
		printf("%s\n","  OPTIONS");
		printf("%s\n","    --dump=FOLDERNAME    Dump data to folder. Folder name defaults to 'dump'.");
		printf("%s\n","    --raw                When dumping, dump raw files.");
		printf("%s\n","  FILENAME               Binary watch face file for input.");
		printf("\n");
		return 0;
    }

	// Open the binary input file
	Bytes * bytes = newBytesFromFile(fileName);
	if(bytes == NULL) {
		printf("ERROR: Failed to read file into memory.\n");
		return 1;
	}

	u8 * fileData = bytes->data;
	size_t fileSize = bytes->size;

	// Check file size
	if(fileSize < sizeof(FaceHeaderN)) {
		printf("ERROR: File is less than the header size (%zu bytes)!\n", sizeof(FaceHeaderN));
		deleteBytes(bytes);
		return 1;
	}

	// store discovered data in string, for saving to file
	char watchFaceStr[32000] = "";		// have enough room for all the lines we need to store

	// Load header struct from file
	FaceHeaderN * h = (FaceHeaderN *)&fileData[0];		// interpret it directly

	// Print header info	
	sscatprintf(watchFaceStr, "apiVer          %u\n", h->apiVer);
	sscatprintf(watchFaceStr, "unknown0        0x%04X\n", h->unknown0);
	sscatprintf(watchFaceStr, "unknown1        0x%04X\n", h->unknown1);
	sscatprintf(watchFaceStr, "unknown2        %u\n", h->unknown2);
	sscatprintf(watchFaceStr, "unknown3        %u\n", h->unknown3);
	sscatprintf(watchFaceStr, "unknown4        %u\n", h->unknown4);
	sscatprintf(watchFaceStr, "dhOffset        0x%04X\n", h->dhOffset);
	sscatprintf(watchFaceStr, "bhOffset        0x%04X\n", h->bhOffset);
	
	// Everything should have a background (I hope!)
	StaticHeader * bgh = (StaticHeader *)&fileData[h->bhOffset];
	sscatprintf(watchFaceStr, "bgh.xy          %3u, %3u\n", bgh->xy.x, bgh->xy.y);
	sscatprintf(watchFaceStr, "bgh.owh         0x%08X, %3u, %3u\n", bgh->offset, bgh->width, bgh->height);

	// Create a buffer for storing the dump filenames
	char dfnBuf[1024];
	snprintf(dfnBuf, sizeof(dfnBuf), "%s%s", folderName, DIR_SEPERATOR);
	size_t baseSize = strlen(dfnBuf);
	if(baseSize + 16 >= sizeof(dfnBuf)) {
		printf("ERROR: dfnBuf too small!\n");
		deleteBytes(bytes);
		return 1;
	}

	// Dump the background
	if(dump) {		
		sprintf(&dfnBuf[baseSize], "background.%s", (format==FMT_BMP?"bmp":"raw"));
		dumpImage(dfnBuf, &fileData[bgh->offset], bgh->width, bgh->height, format);
	}

	// Store some counters for filenames
	u16 staticCounter = 0;

	// Buffer for temporary string data
	char sbuf[20];

	// Process the different headers
	size_t offset = sizeof(FaceHeaderN);

	u16 type = get_u16(&fileData[offset]);
	bool more = true;
	while(more) {
		switch(type) {
			case 0x0000:
				// End of header section
				sscatprintf(watchFaceStr, "@ 0x%08zX  0000 (End of headers)\n", offset);
				offset += 2;
				more = false;
				break;
			case 0x0001:
				// StaticHeader for static images
				if(offset == h->bhOffset) {
					sscatprintf(watchFaceStr, "@ 0x%08zX  StaticHeader (Background)\n", offset);
				} else {
					sscatprintf(watchFaceStr, "@ 0x%08zX  StaticHeader\n", offset);
					StaticHeader * statich = (StaticHeader *)&fileData[offset];
					sscatprintf(watchFaceStr, "statich.type    0x%04X\n", statich->type);
					sscatprintf(watchFaceStr, "statich.xy      %3u, %3u\n", statich->xy.x, statich->xy.y);
					sscatprintf(watchFaceStr, "statich.owh     0x%08X, %3u, %3u\n", statich->offset, statich->width, statich->height);					
					if(dump) {		
						sprintf(&dfnBuf[baseSize], "static_%02u.%s", staticCounter++, (format==FMT_BMP?"bmp":"raw"));
						dumpImage(dfnBuf, &fileData[statich->offset], statich->width, statich->height, format);
					}
				}
				offset += sizeof(StaticHeader);
				break;
			case 0x0101:
				// DigitsHeader. Analog-only watchfaces don't have one.				
				// bool hasDigitsHeader = (h->dhOffset != 0);
				DigitsHeader * dh = (DigitsHeader *)&fileData[offset];
				if(dh->subtype == 0) {
					sscatprintf(watchFaceStr, "@ 0x%08zX  DigitsHeader (0: Time)\n", offset);
				} else if(dh->subtype == 1) {
					sscatprintf(watchFaceStr, "@ 0x%08zX  DigitsHeader (1: DayNum)\n", offset);
				} else {
					sscatprintf(watchFaceStr, "@ 0x%08zX  DigitsHeader (%u: Unknown)\n", offset, dh->subtype);
				}
				// what digit font is it
				sprintf(sbuf, "digit%u.owh", dh->subtype);
				// print all the details
				printOwh(&dh->owh[0], 10, sbuf, watchFaceStr, sizeof(watchFaceStr));
				// debug the first one
				// debugImage(&fileData[dh->owh[0].offset], dh->owh[0].height);
				if(dump) {
					for(size_t i=0; i<10; i++) {
						//sscatprintf(watchFaceStr, "dh.owh[%zu]    0x%08X, %3u, %3u\n", i, dh->owh[i].offset,  dh->owh[i].width, dh->owh[i].height);
						sprintf(&dfnBuf[baseSize], "digit_%u_%zu.%s", dh->subtype, i, (format==FMT_BMP?"bmp":"raw"));
						dumpImage(dfnBuf, &fileData[dh->owh[i].offset], dh->owh[i].width, dh->owh[i].height, format);
					}					
				}
				offset += sizeof(DigitsHeader);
				break;
			case 0x0201:
				// TimeHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  TimeHeader\n", offset);
				offset += sizeof(TimeHeader);
				break;				
			case 0x0401:
				// DayNameHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  DayNameHeader\n", offset);
				offset += sizeof(DayNameHeader);
				break;
			case 0x0501:
				// BatteryFillHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  BatteryFillHeader\n", offset);
				offset += sizeof(BatteryFillHeader);
				break;
			case 0x0601:
				// HeartRateNumHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  HeartRateNumHeader\n", offset);
				HeartRateNumHeader * hrn = (HeartRateNumHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "  digitSet: %u\n", hrn->digitSet);
				sscatprintf(watchFaceStr, "  justification: %u\n", hrn->justification);
				offset += sizeof(HeartRateNumHeader);
				break;
			case 0x0701:
				// StepsNumHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  StepsNumHeader\n", offset);
				StepsNumHeader * sn = (StepsNumHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "  digitSet: %u\n", sn->digitSet);
				sscatprintf(watchFaceStr, "  justification: %u\n", sn->justification);
				offset += sizeof(StepsNumHeader);
				break;
			case 0x0901:
				// KCalHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  KCalHeader\n", offset);
				offset += sizeof(KCalHeader);
				break;
			case 0x0A01:
				// HandsHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  HandsHeader\n", offset);
				offset += sizeof(HandsHeader);
				break;
			case 0x0D01:
				// DayNumHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  DayNumHeader\n", offset);
				DayNumHeader * dn = (DayNumHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "  digitSet: %u\n", dn->digitSet);
				sscatprintf(watchFaceStr, "  justification: %u\n", dn->justification);							
				offset += sizeof(DayNumHeader);
				break;
			case 0x0F01:
				// MonthNumHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  MonthNumHeader\n", offset);
				MonthNumHeader * mn = (MonthNumHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "  digitSet: %u\n", mn->digitSet);
				sscatprintf(watchFaceStr, "  justification: %u\n", mn->justification);								
				offset += sizeof(MonthNumHeader);
				break;
			case 0x1201:
				// BarDisplayHeader
				BarDisplayHeader * bdh = (BarDisplayHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "@ 0x%08zX  BarDisplayHeader. subtype: %u. count: %u.\n", offset, bdh->subtype, bdh->count);
				offset += sizeof(BarDisplayHeader) + sizeof(OffsetWidthHeight) * (bdh->count-1);
				break;
			case 0x1B01:
				// WeatherHeader
				WeatherHeader * wh = (WeatherHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "@ 0x%08zX  WeatherHeader. subtype: %u.\n", offset, wh->subtype);
				offset += sizeof(WeatherHeader);
				break;
			case 0x1D01:
				// 
				Unknown1D01 * u1h = (Unknown1D01 *)&fileData[offset];
				sscatprintf(watchFaceStr, "@ 0x%08zX  Unknown1D01Header. unknown: %u.\n", offset, u1h->unknown);
				offset += sizeof(Unknown1D01);
				break;
			case 0x2301:
				// 
				// Unknown2301 * u2h = (Unknown2301 *)&fileData[offset];
				sscatprintf(watchFaceStr, "@ 0x%08zX  Unknown2301Header.\n", offset);
				offset += sizeof(Unknown2301);
				break;	
			case 0x1401:
			case 0xEC02:
			case 0x4C01:
			case 0x8801:
			case 0x2C01:
			case 0x6001:
			case 0xD001:
				// AltDigitsHeader
				AltDigitsHeader * adh = (AltDigitsHeader *)&fileData[offset];
				// Remap it to a sane format
				AltDigitsHeaderSane sane;
				sane.type = adh->type;
				sane.unknown = adh->unknown;
				sane.owh[0].offset = adh->loByteOffset0 | (adh->hiBytesOffset0[0] << 8) | (adh->hiBytesOffset0[1] << 16)| (adh->hiBytesOffset0[2] << 24);
				sane.owh[0].width = adh->width0;
				sane.owh[0].height = adh->height0;
				memcpy(&sane.owh[1], &adh->owh[0], sizeof(OffsetWidthHeight) * 9);
				sscatprintf(watchFaceStr, "@ 0x%08zX  AltDigitsHeader (0x%04X)\n", offset, sane.type);
				sprintf(sbuf, "altDigits.owh");
				// print all the details
				printOwh(&sane.owh[0], 10, sbuf, watchFaceStr, sizeof(watchFaceStr));
				offset += sizeof(AltDigitsHeader);
				break;	
			default:
				// UNKNOWN DATA
				sscatprintf(watchFaceStr, "@ 0x%08zX  UNKNOWN TYPE 0x%04X\n", offset, type);
				sscatprintf(watchFaceStr, "ERROR: Unknown type found. Stopping early.\n");
				more = false;
				break;
		}
		type = get_u16(&fileData[offset]);
	}

	// display all the important data
	printf("%s", watchFaceStr);		

	// clean up
	deleteBytes(bytes);
	printf("\ndone.\n\n");

    return 0; // SUCCESS
}
