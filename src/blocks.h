#pragma once

#include "buffer.h"

#define BLOCK_HASH_SIZE (256)

#define BLOCK_WIDTH (8)
#define BLOCK_HEIGHT (8)

typedef uint32_t block_index_t;
#define NO_BLOCK (0xffffffff)
#define BLOCK_FLIP_X (0x80000000)
#define BLOCK_FLIP_Y (0x40000000)
#define BLOCK_INVERT (0x20000000)
#define BLOCK_BITS_MASK (BLOCK_FLIP_X|BLOCK_FLIP_Y|BLOCK_INVERT)

typedef struct block_t
{
	uint8_t bits[(BLOCK_WIDTH / 8) * BLOCK_HEIGHT];
	uint32_t count; // number of users
	uint32_t next; // next in hash chain

	block_index_t remap; // block remap index
	uint32_t incoming; // count of incoming references (remove?)
} block_t;

typedef struct blocks_t
{
	buffer_t buffer;
	uint32_t hash[BLOCK_HASH_SIZE];
} blocks_t;

void blocks_init(blocks_t* blocks);
void blocks_release(blocks_t* blocks);

block_t build_block(const uint8_t* pixels, uint8_t threshold, int32_t pitch);
uint32_t hash_block(const block_t* block);

block_index_t blocks_insert(blocks_t* blocks, const block_t* block);
block_t blocks_get(const blocks_t* blocks, block_index_t index);
size_t block_match(const block_t* a, const block_t* b);

void block_render(uint8_t* pixels, const block_t* block, uint32_t pitch);

void blocks_find_matches(blocks_t* blocks, size_t max_error);
void blocks_reduce(blocks_t* blocks);
void blocks_rebuild(blocks_t* blocks, uint32_t* remap);

size_t blocks_load(const buffer_t* in, size_t offset, size_t count, blocks_t* blocks);
void blocks_save(buffer_t* out, const blocks_t* blocks);
