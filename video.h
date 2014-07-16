#pragma once

#define SCREEN_WIDTH (320)
#define SCREEN_HEIGHT (240)

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
#define TILE_FLIP_Y 0x40000000
#define TILE_BITS_SHIFT (16)
#else
typedef uint16_t tile_index_t;
#define TILE_NO_TILE 0xffff
#define TILE_FLIP_X 0x8000
#define TILE_FLIP_Y 0x4000
#define TILE_BITS_SHIFT (0)
#endif
#define TILE_BITS_MASK (TILE_FLIP_X|TILE_FLIP_Y)
#define MAX_TILE_INDEX ((TILE_NO_TILE & ~ (TILE_FLIP_X|TILE_FLIP_Y)) - 1)

typedef struct
{
	tile_index_t tiles[(SCREEN_WIDTH / TILE_WIDTH) * (SCREEN_HEIGHT / TILE_HEIGHT)];
} frame_t;

typedef struct
{
	uint32_t tiles[(SCREEN_WIDTH / TILE_WIDTH) * (SCREEN_HEIGHT / TILE_HEIGHT)];
} raw_frame_t;
