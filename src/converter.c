#include "renderer.h"
#include "stream.h"
#include "frames.h"

#include <string.h>
#include <stdio.h>

//#define FIRST_INDEX (1)
//#define LAST_INDEX (5478)

#define FIRST_INDEX 1200
#define LAST_INDEX 1200

#define MAX_ERROR (32)

int main(int argc, char* argv[])
{
	uint8_t input[FRAME_WIDTH * FRAME_HEIGHT];

	if (renderer_create(FRAME_WIDTH, FRAME_HEIGHT, RENDER_VISIBLE) < 0)
		return -1;

	stream_t* stream = stream_create();
	tiles_t* tiles = tiles_create(stream);
	frames_t* frames = frames_create(stream);
	size_t actual = 0;

	for (int index = FIRST_INDEX; index <= LAST_INDEX; ++index)
	{
		char path[256];
		sprintf(path, "images/image-%04d.raw", index);

		FILE* fp = fopen(path, "rb");
		if (!fp)
			break;

		if (fread(input, FRAME_WIDTH*FRAME_HEIGHT, 1, fp) < 1)
			break;

		fclose(fp);

		frame_t frame;
		tile_index_t* indices = frame.tiles;

		for (int y = 0; y < FRAME_HEIGHT; y += TILE_HEIGHT)
		{
			for (int x = 0; x < FRAME_WIDTH; x += TILE_WIDTH)
			{
				tile_t tile;

				build_tile(&tile, input + x + y * FRAME_HEIGHT, 200, FRAME_WIDTH);  

				tile_index_t index = tiles_insert(tiles, &tile);
				*(indices++) = index;

				++ actual;
			}
		}

		frames_add(frames, &frame);

		if (renderer_update(FRAME_WIDTH, FRAME_HEIGHT, input, 400) < 0)
			return 0;

		fprintf(stderr, "\rpath: %s, frames: %lu tiles: %lu/%lu (%.2f%%)", path, frames->size, tiles->size, actual, tiles->size*100.0f / actual);
	}

	fprintf(stderr, "\nsaving...");

	FILE* out = fopen("anim.bin", "wb");
	if (stream_save(stream, out) < 0)
	{
		fprintf(stderr, "failed to save stream\n");
		return -1;
	}
	fclose(out);

	fprintf(stderr, "\n");

	stream_destroy(stream);
	renderer_destroy();

	return 0;
}
