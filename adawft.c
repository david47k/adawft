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

#include "types.h"
#include "adawft.h"
#include "bmp.h"
#include "bytes.h"

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
//  BINARY FILE STRUCTURE
//----------------------------------------------------------------------------

#pragma pack (push)
#pragma pack (1)

typedef struct __OffsetWidthHeight {
	u32 offset;
	u16 width;
	u16 height;
} OffsetWidthHeight;

typedef struct _XY {
	u16 x;
	u16 y;
} XY;

// The FaceHeader is located at the beginning of the file
typedef struct _FaceHeaderN {
	u16 apiVer;				// api_ver
	u16 unknown0;			// FF FF
	u16 unknown1;			// 0x61F4 or 0x93A4 (for example)
	u16 unknown2;			// 0
	u16 unknown3;			// 8C 00
	u16 unknown4;			// A3 00
	u16 thOffset;			// Offset of the TimeHeader. Usually 0x0010. Seen as 0x0000 for an analog-only watchface using API10.
	u16 bhOffset;			// Offset of the background image (a StaticHeader)
} FaceHeaderN;

// TimeHeader and DayNumHeader are typically located between the FaceHeader and the background image header

// Digital clocks have a TimeHeader, Analog-Only clocks don't.
typedef struct _TimeHeader {
	u16 type;				// 0x0101
	u8 subtype;				// 00
	OffsetWidthHeight owh[10];	// Offset, Width and Height of all the digit images 0-9.
	u8 unknown2[2];			// 0
} TimeHeader;

// Some faces (e.g. API4/13) have a DayNumHeader next.
typedef struct _DayNumHeader {
	u16 type;				// 0x0101
	u8 subtype;				// 01
	OffsetWidthHeight owh[10];	// Offset, Width and Height of all the digit images 0-9.
	u8 unknown2[2];			// 0
} DayNumHeader;

// StaticHeader is for static images (e.g. the background)
typedef struct _StaticHeader {
	u16 type;				// 0x0001
	XY xy;					// 0, 0 for background
	u32 offset;				// offset of image data
	u16 width;				// width of image
	u16 height;				// height of image
} StaticHeader;

// TimePosHeader is the location of the time (HHMM) digits on the screen
typedef struct _TimePosHeader {
	u16 type;				// 0x0201
	u32 unknown;			// 0
	XY xy[4];				// x and y position of the four time digits HHMM
	u8 padding[12];			// 0
} TimePosHeader;

// DayNameHeader is for days Mon, Tue, Wed, Thu, Fri, Sat, Sun.
typedef struct _DayNameHeader {
	u16 type;				// 0x0401
	u8 subtype;				// 01
	XY xy;
	OffsetWidthHeight owh[7];
} DayNameHeader;

// Battery charge displayed as an image with a specified fill region
typedef struct _BatteryFillHeader {
	u16 type;				// 0x0501
	XY xy;
	OffsetWidthHeight owh;
	u8 x1;					// subsection for watch to fill, coords from image top left
	u8 y1;					
	u8 x2;					
	u8 y2;
	u32 unknown;
	u32 unknown2;
	OffsetWidthHeight owh2;	// maybe for empty?
	OffsetWidthHeight owh3;	// maybe for full?
} BatteryFillHeader;

// Heart rate displayed as a number
typedef struct _HeartRateNumHeader {
	u16 type;				// 0x0601
	u16 unknown;			// 0x0201
	XY xy;					// xy, centered text in this case
	u8 unknown2[18];		// 0
} HeartRateNumHeader;

// Number of steps done today
typedef struct _StepsNumHeader {
	u16 type;				// 0x0701
	u16 unknown;			// 02 02
	XY xy;					// X and Y of the steps number
	u8 unknown2[18];		// 0
} StepsNumHeader;

// KCal displayed as a number
typedef struct _KCalHeader {
	u16 type;				// 0x0901
	u16 unknown;			// 0x0201
	XY xy;					// xy, centered text in this case
	u8 unknown2[11];		// 0, size could be wrong
} KCalHeader;

// HandsHeader is for analog watchface hands
typedef struct _HandsHeader {
	u16 type;				// 0x0A01
	u8 subtype;				// 0 = hour, 1 = minutes, 2 = seconds
	XY unknownXY;
	u32 offset;
	u16 width;
	u16 height;
	u16 x;					// typically center of screen
	u16 y;					// typically center of screen
} HandsHeader;

// This unknown header has been seen in API 13 and 15.
typedef struct _UnknownHeader0D01 {
	u16 type;				// 0x0D01
	u16 unknown;			// 01 01
	XY xy[2];				// Just a guess... 
} UnknownHeader0D01;

// Month as a number
typedef struct _MonthNumHeader {
	u16 type;				// 0x0F01
	u16 unknown;			// 02 02
	XY xy[2];				// XY of the two month digits
} MonthNumHeader;

// A bar (multi-image) display for different data sources
typedef struct _BarDisplayHeader {
	u16 type;				// 0x1201
	u8 subtype;				// Data source: 5=HeartRate, 6=Battery, 2=KCal, 0=Steps
	u8 count;				// number of images in the bar display
	XY xy;
	OffsetWidthHeight owh[1];	// there are *COUNT* number of entries! (not just 1!)
} BarDisplayHeader;

#pragma pack (pop)


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
		return 1;
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
			return 2;
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

static int dumpImage(char * filename, u8 * srcData, size_t height) {
	// Calculate the size of the data when the image header is offsets + sizes
	u8 * lastHeaderOffsetOffset = &srcData[(height-1)*4];
	u8 * lastHeaderSizeOffset = &srcData[(height-1)*4+2];
	u16 lastOffset = get_u16(lastHeaderOffsetOffset);
	u16 lastSize = get_u16(lastHeaderSizeOffset);
	printf("last offset: %04X. lastSize: %04X\n", lastOffset, lastSize);
	size_t size = lastOffset + (lastSize / 32);
	printf("size: %u\n", size);
	return dumpBlob(filename, srcData, size);
}

//----------------------------------------------------------------------------
//  MAIN
//----------------------------------------------------------------------------

int main(int argc, char * argv[]) {
	char * fileName = "";
	char * folderName = "dump";
	bool raw = true;
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
			raw = true;
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
		//printf("%s\n","    --raw                When dumping, dump raw files.");
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
		printf("ERROR: File is less than the header size (%u bytes)!\n", sizeof(FaceHeaderN));
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
	sscatprintf(watchFaceStr, "thOffset        0x%04X\n", h->thOffset);
	sscatprintf(watchFaceStr, "bhOffset        0x%04X\n", h->bhOffset);
	
	// Everything should have a background (I hope!)
	StaticHeader * bgh = (StaticHeader *)&fileData[h->bhOffset];
	sscatprintf(watchFaceStr, "bgh.xy          %3u, %3u\n", bgh->xy.x, bgh->xy.y);
	sscatprintf(watchFaceStr, "bgh.owh         0x%08X, %3u, %3u\n", bgh->offset, bgh->width, bgh->height);

	if(dump) {
		char fileNameBuf[1024];
		snprintf(fileNameBuf, sizeof(fileNameBuf), "%s%sbackground.bin", folderName, DIR_SEPERATOR);
		printf("Dumping background to %s\n", fileNameBuf);
		if(dumpImage(fileNameBuf, &fileData[bgh->offset], bgh->xy.y) != 0) {
			printf("Failed to dump!\n");
		}
	}

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
				}
				offset += sizeof(StaticHeader);
				break;
			case 0x0101:
				// TimeHeader. Analog-only watchfaces don't have one.				
				// bool hasTimeHeader = (h->thOffset != 0);
				TimeHeader * timeh = (TimeHeader *)&fileData[offset];
				if(timeh->subtype == 0) {
					sscatprintf(watchFaceStr, "@ 0x%08zX  TimeHeader (Time)\n", offset);
				} else if(timeh->subtype == 1) {
					sscatprintf(watchFaceStr, "@ 0x%08zX  TimeHeader (DayNum)\n", offset);
				} else {
					sscatprintf(watchFaceStr, "@ 0x%08zX  TimeHeader (UNKNOWN type %u)\n", offset, timeh->subtype);
				}
				for(size_t i=0; i<10; i++) {
					sscatprintf(watchFaceStr, "timeh.owh[%zu]    0x%08X, %3u, %3u\n", i, timeh->owh[i].offset,  timeh->owh[i].width, timeh->owh[i].height);
				}
				offset += sizeof(TimeHeader);
				break;
			case 0x0201:
				// TimePosHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  TimePosHeader\n", offset);
				offset += sizeof(TimePosHeader);
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
				offset += sizeof(HeartRateNumHeader);
				break;
			case 0x0701:
				// StepsNumHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  StepsNumHeader\n", offset);
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
				// UnknownHeader0D01
				sscatprintf(watchFaceStr, "@ 0x%08zX  UnknownHeader0D01\n", offset);
				offset += sizeof(UnknownHeader0D01);
				break;
			case 0x0F01:
				// MonthNumHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  MonthNumHeader\n", offset);
				offset += sizeof(MonthNumHeader);
				break;
			case 0x1201:
				// BarDisplayHeader
				BarDisplayHeader * bdh = (BarDisplayHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "@ 0x%08zX  BarDisplayHeader. subtype: %u. count: %u.\n", offset, bdh->subtype, bdh->count);
				offset += sizeof(BarDisplayHeader) + sizeof(OffsetWidthHeight) * (bdh->count-1);
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
