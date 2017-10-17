#ifndef __MP2DEC_BITS_H__
#define __MP2DEC_BITS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mp2DecTypedef.h"

/* buffer, buffer_end and size_in_bits must be present and used by every reader */
typedef struct GetBitContext 
{
    const unsigned char *buffer, *buffer_end;
    unsigned char *buffer_ptr;
    unsigned int cache;
    int bit_count;
    int size_in_bits;
} GetBitContext;

#define bswap_16(x) (((x) & 0x00ff) << 8 | ((x) & 0xff00) >> 8)
#define be2me_16(x) bswap_16(x)

void init_get_bits(GetBitContext *s, unsigned char *buffer, int bit_size);
unsigned int get_bits(GetBitContext *s, int n);

#ifdef __cplusplus
}
#endif

#endif
