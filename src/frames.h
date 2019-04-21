#pragma once

#include <stdint.h>
#include <stdio.h>

#include "tiles.h"

#define FRAME_WIDTH (320)
#define FRAME_HEIGHT (256)

typedef struct frame_header_t
{
	uint16_t size;
} frame_header_t;

#define FRAME_TILE_COUNT ((FRAME_WIDTH / TILE_WIDTH) * (FRAME_HEIGHT / TILE_HEIGHT))
typedef struct frame_t
{
	tile_index_t tiles[FRAME_TILE_COUNT];
} frame_t;

typedef struct frames_t
{
	buffer_t buffer;
} frames_t;

void frames_init(frames_t* frames);
void frames_release(frames_t* frames);

void frames_add(frames_t* frames, const frame_t* frame);
void frames_remap_tiles(frames_t* frames, const tile_index_t* remaps);

size_t frames_load(const buffer_t* in, size_t offset, size_t count, frames_t* frames, size_t tile_bits);
void frames_save(buffer_t* out, const frames_t* frames, size_t tile_bits);
