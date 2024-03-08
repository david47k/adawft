// bytes
// a struct and wrapper around malloc/free that stores size

//----------------------------------------------------------------------------
//  EXPORTED STRUCTS
//----------------------------------------------------------------------------

typedef struct _Bytes {
    size_t size;
    u8 data[16];        // cover weird padding/align situations
} Bytes;

//----------------------------------------------------------------------------
//  EXPORTED FUNCTIONS
//----------------------------------------------------------------------------

Bytes * newBytesFromFile(const char * filename);
Bytes * newBytesFromMemory(const u8 * data, size_t size);
Bytes * deleteBytes(Bytes * b);
int saveBytesToFile(const Bytes * b, const char * filename);