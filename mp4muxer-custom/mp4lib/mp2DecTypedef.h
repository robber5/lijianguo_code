#ifndef __MP2DEC_TYPEDEF_H__
#define __MP2DEC_TYPEDEF_H__

#define BOOL int
#define TRUE 1
#define FALSE 0

#define SWAP32(val) (unsigned int)((((unsigned int)(val)) & 0x000000FF)<<24|	\
					(((unsigned int)(val)) & 0x0000FF00)<<8 |	\
					(((unsigned int)(val)) & 0x00FF0000)>>8 |	\
					(((unsigned int)(val)) & 0xFF000000)>>24)	

#endif

