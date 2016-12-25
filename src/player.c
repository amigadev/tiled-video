#include "renderer.h"
#include "stream.h"
#include "frames.h"

#include <string.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	uint8_t buffer[FRAME_WIDTH * FRAME_HEIGHT];

	if (renderer_create(FRAME_WIDTH, FRAME_HEIGHT, RENDER_VISIBLE) < 0)
		return -1;

	stream_t* stream = stream_create();

	FILE* in = fopen("anim.bin", "rb");
	if (!in)
		return -1;

	if (stream_load(stream, in) < 0)
	{
		fprintf(stderr, "Could not read stream\n");
		return -1;
	}

	const tiles_t* tiles = &(stream->tiles);
	const frames_t* frames = &(stream->frames);

	for (size_t index = 0, n = buffer_count(&(frames->buffer)); index < n; ++index)
	{
		const frame_t* frame = (const frame_t*)buffer_get(&(frames->buffer), index);

		size_t count = 0;
        const tile_index_t* indices = frame->tiles;
		for (size_t y = 0; y < FRAME_HEIGHT; y += TILE_HEIGHT)
		{
			for (size_t x = 0; x < FRAME_WIDTH; x += TILE_WIDTH)
			{
                tile_index_t ti = *(indices++);
                const tile_t tile = tiles_get(tiles, ti);
				uint8_t* target = &buffer[x + y * FRAME_WIDTH];

				tile_render(target, tiles, &tile, ti & TILE_BITS_MASK, FRAME_WIDTH);
				++count;
			}
		}

		fprintf(stderr, "\rframe: %lu tiles: %lu    ", index, count);

		if (renderer_update(FRAME_WIDTH, FRAME_HEIGHT, buffer, 32) < 0)
			return -1;
	}

	fprintf(stderr, "\n");

	renderer_destroy();
}
