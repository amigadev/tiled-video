#pragma once

#include <stdint.h>
#include <stdio.h>

#include "tiles.h"
#include "stream.h"

#define FRAME_WIDTH (320)
#define FRAME_HEIGHT (256)

typedef struct frame_t
{
	tile_index_t tiles[(FRAME_WIDTH / TILE_WIDTH) * (FRAME_HEIGHT / TILE_HEIGHT)];
} frame_t;

typedef struct frames_t
{
	size_t size;
	size_t capacity;
	frame_t* frames;
} frames_t;

frames_t* frames_create(stream_t* stream);
frame_t* frames_add(frames_t* frames);
