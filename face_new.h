
#include "types.h"

//----------------------------------------------------------------------------
//  BINARY FILE STRUCTURE FOR 'NEW' MO YOUNG / DA FIT WATCH FACES
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
	u16 unknown2;			// 0 or 1 or 2
	u16 unknown3;			// 8C 00
	u16 unknown4;			// A3 00
	u16 dhOffset;			// Offset of the DigitsHeader(s). Usually 0x0010. Seen as 0x0000 for an analog-only watchface using API10.
	u16 bhOffset;			// Offset of the background image (a StaticHeader)
} FaceHeaderN;

// DigitsHeader(s) are typically located between the FaceHeader and the background image header

// Digital clocks have a DigitsHeader, Analog-Only clocks don't.
// Can be time (HHMM) digits, or day number (DD) digits
typedef struct _DigitsHeader {
	u16 type;				// 0x0101
	u8 subtype;				// 0 for Time digits, 1 for DayNum digits, 2 for Steps? digits?
	OffsetWidthHeight owh[10];	// Offset, Width and Height of all the digit images 0-9.
	u8 unknown2[2];			// 0
} DigitsHeader;

// StaticHeader is for static images (e.g. the background)
typedef struct _StaticHeader {
	u16 type;				// 0x0001
	XY xy;					// 0, 0 for background
	u32 offset;				// offset of image data
	u16 width;				// width of image
	u16 height;				// height of image
} StaticHeader;

// TimeHeader is the location of the time (HHMM) digits on the screen
typedef struct _TimeHeader {
	u16 type;				// 0x0201
	u8 unknown[4];			// 0 or 1, probably which digit font set to use for each digit
	XY xy[4];				// x and y position of the four time digits HHMM
	u8 padding[12];			// 0
} TimeHeader;

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
	u8 digitSet;			// suspected digit set # (font)
	u8 justification;		// suspect justification (L, R, C)
	XY xy;					// xy, centered text in this case
	u8 unknown2[18];		// 0
} HeartRateNumHeader;

// Number of steps done today
typedef struct _StepsNumHeader {
	u16 type;				// 0x0701
	u8 digitSet;			// suspected digit set # (font)
	u8 justification;		// suspect justification (L, R, C)
	XY xy;					// X and Y of the steps number
	u8 unknown2[18];		// 0
} StepsNumHeader;

// KCal displayed as a number
typedef struct _KCalHeader {
	u16 type;				// 0x0901
	u8 digitSet;			// suspected digit set # (font)
	u8 justification;			// suspect justification (L, R, C)
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
typedef struct _DayNumHeader {
	u16 type;				// 0x0D01
	u8 digitSet;			// suspected digit set # (font)
	u8 justification;		// suspect justification (L, R, C)
	XY xy[2];				// XY of each digit in the day number (of the month)
} DayNumHeader;

// Month as a number
typedef struct _MonthNumHeader {
	u16 type;				// 0x0F01
	u8 digitSet;			// suspected digit set # (font)
	u8 justification;		// suspect justification (L, R, C)
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

// Used when the minute digits are different to the hour digits (0x1401)
// Unknown use: (0xEC02)
typedef struct _AltDigitsHeader {
	u16 type;				// 0x1401 (typ. mins) or 0xEC02 (??) or 0x4C01 (??)
	u8 hiBytesOffset0[3];		// Looks like they stuffed up this item and didn't allow enough bytes to store the full offset!
	u16 width0;				// width of the first digit
	u16 height0;			// height of the first digit
	OffsetWidthHeight owh[9];	// For the rest of the digits!
	u8 loByteOffset0;		// The low byte of the offset for the first digit!
	u8 unknown;				// 0
} AltDigitsHeader;

// We can remap the above to a saner format
typedef struct _AltDigitsHeaderSane {
	u16 type;				// 0x1401
	OffsetWidthHeight owh[10];	// For all the digits
	u8 unknown;				// 0
} AltDigitsHeaderSane;

typedef struct _WeatherHeader {
	u16 type;				// 0x1B01
	u8 subtype;				// subtype or count? e.g. 9
	XY xy;
	OffsetWidthHeight owh[9];	
} WeatherHeader;

typedef struct _Unknown1D01 {
	u16 type;				// 0x1D01
	u8 unknown;				// 2
} Unknown1D01;

typedef struct _Unknown2301 {
	u16 type;				// 0x2301
	u32 offset;
	u16 width;
	u16 height;
} Unknown2301;

#pragma pack (pop)
