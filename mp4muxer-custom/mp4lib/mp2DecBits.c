#include "mp2DecBits.h"

void init_get_bits(GetBitContext *s, unsigned char *buffer, int bit_size)
{
    const int buffer_size= (bit_size+7)>>3;
	
    s->buffer= buffer;
    s->size_in_bits= bit_size;
    s->buffer_end= buffer + buffer_size;
	s->buffer_ptr = buffer;
	s->bit_count = 16;
	s->cache = 0;
	{
		int re_bit_count=s->bit_count; 
		int re_cache= s->cache; 
		unsigned char* re_buffer_ptr=s->buffer_ptr; 

		if(re_bit_count >= 0)
		{ 
			re_cache+= (int)be2me_16(*(unsigned short*)re_buffer_ptr) << re_bit_count; 
			re_buffer_ptr += 2; 
			re_bit_count-= 16; 
		}
		if(re_bit_count >= 0)
		{ 
			re_cache+= (int)be2me_16(*(unsigned short*)re_buffer_ptr) << re_bit_count; 
			re_buffer_ptr += 2; 
			re_bit_count-= 16; 
		}
		s->bit_count= re_bit_count; 
		s->cache= re_cache; 
		s->buffer_ptr= re_buffer_ptr; 
	}
}

unsigned int get_bits(GetBitContext *s, int n)
{
    register int tmp;
	int re_bit_count=s->bit_count; 
	int re_cache= s->cache; 
	unsigned char* re_buffer_ptr=s->buffer_ptr; 

	if(re_bit_count >= 0)
	{ 
		re_cache+= (int)be2me_16(*(unsigned short*)re_buffer_ptr) << re_bit_count; 
		//((uint16_t*)re_buffer_ptr)++; 
		re_buffer_ptr += 2; 
		re_bit_count-= 16; 
	}

	tmp= (((unsigned int)(re_cache))>>(32-(n)));

	re_cache <<= (n);
	re_bit_count += (n);	
	s->bit_count= re_bit_count; 
	s->cache= re_cache; 
	s->buffer_ptr= re_buffer_ptr; 
	
	return tmp;
}


