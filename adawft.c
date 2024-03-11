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
#include "face_new.h"
#include "adawft.h"
#include "bytes.h"
#include "bmp.h"
#include "dump.h"
#include "strutil.h"


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
	sscatprintf(watchFaceStr, "previewWidth    %u\n", h->previewWidth);
	sscatprintf(watchFaceStr, "previewHeight   %u\n", h->previewHeight);
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

	// First we check the digits headers. They come before the background header
	
	bool more = true;
	if(h->dhOffset == 0) {
		more = false; 	// no digits headers to process
	}

	size_t offset = h->dhOffset;	// Usually 0x10
	u16 digitStart = get_u16(&fileData[offset]);
	offset += 2;
	if(digitStart == 0x0101) {
		printf("WARNING: Unknown start to digits section 0x%04X\n", digitStart);
	}
	u16 digitsCounter = 0;

	while(more) {
		DigitsHeader * dh = (DigitsHeader *)&fileData[offset];
		sscatprintf(watchFaceStr, "@ 0x%08zX  DigitsHeader (%u)\n", offset, dh->digitSet);
		// what digit font is it
		sprintf(sbuf, "digit[%u].owh", dh->digitSet);
		// print all the details
		printOwh(&dh->owh[0], 10, sbuf, watchFaceStr, sizeof(watchFaceStr));
		if(dump) {
			for(size_t i=0; i<10; i++) {
				//sscatprintf(watchFaceStr, "dh.owh[%zu]    0x%08X, %3u, %3u\n", i, dh->owh[i].offset,  dh->owh[i].width, dh->owh[i].height);
				sprintf(&dfnBuf[baseSize], "digit_%u_%zu.%s", dh->digitSet, i, (format==FMT_BMP?"bmp":"raw"));
				dumpImage(dfnBuf, &fileData[dh->owh[i].offset], dh->owh[i].width, dh->owh[i].height, format);
			}					
		}
		offset += sizeof(DigitsHeader);
		digitsCounter++;
		if(offset >= h->bhOffset) {
			more = false;
		}
	}

	// Now we check the rest of the headers
	offset = h->bhOffset;
	more = true;
	while(more) {
		u16 type = get_u16(&fileData[offset]);
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
						sprintf(&dfnBuf[baseSize], "static_%u.%s", staticCounter++, (format==FMT_BMP?"bmp":"raw"));
						dumpImage(dfnBuf, &fileData[statich->offset], statich->width, statich->height, format);
					}
				}
				offset += sizeof(StaticHeader);
				break;
			case 0x0201:
				// TimeHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  TimeHeader\n", offset);
				TimeHeader * time = (TimeHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "                digitSet: %u %u %u %u\n", time->digitSet[0], time->digitSet[1], time->digitSet[2], time->digitSet[3]);
				offset += sizeof(TimeHeader);
				break;				
			case 0x0401:
				// DayNameHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  DayNameHeader\n", offset);
				DayNameHeader * dname = (DayNameHeader *)&fileData[offset];
				if(dump) {
					for(size_t i=0; i<7; i++) {
						sprintf(&dfnBuf[baseSize], "dayname_%u_%zu.%s", dname->subtype, i, (format==FMT_BMP?"bmp":"raw"));
						dumpImage(dfnBuf, &fileData[dname->owh[i].offset], dname->owh[i].width, dname->owh[i].height, format);
					}
				}				
				offset += sizeof(DayNameHeader);
				break;
			case 0x0501:
				// BatteryFillHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  BatteryFillHeader\n", offset);
				BatteryFillHeader * batteryFill = (BatteryFillHeader *)&fileData[offset];
				if(dump) {
					sprintf(&dfnBuf[baseSize], "batteryfill_%u_.%s", 0, (format==FMT_BMP?"bmp":"raw"));
					dumpImage(dfnBuf, &fileData[batteryFill->owh.offset], batteryFill->owh.width, batteryFill->owh.height, format);
					sprintf(&dfnBuf[baseSize], "batteryfill_%u_.%s", 1, (format==FMT_BMP?"bmp":"raw"));
					dumpImage(dfnBuf, &fileData[batteryFill->owh1.offset], batteryFill->owh1.width, batteryFill->owh1.height, format);
					sprintf(&dfnBuf[baseSize], "batteryfill_%u_.%s", 2, (format==FMT_BMP?"bmp":"raw"));
					dumpImage(dfnBuf, &fileData[batteryFill->owh2.offset], batteryFill->owh2.width, batteryFill->owh2.height, format);
				}
				offset += sizeof(BatteryFillHeader);
				break;
			case 0x0601:
				// HeartRateNumHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  HeartRateNumHeader\n", offset);
				HeartRateNumHeader * hrn = (HeartRateNumHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "                digitSet: %u, justification: %u\n", hrn->digitSet, hrn->justification);
				offset += sizeof(HeartRateNumHeader);
				break;
			case 0x0701:
				// StepsNumHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  StepsNumHeader\n", offset);
				StepsNumHeader * sn = (StepsNumHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "                digitSet: %u, justification: %u\n", sn->digitSet, sn->justification);
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
				HandsHeader * hands = (HandsHeader *)&fileData[offset];				
				if(dump) {		
					sprintf(&dfnBuf[baseSize], "hand_%u.%s", hands->subtype, (format==FMT_BMP?"bmp":"raw"));
					dumpImage(dfnBuf, &fileData[hands->offset], hands->width, hands->height, format);
				}
				offset += sizeof(HandsHeader);
				break;
			case 0x0D01:
				// DayNumHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  DayNumHeader\n", offset);
				DayNumHeader * dn = (DayNumHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "                digitSet: %u, justification: %u\n", dn->digitSet, dn->justification);
				offset += sizeof(DayNumHeader);
				break;
			case 0x0F01:
				// MonthNumHeader
				sscatprintf(watchFaceStr, "@ 0x%08zX  MonthNumHeader\n", offset);
				MonthNumHeader * mn = (MonthNumHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "                digitSet: %u, justification: %u\n", mn->digitSet, mn->justification);
				offset += sizeof(MonthNumHeader);
				break;
			case 0x1201:
				// BarDisplayHeader
				BarDisplayHeader * bdh = (BarDisplayHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "@ 0x%08zX  BarDisplayHeader. subtype: %u. count: %u.\n", offset, bdh->subtype, bdh->count);
				if(dump) {
					for(size_t i=0; i<bdh->count; i++) {
						sprintf(&dfnBuf[baseSize], "bardisplay_%u_%zu.%s", bdh->subtype, i, (format==FMT_BMP?"bmp":"raw"));
						dumpImage(dfnBuf, &fileData[bdh->owh[i].offset], bdh->owh[i].width, bdh->owh[i].height, format);
					}
				}						
				offset += sizeof(BarDisplayHeader) + sizeof(OffsetWidthHeight) * (bdh->count-1);
				break;
			case 0x1B01:
				// WeatherHeader
				WeatherHeader * wh = (WeatherHeader *)&fileData[offset];
				sscatprintf(watchFaceStr, "@ 0x%08zX  WeatherHeader. subtype: %u.\n", offset, wh->subtype);
				if(dump) {
					for(size_t i=0; i<wh->subtype; i++) {
						sprintf(&dfnBuf[baseSize], "weather_%u_%zu.%s", wh->subtype, i, (format==FMT_BMP?"bmp":"raw"));
						dumpImage(dfnBuf, &fileData[wh->owh[i].offset], wh->owh[i].width, wh->owh[i].height, format);
					}
				}										
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
			default:
				// UNKNOWN DATA
				sscatprintf(watchFaceStr, "@ 0x%08zX  UNKNOWN TYPE 0x%04X\n", offset, type);
				sscatprintf(watchFaceStr, "ERROR: Unknown type found. Stopping early.\n");
				more = false;
				break;
		}
	}

	// display all the important data
	printf("%s", watchFaceStr);		

	// clean up
	deleteBytes(bytes);
	printf("\ndone.\n\n");

    return 0; // SUCCESS
}
