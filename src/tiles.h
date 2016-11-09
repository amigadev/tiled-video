#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "stream.h"

#define TILES_HASH_SIZE (256)

#define TILE_WIDTH (16)
#define TILE_HEIGHT (16)

// TODO: 16-bit support

typedef uint32_t tile_index_t;
#define TILE_NO_TILE 0xffffffff
#define TILE_FLIP_X 0x80000000
#define TILE_FLIP_Y 0x40000000
#define TILE_INVERT 0x20000000

#define TILE_BITS_MASK (TILE_FLIP_X|TILE_FLIP_Y|TILE_INVERT)
#define MAX_TILE_INDEX ((TILE_NO_TILE & ~TILE_BITS_MASK) - 1)

typedef struct raw_tile_t {
	uint8_t bits[(TILE_WIDTH / 8) * TILE_HEIGHT];
} raw_tile_t;

typedef struct tile_t
{
	raw_tile_t data;
	tile_index_t remap; // index (and mutation) to remap tile to
	uint32_t next; // index to next tile in hash
	uint32_t count; // number of duplicates referencing this tile
} tile_t;

typedef struct tiles_t
{
	size_t size;
	size_t capacity;
	tile_t* tiles;

	uint32_t hash[TILES_HASH_SIZE]; // hash table for collisions
} tiles_t;

void build_tile(tile_t* tile, const uint8_t* pixels, uint8_t threshold, int32_t pitch);
void tile_render(uint8_t* target, const raw_tile_t* tile, uint32_t flags, int32_t pitch);

tiles_t* tiles_create(stream_t* stream);
tile_index_t tiles_insert(tiles_t* tiles, const tile_t* tile);
