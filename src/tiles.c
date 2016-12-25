#include "tiles.h"
#include "blocks.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//#define DEBUG_COLORS

#define sizeof_array(x) (sizeof(x) / sizeof(x[0]))

static uint32_t tile_variants[] = {
    0,
    TILE_FLIP_Y,
    TILE_INVERT,
    TILE_INVERT|TILE_FLIP_Y,
    TILE_FLIP_X,
    TILE_FLIP_X|TILE_FLIP_Y,
    TILE_INVERT|TILE_FLIP_X,
    TILE_INVERT|TILE_FLIP_X|TILE_FLIP_Y
};


uint32_t hash_tile(const tiles_t* tiles, const tile_t* tile)
{
	uint32_t temp = 0;
	for (size_t i = 0; i < (TILE_WIDTH / BLOCK_WIDTH) * (TILE_HEIGHT / BLOCK_HEIGHT); ++i)
	{
		block_index_t index = tile->indices[i];
		block_t block = blocks_get(&(tiles->blocks), index);
		temp += hash_block(&block);
	}
	return temp;
}

void tiles_init(tiles_t* tiles)
{
	blocks_init(&(tiles->blocks));
	buffer_init(&(tiles->buffer), sizeof(tile_t));

	for (size_t i = 0; i < TILES_HASH_SIZE; ++i)
		tiles->hash[i] = NO_TILE;
}

void tiles_release(tiles_t* tiles)
{
	blocks_release(&(tiles->blocks));
	buffer_release(&(tiles->buffer));
}

static tile_t tile_flip_x(const tile_t* in)
{
	tile_t out;

	for (size_t y = 0; y < (TILE_HEIGHT / BLOCK_HEIGHT); ++y)
	{
		for (size_t x = 0; x < (TILE_WIDTH / BLOCK_WIDTH); ++x)
		{
			block_index_t t = in->indices[x + y * (TILE_WIDTH / BLOCK_WIDTH)];
			out.indices[(TILE_WIDTH / BLOCK_WIDTH) - (x + 1) + y * (TILE_WIDTH / BLOCK_WIDTH)] = t ^ BLOCK_FLIP_X;
		}
	}

	return out;
}

static tile_t tile_flip_y(const tile_t* in)
{
	tile_t out;

	for (size_t y = 0; y < (TILE_HEIGHT / BLOCK_HEIGHT); ++y)
	{
		const block_index_t* iny = &(in->indices[y * (TILE_WIDTH / BLOCK_WIDTH)]);
		block_index_t* outy = &(out.indices[((TILE_HEIGHT / BLOCK_HEIGHT) - (y+1)) * (TILE_WIDTH / BLOCK_WIDTH)]);

		for (size_t x = 0; x < (TILE_WIDTH / BLOCK_WIDTH); ++x)
		{
			*(outy++) = (*(iny++)) ^ BLOCK_FLIP_Y;
		}
	}

	return out;
}

static tile_t tile_invert(const tile_t* in)
{
	tile_t out;

	for (size_t i = 0; i < (TILE_WIDTH / BLOCK_WIDTH) * (TILE_HEIGHT / BLOCK_HEIGHT); ++i)
	{
		out.indices[i] = in->indices[i] ^ BLOCK_INVERT;		
	}

	return out;
}

tile_t tiles_get(const tiles_t* tiles, tile_index_t ti)
{
    uint32_t index = ti & ~TILE_BITS_MASK;
    tile_t temp = *((tile_t*)buffer_get(&(tiles->buffer), index));

    if (ti & TILE_FLIP_X)
        temp = tile_flip_x(&temp);
    if (ti & TILE_FLIP_Y)
        temp = tile_flip_y(&temp);
    if (ti & TILE_INVERT)
        temp = tile_invert(&temp);

    return temp;
}

int tile_match(const tiles_t* tiles, const tile_t* a, const tile_t* b)
{
    int cmp = 0;
    for (size_t i = 0; !cmp && i < (TILE_WIDTH / BLOCK_WIDTH) * (TILE_HEIGHT / BLOCK_HEIGHT); ++i)
    {
        block_t ba = blocks_get(&(tiles->blocks), a->indices[i]);
        block_t bb = blocks_get(&(tiles->blocks), b->indices[i]);

        cmp = memcmp(&(ba.bits), &(bb.bits), sizeof(ba.bits));
    }

    return cmp;
}

tile_index_t tiles_match(tiles_t* tiles, const tile_t* tile)
{
	for (size_t i = 0; i < sizeof_array(tile_variants); ++i)
	{
		uint32_t variant = tile_variants[i];

		tile_t temp = *tile;

		if (variant & TILE_FLIP_X)
			temp = tile_flip_x(&temp);
		if (variant & TILE_FLIP_Y)
		    temp = tile_flip_y(&temp);
        if (variant & TILE_INVERT)
            temp = tile_invert(&temp);

		uint32_t hash = hash_tile(tiles, &temp) & (TILES_HASH_SIZE-1);
		uint32_t index = tiles->hash[hash];

		while (index != NO_TILE)
		{
			const tile_t* candidate = buffer_get(&(tiles->buffer), index);

            if (!tile_match(tiles, &temp, candidate))
				break;
			index = candidate->next;
		}

		if (index != NO_TILE)
		{
			tile_t* match = buffer_get(&(tiles->buffer), index);
			return variant | index;
		}
	}

	return NO_BLOCK;
}

tile_index_t tiles_insert(tiles_t* tiles, const uint8_t* pixels, uint8_t threshold, int32_t pitch)
{
	tile_t temp;

	block_index_t* indices = temp.indices;
	for (size_t y = 0; y < (TILE_HEIGHT / BLOCK_HEIGHT); ++y)
	{
		for (size_t x = 0; x < (TILE_WIDTH / BLOCK_WIDTH); ++x)
		{
			const uint8_t* start = pixels + x * BLOCK_WIDTH + (y * BLOCK_HEIGHT) * pitch;
			block_t block = build_block(start, threshold, pitch);

			*(indices++) = blocks_insert(&(tiles->blocks), &block);
		}
	}

	tile_index_t index = tiles_match(tiles, &temp);
	if (index != NO_TILE)
	{
        tile_t* out = buffer_get(&(tiles->buffer), index & ~TILE_BITS_MASK);
        out->count++;

		return index;
	}

	tile_t* out = buffer_alloc(&(tiles->buffer), 1);

	memcpy(out->indices, temp.indices, sizeof(out->indices));

	uint32_t offset = buffer_offset(&(tiles->buffer), out);
	uint32_t hash = hash_tile(tiles, &temp) & (TILES_HASH_SIZE-1);
	out->next = tiles->hash[hash];
	tiles->hash[hash] = offset;

	out->count = 1;
	out->remap = NO_TILE;

	return offset;
}

void tiles_remap_blocks(tiles_t* tiles, const block_index_t* remaps)
{
    for (size_t i = 0; i < TILES_HASH_SIZE; ++i)
    {
        tiles->hash[i] = NO_TILE;
    }

    for (size_t i = 0, n = buffer_count(&(tiles->buffer)); i < n; ++i)
    {
        tile_t* curr = buffer_get(&(tiles->buffer), i);
        for (size_t j = 0; j < TILE_INDEX_COUNT; ++j)
        {
            uint32_t in = curr->indices[j];
            uint32_t remap = remaps[(in & ~BLOCK_BITS_MASK)];

            uint32_t out = remap^(in & BLOCK_BITS_MASK);
            curr->indices[j] = out;
        }

        uint32_t hash = hash_tile(tiles, curr) & (TILES_HASH_SIZE-1);
        curr->next = tiles->hash[hash];
        tiles->hash[hash] = i;
    }
}

// TODO: match with variants
void tiles_dedupe(tiles_t* tiles)
{
    size_t removed = 0;

    for (size_t i = 0; i < TILES_HASH_SIZE; ++i)
    {
        tiles->hash[i] = NO_TILE;
    }

    for (size_t i = 0, n = buffer_count(&(tiles->buffer)); i < n; ++i)
    {
        tile_t* curr = buffer_get(&(tiles->buffer), i);

        uint32_t hash = hash_tile(tiles, curr) & (TILES_HASH_SIZE-1);

        tile_index_t index = NO_TILE;
        for (size_t j = 0; j < sizeof_array(tile_variants); ++j)
        {
            uint32_t flags = tile_variants[j];

            tile_t temp = *curr;
            if (flags & TILE_FLIP_X)
                temp = tile_flip_x(&temp);
            if (flags & TILE_FLIP_Y)
                temp = tile_flip_y(&temp);
            if (flags & TILE_INVERT)
                temp = tile_invert(&temp);

            index = tiles->hash[hash];
            while (index != NO_TILE)
            {
                const tile_t* candidate = buffer_get(&(tiles->buffer), index);

                if (index == j)
                {
                    index = candidate->next;
                    continue;
                }

                if (!tile_match(tiles, &temp, candidate))
                    break;

                index = candidate->next;
            }

            if (index != NO_TILE)
            {
                index = index|flags;
                break;
            }
        }

        if (index != NO_TILE)
        {
            curr->remap = index;
            curr->count = 0;
            curr->next = NO_TILE;
            removed++;
        }
        else
        {
            curr->remap = NO_TILE;
            curr->next = tiles->hash[hash];
            tiles->hash[hash] = i;
        }

        fprintf(stderr, "\rdeduping tiles: %lu/%lu (%lu removed)", i+1, n, removed);
    }

    fprintf(stderr, "\n");
}

void tiles_rebuild(tiles_t* tiles, tile_index_t* remaps)
{
    buffer_t old = tiles->buffer;
    buffer_init(&(tiles->buffer), old.elemsize);

    for (size_t i = 0; i < TILES_HASH_SIZE; ++i)
    {
        tiles->hash[i] = NO_TILE;
    }

    for (size_t i = 0, n = buffer_count(&old); i < n; ++i)
    {
        tile_t* old_tile = (tile_t*)buffer_get(&old, i);
        if (old_tile->count == 0)
        {
            remaps[i] = NO_TILE;
            continue;
        }

        tile_t* new_tile = (tile_t*)buffer_alloc(&(tiles->buffer), 1);

        uint32_t hash = hash_tile(tiles, old_tile) & (TILES_HASH_SIZE-1);
        uint32_t offset = buffer_offset(&(tiles->buffer), new_tile);

        memcpy(&(new_tile->indices), &(old_tile->indices), sizeof(new_tile->indices));
        new_tile->count = old_tile->count;

        new_tile->next = tiles->hash[hash];
        tiles->hash[hash] = offset;

        new_tile->remap = NO_TILE;
        old_tile->remap = offset;

        remaps[i] = offset;
    }

    for (size_t i = 0, n = buffer_count(&old); i < n; ++i)
    {
        const tile_t* old_tile = (const tile_t*)buffer_get(&old, i);
        if (old_tile->count > 0)
            continue;

        uint32_t index = old_tile->remap & ~TILE_BITS_MASK;
        uint32_t flags = old_tile->remap & TILE_BITS_MASK;

        tile_t* target_tile = (tile_t*)buffer_get(&old, index);
        remaps[i] = target_tile->remap | flags;
    }

    buffer_release(&old);
}

void tile_render(uint8_t* target, const tiles_t* tiles, const tile_t* tile, uint32_t bits, uint32_t pitch)
{
    const block_index_t* indices = tile->indices;

    for (size_t j = 0; j < TILE_HEIGHT; j += BLOCK_HEIGHT)
    {
        for (size_t i = 0; i < TILE_WIDTH; i += BLOCK_HEIGHT)
        {
            block_index_t index = *(indices++);
            block_t block = blocks_get(&(tiles->blocks), index);
            uint8_t* pixels = &target[i + j * pitch];

            block_render(pixels, &block, pitch);
        }
    }
}

static uint32_t bi_compress(block_index_t index, size_t bits)
{
	uint32_t flags = (index & BLOCK_BITS_MASK) >> (32 - bits);

	uint32_t in = index & ~BLOCK_BITS_MASK;
	uint32_t out = index & (~0U >> (32-(bits-3)));
    uint32_t result = out | flags;
	return result;
}

static block_index_t bi_uncompress(uint32_t in, size_t bits)
{
    uint32_t flags = (in & (BLOCK_BITS_MASK >> (32 - bits))) << (32 - bits);
    uint32_t out = in & (~BLOCK_BITS_MASK) >> (32 - bits);
    uint32_t result = out | flags;
    return result;
}

#define sizeof_member(type, member) sizeof(((type *)0)->member)
static const size_t TILE_DATA_SIZE = sizeof_member(tile_t, indices);

size_t tiles_load(const buffer_t* in, size_t offset, size_t count, tiles_t* tiles, size_t block_bits)
{
    for (size_t i = 0; i < TILES_HASH_SIZE; ++i)
    {
        tiles->hash[i] = NO_TILE;
    }

    for (size_t i = 0, n = count; i < n; ++i)
    {
        tile_t* tile = buffer_alloc(&(tiles->buffer), 1);

        for (size_t j = 0; j < TILE_INDEX_COUNT; ++j)
        {
            if (block_bits > 16)
            {
                uint32_t temp;
                const uint8_t* data = buffer_get(in, offset);

                memcpy(&temp, data, sizeof(temp));
                tile->indices[j] = temp;

                offset += sizeof(temp);
            }
            else
            {
                uint16_t temp;
                const uint8_t* data = buffer_get(in, offset);

                memcpy(&temp, data, sizeof(temp));
                tile->indices[j] = bi_uncompress(temp, 16);

                offset += sizeof(temp);
            }
        }

        uint32_t hash = hash_tile(tiles, tile) & (TILES_HASH_SIZE-1);

        tile->next = tiles->hash[hash];
        tiles->hash[hash] = i;

        tile->count = 0;
        tile->remap = NO_TILE;
    }

    return offset;
}

void tiles_save(buffer_t* out, const tiles_t* tiles, size_t block_bits)
{
	for (size_t i = 0, n = buffer_count(&(tiles->buffer)); i < n; ++i)
	{
		const tile_t* tile = buffer_get(&(tiles->buffer), i);
        for (size_t j = 0; j < TILE_INDEX_COUNT; ++j)
        {
            block_index_t index = tile->indices[j];

            if (block_bits > 16)
            {
                uint32_t* temp = ((uint32_t*)buffer_alloc(out, sizeof(uint32_t)));
                *temp = index;
            }
            else
            {
                uint16_t* temp = ((uint16_t*)buffer_alloc(out, sizeof(uint16_t)));
                *temp = bi_compress(index, 16);
            }
        }
	}
}
