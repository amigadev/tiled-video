#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "blocks.h"

#define TILES_HASH_SIZE (256)

#define TILE_WIDTH (16)
#define TILE_HEIGHT (16)

// TODO: 16-bit support

typedef uint32_t tile_index_t;
#define NO_TILE 0xffffffff
#define TILE_FLIP_X 0x80000000
#define TILE_FLIP_Y 0x40000000
#define TILE_INVERT 0x20000000

#define TILE_BITS_MASK (TILE_FLIP_X|TILE_FLIP_Y|TILE_INVERT)
#define MAX_TILE_INDEX ((TILE_NO_TILE & ~TILE_BITS_MASK) - 1)

#define TILE_INDEX_COUNT ((TILE_WIDTH / BLOCK_WIDTH) * (TILE_HEIGHT / BLOCK_HEIGHT))
typedef struct tile_t
{
	block_index_t indices[TILE_INDEX_COUNT];

	uint32_t next; // index to next tile in hash
	uint32_t count; // number of duplicates referencing this tile
	tile_index_t remap; // tile remap index
} tile_t;

typedef struct tiles_t
{
	blocks_t blocks;
	buffer_t buffer;
	uint32_t hash[TILES_HASH_SIZE]; // hash table for collisions
} tiles_t;

void tiles_init(tiles_t* tiles);
void tiles_release(tiles_t* tiles);

tile_t tiles_get(const tiles_t* tiles, tile_index_t ti);

tile_index_t tiles_insert(tiles_t* tiles, const uint8_t* pixels, uint8_t threshold, int32_t pitch);
void tiles_remap_blocks(tiles_t* tiles, const block_index_t* remaps);
void tiles_dedupe(tiles_t* tiles);
void tiles_reduce(tiles_t* tiles);
void tiles_rebuild(tiles_t* tiles, uint32_t* remaps);

void tile_render(uint8_t* target, const tiles_t* tiles, const tile_t* tile, uint32_t bits, uint32_t pitch);

size_t tiles_load(const buffer_t* in, size_t offset, size_t count, tiles_t* tiles, size_t block_bits);
void tiles_save(buffer_t* out, const tiles_t* tiles, size_t block_bits);
