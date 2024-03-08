// dump.h
// dump image/binary data to file

//----------------------------------------------------------------------------
//  IMAGE DUMP FORMAT
//----------------------------------------------------------------------------

typedef enum _Format {
	FMT_RAW = 0,
	FMT_SEMI = 1,
	FMT_BMP = 2,
} Format;

//----------------------------------------------------------------------------
//  DUMP FUNCTIONS
//----------------------------------------------------------------------------

int dumpImage(const char * filename, u8 * srcData, const size_t width, const size_t height, const Format format);
int dumpBlob(const char * fileName, const u8 * srcData, size_t length);
