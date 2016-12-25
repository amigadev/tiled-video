#include "bits.h"

#include <stdio.h>

void bits_init_write(bits_t* bits)
{
	buffer_init(&(bits->buf), 1);
	bits->data = 0;
	bits->count = 0;
	bits->offset = ~0UL;
}

void bits_init_read(bits_t* bits, const uint8_t* data, size_t length)
{
	buffer_init(&(bits->buf), 1);
    buffer_set(&(bits->buf), data, length);
	bits->data = 0;
	bits->count = 0;
	bits->offset = 0;
}

void bits_flush(bits_t* bits)
{
	if (bits->count > 0)
	{
		uint8_t* s = buffer_alloc(&(bits->buf), 1);
		*s = bits->data;
	}

	bits->data = 0;
	bits->count = 0;
}

void bits_release(bits_t* bits)
{
    if (bits->offset == ~0UL)
    {
        buffer_release(&(bits->buf));
    }
}

void bits_reset(bits_t* bits)
{
    if (bits->offset == ~0UL)
    {
    	buffer_reset(&(bits->buf));
    }
    else
    {
        bits->offset = 0;
    }
	bits->data = 0;
	bits->count = 0;
}

#define min(a,b) ((a) < (b) ? (a) : (b))

void bits_write(bits_t* bits, uint32_t data, size_t count)
{
    uint8_t bdata = bits->data;
    size_t bcount = bits->count;

    while (count > 0)
    {
        size_t size = min(8 - bcount, count);
        uint32_t mask = 0xff >> (8 - size);

        bdata |= (data & mask) << ((8 - bcount) - size);
        bcount += size;

        if (bcount == 8)
        {
            uint8_t* temp = buffer_alloc(&(bits->buf), 1);
            *temp = bdata;

            bdata = 0;
            bcount = 0;
        }

        count -= size;
        data >>= size;
    }

    bits->data = bdata;
    bits->count = bcount;
}

uint32_t bits_read(bits_t* bits, size_t count)
{
    uint32_t out = 0;
    uint8_t bdata = bits->data;
    size_t bcount = bits->count;

    size_t read = 0;
    while (read < count)
    {
        if (bcount == 0)
        {
            bdata = *(const uint8_t*)buffer_get(&(bits->buf), bits->offset++);
            bcount = 8;
        }

        size_t size = min(bcount, (count - read));
        size_t shift = bcount - size;
        uint32_t mask = 0xff >> (8 - size);

        out |= (((bdata >> shift) & mask)) << read;

        read  += size;
        bcount -= size;
    }

    bits->data = bdata;
    bits->count = bcount;

    return out;
}
