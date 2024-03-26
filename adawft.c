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
#include <stdarg.h>

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
void nullcheck(void * ptr, ...) {
	if(ptr==NULL) {
		printf("ERROR: Null pointer (out of memory?)\n");
		exit(1);
	}
	va_list args;
    va_start(args, ptr);
	ptr = va_arg(args, void *);
	if(ptr==NULL) {
		printf("ERROR: Null pointer (out of memory?)\n");
		exit(1);
	}
	va_end(args);
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
		if(streq(argv[i], "--bin")) {
			format = FMT_BIN;
		} else if(streq(argv[i], "--raw")) {
			format = FMT_RAW;
		} else if(streq(argv[i], "--bmp")) {
			format = FMT_BMP;
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
		dprintf(0, "%s\n","    --bmp                When dumping, dump BMP (windows bitmap) files. Default.");
		dprintf(0, "%s\n","    --raw                When dumping, dump raw (decompressed raw bitmap) files.");
		dprintf(0, "%s\n","    --bin                When dumping, dump binary (rle compressed) files.");
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
	dprintf(2, "unknown         0x%04X\n", h->unknown);
	dprintf(2, "previewOffset   0x%04X\n", h->previewOffset);
	dprintf(2, "previewWidth    %u\n", h->previewWidth);
	dprintf(2, "previewHeight   %u\n", h->previewHeight);
	dprintf(2, "dhOffset        0x%04X\n", h->dhOffset);
	dprintf(2, "bhOffset        0x%04X\n", h->bhOffset);
	
	// Create JSON structure to hold all the data
	cJSON * cj = cJSON_CreateObject();
	nullcheck(cj);
	cJSON_AddStringToObject(cj, "type_str", "extrathunder watchface");
	cJSON_AddNumberToObject(cj, "rev", 0);
	cJSON_AddNumberToObject(cj, "tpls", 0);
	cJSON_AddNumberToObject(cj, "api_ver", h->apiVer);
	cJSON_AddNumberToObject(cj, "unknown", h->unknown);
	//cJSON_AddNumberToObject(cj, "preview_offset", h->previewOffset);
	//cJSON_AddNumberToObject(cj, "preview_w", h->previewWidth);
	//cJSON_AddNumberToObject(cj, "preview_h", h->previewHeight);
	cJSON * cjpreview = cJSON_AddObjectToObject(cj, "preview_img_data");
	cJSON * cjdigits = cJSON_AddArrayToObject(cj, "digits");
	cJSON * cjelements = cJSON_AddArrayToObject(cj, "elements");
	nullcheck(cjpreview, cjdigits, cjelements);

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

	// Save the preview image
	if (dump) {
		sprintf(fnBuf, "preview.%s", dumpFormatStr(format));
		sprintf(&dfnBuf[baseSize], "%s", fnBuf);
		dumpImage(dfnBuf, &fileData[h->previewOffset], h->previewWidth, h->previewHeight, format);
		cJSON_AddNumberToObject(cjpreview, "w", h->previewWidth);
		cJSON_AddNumberToObject(cjpreview, "h", h->previewHeight);
		cJSON_AddStringToObject(cjpreview, "file_name", fnBuf);
	}

	u16 digitsCounter = 0;			// A counter to count digit sets
	u16 imageCounter = 0;			// A counter to count images
	char sbuf[32];					// Buffer for temporary string data

	// First we check the digits headers. They come before the background header

	size_t offset = h->dhOffset;	// Usually 0x10
	
	// Read the introduction to the digit section 0x0101
	u16 digitSectionStart = get_u16(&fileData[offset]);
	if(digitSectionStart != 0x0101) {
		dprintf(0, "WARNING: Unknown start to digits section 0x%04X\n", digitSectionStart);
	}
	offset += 2;
	
	while((offset < h->bhOffset) && (h->dhOffset != 0)) {	// while we are in the digits section, and the digits section actually exists
		DigitsHeader * dh = (DigitsHeader *)&fileData[offset];
		dprintf(2, "@ 0x%08zX  DigitsHeader (%u)\n", offset, dh->digitSet);
		sprintf(sbuf, "digit[%u].owh", dh->digitSet);
		printOwh(&dh->owh[0], 10, sbuf);					// print all the details
		if(dump) {
			cJSON * digits = cJSON_CreateObject();
			cJSON * arr = cJSON_AddArrayToObject(digits, "img_data");
			cJSON_AddNumberToObject(digits, "unknown", dh->unknown);
			for(size_t i=0; i<10; i++) {
				sprintf(fnBuf, "digit_%u_%zu.%s", dh->digitSet, i, dumpFormatStr(format));
				sprintf(&dfnBuf[baseSize], "%s", fnBuf);
				dumpImage(dfnBuf, &fileData[dh->owh[i].offset], dh->owh[i].width, dh->owh[i].height, format);
				cJSON * obj = cJSON_CreateObject();
				cJSON_AddNumberToObject(obj, "w", dh->owh[i].width);
				cJSON_AddNumberToObject(obj, "h", dh->owh[i].height);
				cJSON_AddStringToObject(obj, "file_name", fnBuf);
				cJSON_AddItemToArray(arr, obj);
			}
			cJSON_AddItemToArray(cjdigits, digits);
		}
		offset += sizeof(DigitsHeader);
		digitsCounter++;
	}

	// Now we check the rest of the headers

	offset = h->bhOffset;
	bool more = true;
	while(more) {
		u8 one = fileData[offset];
		u8 e_type = fileData[offset+1];
		if(one==0) {
			// End of header section
			dprintf(2, "@ 0x%08zX  00 (End of headers)\n", offset);
			offset += 2;
			more = false;
			break;
		}
		switch(e_type) {
			case 0x00:
				// ImageHeader for images (including the background)
				ImageHeader * imageh = (ImageHeader *)&fileData[offset];
				if(offset == h->bhOffset) {
					dprintf(2, "@ 0x%08zX  ImageHeader (Background)\n", offset);
				} else {
					dprintf(2, "@ 0x%08zX  ImageHeader\n", offset);
				}
				sprintf(fnBuf, "image_%u.%s", imageCounter++, dumpFormatStr(format));
				dprintf(3, "imageh.one     0x%02X\n", imageh->one);
				dprintf(3, "imageh.xy      %3u, %3u\n", imageh->xy.x, imageh->xy.y);
				dprintf(3, "imageh.owh     0x%08X, %3u, %3u\n", imageh->offset, imageh->width, imageh->height);					
				if(dump) {
					sprintf(&dfnBuf[baseSize], "%s", fnBuf);
					dumpImage(dfnBuf, &fileData[imageh->offset], imageh->width, imageh->height, format);
					cJSON * cjimg = cJSON_CreateObject();
					//cJSON_AddNumberToObject(cjimg, "e_type", imageh->e_type);
					cJSON_AddStringToObject(cjimg, "e_type", "image");
					cJSON_AddNumberToObject(cjimg, "x", imageh->xy.x);
					cJSON_AddNumberToObject(cjimg, "y", imageh->xy.y);
					cJSON * cjimgdata = cJSON_CreateObject();
					cJSON_AddNumberToObject(cjimgdata, "w", imageh->width);
					cJSON_AddNumberToObject(cjimgdata, "h", imageh->height);
					cJSON_AddStringToObject(cjimgdata, "file_name", fnBuf);
					cJSON_AddItemToObject(cjimg, "img_data", cjimgdata);
					cJSON_AddItemToArray(cjelements, cjimg);
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
					//cJSON_AddNumberToObject(cjtime, "e_type", time->e_type);
					cJSON_AddStringToObject(cjtime, "e_type", "time_num");
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
					int unkArr[12];
					for(int i=0; i<12; i++) {
						unkArr[i] = time->unknown[i];
					}
					cjarr = cJSON_CreateIntArray(unkArr, 12);
					cJSON_AddItemToObject(cjtime, "unknown", cjarr);
					cJSON_AddItemToArray(cjelements, cjtime);				
				}								
				offset += sizeof(TimeHeader);
				break;				
			case 0x04:
				// DayNameHeader
				dprintf(2, "@ 0x%08zX  DayNameHeader\n", offset);
				DayNameHeader * dname = (DayNameHeader *)&fileData[offset];
				if(dump) {
					for(size_t i=0; i<7; i++) {
						sprintf(&dfnBuf[baseSize], "dayname_%u_%zu.%s", dname->subtype, i, dumpFormatStr(format));
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
					sprintf(&dfnBuf[baseSize], "batteryfill_%u_.%s", 0, dumpFormatStr(format));
					dumpImage(dfnBuf, &fileData[batteryFill->owh.offset], batteryFill->owh.width, batteryFill->owh.height, format);
					sprintf(&dfnBuf[baseSize], "batteryfill_%u_.%s", 1, dumpFormatStr(format));
					dumpImage(dfnBuf, &fileData[batteryFill->owh1.offset], batteryFill->owh1.width, batteryFill->owh1.height, format);
					sprintf(&dfnBuf[baseSize], "batteryfill_%u_.%s", 2, dumpFormatStr(format));
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
					sprintf(&dfnBuf[baseSize], "hand_%u.%s", hands->subtype, dumpFormatStr(format));
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
						sprintf(&dfnBuf[baseSize], "bardisplay_%u_%zu.%s", bdh->subtype, i, dumpFormatStr(format));
						dumpImage(dfnBuf, &fileData[bdh->owh[i].offset], bdh->owh[i].width, bdh->owh[i].height, format);
					}
				}						
				offset += sizeof(BarDisplayHeader) + sizeof(OffsetWidthHeight) * (bdh->count-1);
				break;
			case 0x1B:
				// WeatherHeader
				WeatherHeader * wh = (WeatherHeader *)&fileData[offset];
				dprintf(2, "@ 0x%08zX  WeatherHeader. count: %u.\n", offset, wh->count);
				if(dump) {
					for(size_t i=0; i<wh->count; i++) {
						sprintf(&dfnBuf[baseSize], "weather_%u_%zu.%s", wh->count, i, dumpFormatStr(format));
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
				dprintf(1, "@ 0x%08zX  DashHeader.\n", offset);
				offset += sizeof(DashHeader);
				break;	
			default:
				// UNKNOWN DATA
				dprintf(0, "@ 0x%08zX  UNKNOWN TYPE 0x%02X (one=0x%02X)\n", offset, e_type, one);
				dprintf(0, "ERROR: Unknown e_type found. Stopping early.\n");
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
