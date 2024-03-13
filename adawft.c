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
#include "cjson/cJSON.h"


//----------------------------------------------------------------------------
//  API_VER_INFO - information about what is supported at each API level
//----------------------------------------------------------------------------

typedef struct _ApiVerInfo {
	u8 apiVer;
	const char * description;
} ApiVerInfo;

static const ApiVerInfo API_VER_INFO[] = {
	{  2, "Time only" },
	{  4, "Time, DayName" },
	{ 10, "Hands only" },
	{ 13, "Time, DayName, DayNum" },
	{ 15, "Time, DayName, DayNum, MonthNum, StepsNum" },
	{ 18, "Time, Hands, DayName, DayNum, HeartRateNum, BarDisplay(0,2,5,6), KCalNum, StepsNum" },
	{ 20, "Same as 18? plus BatteryFill" },
	{ 29, "Same as 18? plus Weather, Unknown1D01" },
	{ 35, "Same as 18? plus BarDisplay(3), Unknown2301" },
};


//----------------------------------------------------------------------------
//  PRINT OFFSET, WIDTH, HEIGHT ARRAY TO STRING
//----------------------------------------------------------------------------

void printOwh(OffsetWidthHeight * owh, size_t count, const char * name) {
	for(size_t i=0; i<count; i++) {
		dprintf(3, "%s[%zu]    0x%08X, %3u, %3u\n", name, i, owh[i].offset, owh[i].width, owh[i].height);
	}
}

//----------------------------------------------------------------------------
//  NULLCHECK - check for null errors (e.g. out of memory)
//----------------------------------------------------------------------------
void nullcheck(void * ptr) {
	if(ptr==NULL) {
		printf("ERROR: Null pointer (out of memory?)\n");
		exit(1);
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

	// check byte order
	if(!systemIsLittleEndian()) {
		dprintf(0, "Sorry, this system is big-endian, and this program has only been designed for little-endian systems.\n");
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
		} else if(streqn(argv[i], "--debug", 6)) {
			DEBUG_LEVEL = 3;
			if(strlen(argv[i]) >= 9 && argv[i][7] == '=') {
				DEBUG_LEVEL = atoi(&argv[i][8]);
			}
		}else if(streqn(argv[i], "--help", 6)) {
			showHelp = true;
		} else if(streqn(argv[i], "--", 2)) {
			dprintf(0, "ERROR: Unknown option: %s\n", argv[i]);
			showHelp = true;
		} else {
			// must be fileName
			if(!fileNameSet) {
				fileName = argv[i];
				fileNameSet = true;
			} else {
				dprintf(0, "WARNING: Ignored unknown parameter: %s\n", argv[i]);
			}
		}
	}

	// display basic program header
    dprintf(1, "\n%s\n\n","adawft: Alternate Da Watch Face Tool for MO YOUNG / DA FIT binary watch face files.");
 
	// display help
    if(argc<2 || showHelp) {
		dprintf(0, "Usage:   %s [OPTIONS] FILENAME\n\n",basename);
		dprintf(0, "%s\n","  OPTIONS");
		dprintf(0, "%s\n","    --dump=FOLDERNAME    Dump data to folder. Folder name defaults to 'dump'.");
		dprintf(0, "%s\n","    --raw                When dumping, dump raw (compressed) files.");
		dprintf(0, "%s\n","    --semiraw            When dumping, dump semiraw (decompressed raw bitmap) files.");
		dprintf(0, "%s\n","    --debug=LEVEL        Print more debug info. Range 0 to 3.");
		dprintf(0, "%s\n","  FILENAME               Binary watch face file for input.");
		dprintf(0, "\n");
		return 0;
    }

	// Open the binary input file
	Bytes * bytes = newBytesFromFile(fileName);
	if(bytes == NULL) {
		dprintf(0, "ERROR: Failed to read file into memory.\n");
		return 1;
	}

	u8 * fileData = bytes->data;
	size_t fileSize = bytes->size;

	// Check file size
	if(fileSize < sizeof(FaceHeaderN)) {
		dprintf(0, "ERROR: File is less than the header size (%zu bytes)!\n", sizeof(FaceHeaderN));
		deleteBytes(bytes);
		return 1;
	}

	// Load header struct from file
	FaceHeaderN * h = (FaceHeaderN *)&fileData[0];		// interpret it directly

	// Print header info	
	dprintf(2, "apiVer          %u\n", h->apiVer);
	dprintf(2, "unknown0        0x%04X\n", h->unknown0);
	dprintf(2, "unknown1        0x%04X\n", h->unknown1);
	dprintf(2, "unknown2        %u\n", h->unknown2);
	dprintf(2, "previewWidth    %u\n", h->previewWidth);
	dprintf(2, "previewHeight   %u\n", h->previewHeight);
	dprintf(2, "dhOffset        0x%04X\n", h->dhOffset);
	dprintf(2, "bhOffset        0x%04X\n", h->bhOffset);
	
	// Everything should have a background (I hope!)
	ImageHeader * bgh = (ImageHeader *)&fileData[h->bhOffset];
	dprintf(2, "bgh.xy          %3u, %3u\n", bgh->xy.x, bgh->xy.y);
	dprintf(2, "bgh.owh         0x%08X, %3u, %3u\n", bgh->offset, bgh->width, bgh->height);

	// Create JSON structure to hold all the data
	cJSON * cj = cJSON_CreateObject();
	nullcheck(cj);
	cJSON_AddStringToObject(cj, "type", "extrathunder watchface");
	cJSON_AddNumberToObject(cj, "rev", 0);
	cJSON_AddNumberToObject(cj, "tpls", 0);
	cJSON_AddNumberToObject(cj, "api_ver", h->apiVer);
	cJSON_AddNumberToObject(cj, "unknown0", h->unknown0);
	cJSON_AddNumberToObject(cj, "unknown1", h->unknown1);
	cJSON_AddNumberToObject(cj, "unknown2", h->unknown2);
	cJSON_AddNumberToObject(cj, "preview_width", h->previewWidth);
	cJSON_AddNumberToObject(cj, "preview_height", h->previewHeight);
	cJSON * cjdigits = cJSON_AddArrayToObject(cj, "digits");
	cJSON * cjwidgets = cJSON_AddArrayToObject(cj, "widgets");
	nullcheck(cjdigits);
	nullcheck(cjwidgets);

	// Create a buffer for storing the dump filenames
	char dfnBuf[1024];
	char fnBuf[32];
	snprintf(dfnBuf, sizeof(dfnBuf), "%s%s", folderName, DIR_SEPERATOR);
	size_t baseSize = strlen(dfnBuf);
	if(baseSize + 32 >= sizeof(dfnBuf)) {
		dprintf(0, "ERROR: dfnBuf too small!\n");
		deleteBytes(bytes);
		return 1;
	}

	// Store some counters for filenames
	u16 imageCounter = 0;

	// Buffer for temporary string data
	char sbuf[32];

	// First we check the digits headers. They come before the background header

	bool more = true;
	if(h->dhOffset == 0) {
		more = false; 	// no digits headers to process
	}

	size_t offset = h->dhOffset;	// Usually 0x10
	u16 digitSectionStart = get_u16(&fileData[offset]);
	offset += 2;
	if(digitSectionStart != 0x0101) {
		dprintf(0, "WARNING: Unknown start to digits section 0x%04X\n", digitSectionStart);
	}
	u16 digitsCounter = 0;

	while(more) {
		DigitsHeader * dh = (DigitsHeader *)&fileData[offset];
		dprintf(2, "@ 0x%08zX  DigitsHeader (%u)\n", offset, dh->digitSet);
		// what digit font is it
		sprintf(sbuf, "digit[%u].owh", dh->digitSet);
		// print all the details
		printOwh(&dh->owh[0], 10, sbuf);
		if(dump) {
			cJSON * digits = cJSON_CreateObject();
			cJSON_AddNumberToObject(digits, "set", dh->digitSet);
			cJSON * arr = cJSON_AddArrayToObject(digits, "files");
			cJSON_AddNumberToObject(digits, "unknown", dh->unknown);
			for(size_t i=0; i<10; i++) {
				sprintf(fnBuf, "digit_%u_%zu.%s", dh->digitSet, i, (format==FMT_BMP?"bmp":"raw"));
				sprintf(&dfnBuf[baseSize], "%s", fnBuf);
				dumpImage(dfnBuf, &fileData[dh->owh[i].offset], dh->owh[i].width, dh->owh[i].height, format);
				if(format==FMT_BMP) {
					cJSON * obj = cJSON_CreateObject();
					cJSON_AddStringToObject(obj, "type", "bmp");
					cJSON_AddStringToObject(obj, "name", fnBuf);					
					cJSON_AddItemToArray(arr, obj);
				}
			}
			cJSON_AddItemToArray(cjdigits, digits);
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
		u8 one = fileData[offset];
		u8 type = fileData[offset+1];
		if(one==0) {
			// End of header section
			dprintf(2, "@ 0x%08zX  00 (End of headers)\n", offset);
			offset += 2;
			more = false;
			break;
		}
		switch(type) {
			case 0x00:
				// ImageHeader for images
				ImageHeader * imageh = (ImageHeader *)&fileData[offset];
				if(offset == h->bhOffset) {
					dprintf(2, "@ 0x%08zX  ImageHeader (Background)\n", offset);
					sprintf(fnBuf, "background.%s", (format==FMT_BMP?"bmp":"raw"));
				} else {
					dprintf(2, "@ 0x%08zX  ImageHeader\n", offset);
					sprintf(fnBuf, "image_%u.%s", imageCounter++, (format==FMT_BMP?"bmp":"raw"));
				}
				dprintf(3, "imageh.one     0x%02X\n", imageh->one);
				dprintf(3, "imageh.xy      %3u, %3u\n", imageh->xy.x, imageh->xy.y);
				dprintf(3, "imageh.owh     0x%08X, %3u, %3u\n", imageh->offset, imageh->width, imageh->height);					
				if(dump) {
					sprintf(&dfnBuf[baseSize], "%s", fnBuf);
					dumpImage(dfnBuf, &fileData[imageh->offset], imageh->width, imageh->height, format);
					if(format==FMT_BMP) {
						cJSON * cjimage = cJSON_CreateObject();
						cJSON_AddStringToObject(cjimage, "name", fnBuf);
						cJSON_AddStringToObject(cjimage, "type", "bmp");
						cJSON_AddNumberToObject(cjimage, "w", imageh->width);
						cJSON_AddNumberToObject(cjimage, "h", imageh->height);
						if(offset == h->bhOffset) {
							cJSON_AddItemToObject(cj, "background", cjimage);
						} else {
							cJSON_AddItemToArray(cjwidgets, cjimage);
						}
					}
				}				
				offset += sizeof(ImageHeader);
				break;
			case 0x02:
				// TimeHeader
				dprintf(2, "@ 0x%08zX  TimeHeader\n", offset);
				TimeHeader * time = (TimeHeader *)&fileData[offset];
				dprintf(3, "                digitSet: %u %u %u %u\n", time->digitSet[0], time->digitSet[1], time->digitSet[2], time->digitSet[3]);
				if(dump) {
					cJSON * cjtime = cJSON_CreateObject();
					cJSON_AddNumberToObject(cjtime, "type", time->type);
					int iArr[4] = { time->digitSet[0], time->digitSet[1], time->digitSet[2], time->digitSet[3] };
					cJSON * cjarr = cJSON_CreateIntArray(iArr, 4);
					cJSON_AddItemToObject(cjtime, "digit_sets", cjarr);
					cjarr = cJSON_CreateArray();
					for(int i=0; i<4; i++) {
						cJSON * xy = cJSON_CreateObject();
						cJSON_AddNumberToObject(xy, "x", time->xy[i].x);
						cJSON_AddNumberToObject(xy, "y", time->xy[i].y);
						cJSON_AddItemToArray(cjarr, xy);
					}
					cJSON_AddItemToObject(cjtime, "xys", cjarr);
					cJSON_AddItemToArray(cjwidgets, cjtime);				
				}								
				offset += sizeof(TimeHeader);
				break;				
			case 0x04:
				// DayNameHeader
				dprintf(2, "@ 0x%08zX  DayNameHeader\n", offset);
				DayNameHeader * dname = (DayNameHeader *)&fileData[offset];
				if(dump) {
					for(size_t i=0; i<7; i++) {
						sprintf(&dfnBuf[baseSize], "dayname_%u_%zu.%s", dname->subtype, i, (format==FMT_BMP?"bmp":"raw"));
						dumpImage(dfnBuf, &fileData[dname->owh[i].offset], dname->owh[i].width, dname->owh[i].height, format);
					}
				}				
				offset += sizeof(DayNameHeader);
				break;
			case 0x05:
				// BatteryFillHeader
				dprintf(2, "@ 0x%08zX  BatteryFillHeader\n", offset);
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
			case 0x06:
				// HeartRateNumHeader
				dprintf(2, "@ 0x%08zX  HeartRateNumHeader\n", offset);
				HeartRateNumHeader * hrn = (HeartRateNumHeader *)&fileData[offset];
				dprintf(3, "                digitSet: %u, justification: %u\n", hrn->digitSet, hrn->justification);
				offset += sizeof(HeartRateNumHeader);
				break;
			case 0x07:
				// StepsNumHeader
				dprintf(2, "@ 0x%08zX  StepsNumHeader\n", offset);
				StepsNumHeader * sn = (StepsNumHeader *)&fileData[offset];
				dprintf(3, "                digitSet: %u, justification: %u\n", sn->digitSet, sn->justification);
					offset += sizeof(StepsNumHeader);
				break;
			case 0x09:
				// KCalNumHeader
				dprintf(2, "@ 0x%08zX  KCalNumHeader\n", offset);
				offset += sizeof(KCalNumHeader);
				break;
			case 0x0A:
				// HandsHeader
				dprintf(2, "@ 0x%08zX  HandsHeader\n", offset);
				HandsHeader * hands = (HandsHeader *)&fileData[offset];				
				if(dump) {		
					sprintf(&dfnBuf[baseSize], "hand_%u.%s", hands->subtype, (format==FMT_BMP?"bmp":"raw"));
					dumpImage(dfnBuf, &fileData[hands->offset], hands->width, hands->height, format);
				}
				offset += sizeof(HandsHeader);
				break;
			case 0x0D:
				// DayNumHeader
				dprintf(2, "@ 0x%08zX  DayNumHeader\n", offset);
				DayNumHeader * dn = (DayNumHeader *)&fileData[offset];
				dprintf(3, "                digitSet: %u, justification: %u\n", dn->digitSet, dn->justification);
				offset += sizeof(DayNumHeader);
				break;
			case 0x0F:
				// MonthNumHeader
				dprintf(2, "@ 0x%08zX  MonthNumHeader\n", offset);
				MonthNumHeader * mn = (MonthNumHeader *)&fileData[offset];
				dprintf(3, "                digitSet: %u, justification: %u\n", mn->digitSet, mn->justification);
				offset += sizeof(MonthNumHeader);
				break;
			case 0x12:
				// BarDisplayHeader
				BarDisplayHeader * bdh = (BarDisplayHeader *)&fileData[offset];
				dprintf(2, "@ 0x%08zX  BarDisplayHeader. subtype: %u. count: %u.\n", offset, bdh->subtype, bdh->count);
				if(dump) {
					for(size_t i=0; i<bdh->count; i++) {
						sprintf(&dfnBuf[baseSize], "bardisplay_%u_%zu.%s", bdh->subtype, i, (format==FMT_BMP?"bmp":"raw"));
						dumpImage(dfnBuf, &fileData[bdh->owh[i].offset], bdh->owh[i].width, bdh->owh[i].height, format);
					}
				}						
				offset += sizeof(BarDisplayHeader) + sizeof(OffsetWidthHeight) * (bdh->count-1);
				break;
			case 0x1B:
				// WeatherHeader
				WeatherHeader * wh = (WeatherHeader *)&fileData[offset];
				dprintf(2, "@ 0x%08zX  WeatherHeader. subtype: %u.\n", offset, wh->subtype);
				if(dump) {
					for(size_t i=0; i<wh->subtype; i++) {
						sprintf(&dfnBuf[baseSize], "weather_%u_%zu.%s", wh->subtype, i, (format==FMT_BMP?"bmp":"raw"));
						dumpImage(dfnBuf, &fileData[wh->owh[i].offset], wh->owh[i].width, wh->owh[i].height, format);
					}
				}										
				offset += sizeof(WeatherHeader);
				break;
			case 0x1D:
				// 
				Unknown1D01 * u1h = (Unknown1D01 *)&fileData[offset];
				dprintf(1, "@ 0x%08zX  Unknown1D01Header. unknown: %u.\n", offset, u1h->unknown);
				offset += sizeof(Unknown1D01);
				break;
			case 0x23:
				// Unknown2301 * u2h = (Unknown2301 *)&fileData[offset];
				dprintf(1, "@ 0x%08zX  Unknown2301Header.\n", offset);
				offset += sizeof(Unknown2301);
				break;	
			default:
				// UNKNOWN DATA
				dprintf(0, "@ 0x%08zX  UNKNOWN TYPE 0x%02X (one=0x%02X)\n", offset, type, one);
				dprintf(0, "ERROR: Unknown type found. Stopping early.\n");
				more = false;
				break;
		}
	}

	// if we are dumping, then save the json file
	if(dump) {
		// json filename
		dprintf(2, "Saving JSON data... ");
		sprintf(fnBuf, "watchface.json");
		sprintf(&dfnBuf[baseSize], "%s", fnBuf);
		char * jsonString = cJSON_Print(cj);
		dumpBlob(dfnBuf, (u8 *)jsonString, strlen(jsonString));
		free(jsonString);
		dprintf(2, "done.\n");
	}

	// clean up
	cJSON_Delete(cj);
	deleteBytes(bytes);
	dprintf(1, "\ndone.\n\n");

    return 0; // SUCCESS
}
