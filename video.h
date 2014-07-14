#pragma once

#define SCREEN_WIDTH (320)
#define SCREEN_HEIGHT (192)

#define TILE_WIDTH 16
#define TILE_HEIGHT 16

typedef struct
{
	uint8_t data[(TILE_WIDTH / 8) * TILE_HEIGHT];
} tile_t;

//#define LONG_TILE_INDEX
#if defined(LONG_TILE_INDEX)
typedef uint32_t tile_index_t;
#define TILE_NO_TILE 0xffffffff
#define TILE_FLIP_X 0x80000000
#else
typedef uint16_t tile_index_t;
#define TILE_NO_TILE 0xffff
#define TILE_FLIP_X 0x8000
#endif

typedef struct
{
	tile_index_t tiles[(SCREEN_WIDTH / TILE_WIDTH) * (SCREEN_HEIGHT / TILE_HEIGHT)];
} frame_t;
