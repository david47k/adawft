//----------------------------------------------------------------------------
//  BASIC TYPES
//----------------------------------------------------------------------------

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;

//----------------------------------------------------------------------------
//  BASIC BYTE-ORDER MACROS
//----------------------------------------------------------------------------

// Swap byte order on u16
#define swap_bo_u16(input) (u16)((input & 0xFF) << 8) | ((input & 0xFF00) >> 8)

// gets a LE u16, without care for alignment or system byte order
#define get_u16(p) (u16)( ((const u8*)p)[0] | (((const u8*)p)[1] << 8) )

// gets a LE u32, without care for alignment or system byte order
#define get_u32(p) (u32)( ((const u8*)p)[0] | (((const u8*)p)[1] << 8) | (((const u8*)p)[2] << 16) | (((const u8*)p)[3] << 24) )

// sets a LE u16, without care for alignment or system byte order
void set_u16(u8 * p, u16 v);

// return 0 for big endian, 1 for little endian.
int systemIsLittleEndian();
