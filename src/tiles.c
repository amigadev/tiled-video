#include "tiles.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static uint32_t count_bits(uint8_t x)
{
	x = (x & 0x55) + ((x >> 1) & 0x55);
	x = (x & 0x33) + ((x >> 2) & 0x33);
	return (x & 0x0f) + ((x >> 4) & 0x0f);
}

// this hash allows us to search close matches
static uint32_t build_hash(const void* input, size_t length)
{
	uint32_t temp = 0;//2166136261UL;
	for (const uint8_t* begin = (const uint8_t*)input, *end = ((const uint8_t*)input) + length; begin != end; ++begin)
	{
		temp += count_bits(*begin);
	}

	return temp;
}

void build_tile(tile_t* tile, const uint8_t* pixels, uint8_t threshold, int32_t pitch)
{
	memset(tile, 0, sizeof(tile_t));

	for (size_t y = 0; y < TILE_HEIGHT; ++y)
	{
		for (size_t x = 0; x < TILE_WIDTH; x += 8)
		{
			uint8_t bits = 0;
			for (size_t i = 0; i < 8; ++i)
			{
				bits = (bits << 1) | ((pixels[x + i + (y * pitch)] > threshold) ? 1 : 0);
			}
			tile->data.bits[(x / 8) + y * (TILE_WIDTH / 8)] = bits;
		}
	}

	tile->remap = TILE_NO_TILE;
	tile->next = TILE_NO_TILE;
	tile->count = 1;
}

tiles_t* tiles_create(stream_t* stream)
{
	tiles_t* tiles = malloc(sizeof(tiles_t));
	memset(tiles, 0, sizeof(tiles_t));

	for (size_t i = 0; i < TILES_HASH_SIZE; ++i)
		tiles->hash[i] = TILE_NO_TILE;

	stream->tiles = tiles;
	return tiles;
}

#define sizeof_array(x) (sizeof(x) / sizeof(x[0])) 

raw_tile_t tile_flip_x(const raw_tile_t* in)
{
	raw_tile_t out;
	for (size_t y = 0; y < TILE_HEIGHT; ++y)
	{
		const uint8_t* iny = &(in->bits[y * (TILE_WIDTH / 8)]);
		uint8_t* outy = &(out.bits[(TILE_HEIGHT - (y + 1)) * (TILE_WIDTH / 8)]);

		for (size_t x = 0; x < TILE_WIDTH / 8; ++x)
		{
			uint8_t byte = iny[x];

			byte = ((byte & 0x55) << 1) | ((byte & 0xaa) >> 1);
			byte = ((byte & 0x33) << 1) | ((byte & 0xcc) >> 2);
			byte = ((byte & 0x0f) << 4) | ((byte & 0xf0) >> 4);

			outy[(TILE_WIDTH / 8) - (x + 1)] = byte;
		}
	}
	return out;
}

raw_tile_t tile_flip_y(const raw_tile_t* in)
{
	raw_tile_t out;
	for (size_t y = 0; y < TILE_HEIGHT; ++y)
	{
		const uint8_t* iny = &(in->bits[y * (TILE_WIDTH / 8)]);
		uint8_t* outy = &(out.bits[(TILE_HEIGHT - (y + 1)) * (TILE_WIDTH / 8)]);
		memcpy(outy, iny, TILE_WIDTH / 8);
	}
	return out;
}

raw_tile_t tile_invert(const raw_tile_t* in)
{
	raw_tile_t out;
	for (size_t i = 0; i < sizeof(out.bits); ++i)
		out.bits[i] = ~(in->bits[i]);
	return out;
}

tile_index_t tiles_match(tiles_t* tiles, const tile_t* tile)
{
	uint32_t permutations[] = {
		// blitter friendly permutations
		0,
		TILE_FLIP_Y,
		TILE_INVERT,
		TILE_INVERT|TILE_FLIP_Y,
		// permutations that require cpu
		TILE_FLIP_X,
		TILE_FLIP_X|TILE_FLIP_Y,
		TILE_INVERT|TILE_FLIP_X,
		TILE_INVERT|TILE_FLIP_X|TILE_FLIP_Y
	};


	for (size_t i = 0; i < sizeof_array(permutations); ++i)
	{
		uint32_t permutation = permutations[i];
		raw_tile_t temp = tile->data;

		if (permutation & TILE_FLIP_X)
			temp = tile_flip_x(&temp);
		if (permutation & TILE_FLIP_Y)
			temp = tile_flip_y(&temp);
		if (permutation & TILE_INVERT)
			temp = tile_invert(&temp);

		uint32_t hash = build_hash(&temp, sizeof(raw_tile_t)) % TILES_HASH_SIZE;
		uint32_t index = tiles->hash[hash];

		while (index != TILE_NO_TILE)
		{
			const tile_t* candidate = &tiles->tiles[index];
			if (!memcmp(&(candidate->data),&temp,sizeof(raw_tile_t)))
			{
				break;
			}	 
			index = candidate->next;
		}

		if (index != TILE_NO_TILE)
		{
			tile_t* match = &tiles->tiles[index];
			match->count++;

			return permutation | index;
		}
	}

	return TILE_NO_TILE;
}

tile_index_t tiles_insert(tiles_t* tiles, const tile_t* tile)
{
	tile_index_t index = tiles_match(tiles, tile);
	if (index != TILE_NO_TILE)
	{
		return index;
	}

	if (tiles->size == tiles->capacity)
	{
		size_t newCapacity = tiles->size > 0 ? tiles->capacity * 2 : 1024;
		tile_t* newTiles = malloc(sizeof(tile_t) * newCapacity);
		memcpy(newTiles, tiles->tiles, tiles->size * sizeof(tile_t));
		free(tiles->tiles);

		tiles->tiles = newTiles;
		tiles->capacity = newCapacity;
	}

	tile_t* newTile = &tiles->tiles[tiles->size];

	uint32_t hash = build_hash(&(tile->data), sizeof(tile->data)) % TILES_HASH_SIZE;
	memcpy(newTile, tile, sizeof(tile_t));
	newTile->next = tiles->hash[hash];
	tiles->hash[hash] = tiles->size;

	return (tiles->size++);
}

uint32_t tile_diff(const raw_tile_t* a, const raw_tile_t* b, uint32_t* used_permutation)
{
	// TODO: run these further out (to prioritize blitter-friendly modes)

	const uint32_t permutations[] = {
		// 
		0,
		TILE_FLIP_Y,
		TILE_INVERT|TILE_FLIP_Y,
		TILE_INVERT,
		TILE_FLIP_X,
		TILE_FLIP_X|TILE_FLIP_Y,
		TILE_INVERT|TILE_FLIP_X,
		TILE_INVERT|TILE_FLIP_X|TILE_FLIP_Y
	};

	uint32_t curr_error = (uint32_t)~0UL;
	uint32_t curr_permutation = 0L;

	for (int i = 0; i < sizeof_array(permutations); ++i)
	{
		uint32_t permutation = permutations[i];
		raw_tile_t temp = *b;

		if (permutation & TILE_FLIP_X)
			temp = tile_flip_x(&temp);
		if (permutation & TILE_FLIP_Y)
			temp = tile_flip_y(&temp);
		if (permutation & TILE_INVERT)
			temp = tile_invert(&temp);

		uint32_t error = 0;
		for (size_t i = 0; i < (TILE_WIDTH / 8) * TILE_HEIGHT; ++i)
		{
			error += count_bits(a->bits[i] ^ temp.bits[i]);
		}

		if (error < curr_error)
		{
			curr_error = error;
			curr_permutation = permutation;
		}
	}

	*used_permutation = curr_permutation;
	return curr_error;
}

// TODO: allow for permutations
uint32_t tiles_compare(tiles_t* tiles, uint32_t local, uint32_t search, uint32_t* actual_error, uint32_t* actual_permutation)
{
	const tile_t* pattern = &(tiles->tiles[local]);
	uint32_t candidate = TILE_NO_TILE;
	uint32_t error = (uint32_t)~0UL;
	uint32_t permutation = 0;

	while (search != TILE_NO_TILE)
	{
		const tile_t* tile = &(tiles->tiles[search]);
		if (local == search || tile->count == 0)
		{
			search = tile->next;
			continue;
		}

		uint32_t curr_permutation;
		uint32_t curr_error = tile_diff(&(pattern->data), &(tile->data), &curr_permutation);
		if (curr_error <= error)
		{
			candidate = search;
			error = curr_error;
			permutation = curr_permutation;
		}

		search = tile->next;
	}

	*actual_error = error;
	*actual_permutation = permutation;
	return candidate;
}
void tiles_destroy(tiles_t* tiles)
{
	if (tiles->tiles)
		free(tiles->tiles);

	free(tiles);
}
