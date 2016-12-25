#include "frames.h"
#include "bits.h"

#include <string.h>

void frames_init(frames_t* frames)
{
	buffer_init(&(frames->buffer), sizeof(frame_t));
}

void frames_release(frames_t* frames)
{
	buffer_release(&(frames->buffer));
}

void frames_add(frames_t* frames, const frame_t* in)
{
	frame_t* temp = buffer_alloc(&(frames->buffer), 1);
	memcpy(temp, in, sizeof(frame_t));
}

static uint32_t ti_compress(tile_index_t index, size_t bits)
{
	uint32_t flags = (index & TILE_BITS_MASK) >> (32 - bits);

	uint32_t in = index & ~TILE_BITS_MASK;
	uint32_t out = index & (~0U >> (32-(bits-3)));
    uint32_t result = out | flags;
	return result;
}

static tile_index_t ti_uncompress(uint32_t in, size_t bits)
{
    uint32_t flags = (in & (TILE_BITS_MASK >> (32 - bits))) << (32 - bits);
    uint32_t out = in & (~TILE_BITS_MASK) >> (32 - bits);
    uint32_t result = out | flags;
    return result;
}

size_t frames_load(const buffer_t* in, size_t offset, size_t count, frames_t* frames, size_t tile_bits)
{
    frame_t last;
    memset(&last, 0xff, sizeof(last));

    for (size_t i = 0; i < count; ++i)
    {
        frame_t* frame = buffer_alloc(&(frames->buffer), 1);

        frame_header_t header;
        memcpy(&header, buffer_get(in, offset), sizeof(header));
        offset += sizeof(header);

        bits_t fbits;
        bits_init_read(&fbits, buffer_get(in, offset), header.size);
        offset += header.size;

        for (size_t j = 0; j < FRAME_TILE_COUNT;)
        {
            uint8_t header = bits_read(&fbits, 8);
            uint8_t length = (header & 0x7f) + 1;
            if (header & 0x80)
            {
                memcpy(&(frame->tiles[j]), &(last.tiles[j]), length * sizeof(tile_index_t));
            }
            else
            {
                for (size_t k = 0; k < length; ++k)
                {
                    frame->tiles[j + k] = ti_uncompress(bits_read(&fbits, tile_bits), tile_bits);
                }
            }

            j += length;
        }

        last = *frame;
    }

    return offset;
}

void frames_save(buffer_t* out, const frames_t* frames, size_t tile_bits)
{
	frame_t last;
	memset(&last, 0xff, sizeof(last));

	bits_t fbits;
	bits_init_write(&fbits);

	for (size_t i = 0, n = buffer_count(&(frames->buffer)); i < n; ++i)
	{
		bits_reset(&fbits);
		const frame_t* curr = buffer_get(&(frames->buffer), i);

		const tile_index_t* li = last.tiles;
		const tile_index_t* ci = curr->tiles;
		const tile_index_t* first = NULL;
		int skipping = 0;

		for (size_t i = 0; i < (FRAME_WIDTH / TILE_WIDTH) * (FRAME_HEIGHT / TILE_HEIGHT); ++i, ++li, ++ci)
		{
			if (first && (ci - first) == 128)
			{
			    uint8_t length = ((uint8_t)(ci - first))-1;
				if (skipping)
				{
					bits_write(&fbits, length|0x80, 8);
				}
				else
				{
					bits_write(&fbits, length, 8);
					for (; first < ci; ++first)
						bits_write(&fbits, ti_compress(*first, tile_bits), tile_bits);
				}
				first = NULL; skipping = 0;
			}

			if (!first)
			{
				first = ci;
				skipping = (*ci == *li) ? 1 : 0;
			}
			else
			{
			    uint8_t length = ((uint8_t)(ci - first))-1;
				if (skipping && (*ci != *li))
				{
					bits_write(&fbits, length|0x80, 8);
					first = ci; skipping = 0;
				}
				else if (!skipping && (*ci == *li))
				{
					bits_write(&fbits, length, 8);
					for (; first < ci; ++first)
						bits_write(&fbits, ti_compress(*first, tile_bits), tile_bits);
					first = ci; skipping = 1;
				}
			}
		}

		if (first)
		{
            uint8_t length = ((uint8_t)(ci - first))-1;
			if (skipping)
			{
				bits_write(&fbits, length|0x80, 8);
			}
			else
			{
				bits_write(&fbits, length, 8);
				for (; first < ci; ++first)
					bits_write(&fbits, ti_compress(*first, tile_bits), tile_bits);
			}
		}

		bits_flush(&fbits);

		frame_header_t fheader;
		fheader.size = fbits.buf.size;

		buffer_add(out, &fheader, sizeof(frame_header_t));
		buffer_add(out, fbits.buf.data, fbits.buf.size);

		memcpy(&last, curr, sizeof(frame_t));
	}

	bits_release(&fbits);
}

void frames_remap_tiles(frames_t* frames, const tile_index_t* remaps)
{
    for (size_t i = 0, n = buffer_count(&(frames->buffer)); i < n; ++i)
    {
        frame_t* frame = buffer_get(&(frames->buffer), i);
        for (size_t j = 0; j < FRAME_TILE_COUNT; ++j)
        {
            uint32_t in = frame->tiles[j];
            uint32_t remap = remaps[(in & ~TILE_BITS_MASK)];

            uint32_t out = (remap & ~TILE_BITS_MASK) | ((remap & TILE_BITS_MASK)^(in & TILE_BITS_MASK));
            frame->tiles[j] = out;
        }
    }
}