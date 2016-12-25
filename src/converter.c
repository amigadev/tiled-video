#include "renderer.h"
#include "stream.h"
#include "frames.h"
#include "bits.h"

#include <string.h>
#include <stdio.h>

#define FIRST_INDEX (1)
//#define LAST_INDEX (250)
#ifndef LAST_INDEX
#define LAST_INDEX (5478)
#endif

#define MAX_BLOCK_ERROR (8)
#define BLOCK_PASSES (10)
#define MAX_TILE_ERROR (32)

int main(int argc, char* argv[])
{
	uint8_t input[FRAME_WIDTH * FRAME_HEIGHT];

	if (renderer_create(FRAME_WIDTH, FRAME_HEIGHT, RENDER_VISIBLE) < 0)
		return -1;

	stream_t* stream = stream_create();
	frames_t* frames = &(stream->frames);
	tiles_t* tiles = &(stream->tiles);
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

				*(indices++) = tiles_insert(tiles, input + x + y * FRAME_WIDTH, 200, FRAME_WIDTH);
				++ actual;
			}
		}

		frames_add(frames, &frame);

        if (index % 10 == 0)
        {
            if (renderer_update(FRAME_WIDTH, FRAME_HEIGHT, input, 0) < 0)
                return 0;
        }

		fprintf(stderr, "\rpath: %s, frames: %lu tiles: %lu/%lu blocks: %lu", path, buffer_count(&(frames->buffer)), buffer_count(&(tiles->buffer)), actual, buffer_count(&(tiles->blocks.buffer)));
	}

	renderer_destroy();

    fprintf(stderr, "\n");

    stream_optimize_blocks(stream, BLOCK_PASSES, MAX_BLOCK_ERROR);
    stream_optimize_tiles(stream, MAX_TILE_ERROR);
//    stream_optimize_frames(stream);

	fprintf(stderr, "\nsaving...\n");

	FILE* out = fopen("anim.bin", "wb");
	if (stream_save(stream, out) < 0)
	{
		fprintf(stderr, "failed to save stream\n");
		return -1;
	}
	fclose(out);

	stream_destroy(stream);

	return 0;
}
