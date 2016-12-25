#pragma once

#include <stdint.h>
#include <stdio.h>

#include "frames.h"
#include "tiles.h"

typedef struct stream_header_t
{
	uint32_t blocks;
	uint32_t tiles;
	uint32_t frames;
	uint32_t size;
	uint32_t compressed_size;

	uint16_t tile_bits;
	uint16_t block_bits;
} stream_header_t;

typedef struct stream_t
{
	frames_t frames;
	tiles_t tiles;
} stream_t;

#define STREAM_BLOCK_COMPRESSED (1 << 15)
#define STREAM_BLOCK_MAX_SIZE (32768)
typedef struct stream_block_t
{
    uint16_t inlen; // uncompressed
    uint16_t outlen; // potentially compressed
} stream_block_t;

stream_t* stream_create();
void stream_destroy(stream_t* stream);

int stream_save(const stream_t* stream, FILE* fp);
int stream_load(stream_t* stream, FILE* fp);

void stream_optimize_blocks(stream_t* stream, size_t passes, size_t max_error);
void stream_optimize_tiles(stream_t* stream, size_t max_error);
void stream_optimize_frames(stream_t* stream);
