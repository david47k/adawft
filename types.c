// types.c
// byte order and basic types

#include <stdint.h>
#include "types.h"

// sets a LE u16, without care for alignment or system byte order
void set_u16(u8 * p, u16 v) {
    p[0] = v&0xFF;
    p[1] = v>>8;
}

// return 0 for big endian, 1 for little endian.
int systemIsLittleEndian() {				
    volatile uint32_t i=0x01234567;
    return (*((volatile uint8_t*)(&i))) == 0x67;
}
