#include "stream.h"

#include "frames.h"
#include "tiles.h"

#include <string.h>
#include <stdlib.h>

stream_t* stream_create()
{
	stream_t* stream = malloc(sizeof(stream_t));
	memset(stream, 0, sizeof(stream_t));

	return stream;
}

int stream_save(const stream_t* stream, FILE* out)
{
	stream_header_t header;
	header.tiles = stream->tiles->size;
	header.frames = stream->frames->size;
	if (fwrite(&header, sizeof(header), 1, out) < 1)
		return -1;

	const tiles_t* tiles = stream->tiles;
	for (size_t i = 0; i < tiles->size; ++i)
	{
		const tile_t* tile = &(tiles->tiles[i]);
		if (fwrite(&(tile->data), sizeof(raw_tile_t), 1, out) < 1)
			return -1;
	}

	const frames_t* frames = stream->frames;
	for (size_t i = 0; i < frames->size; ++i)
	{
		const frame_t* frame = &(frames->frames[i]);
		if (fwrite(frame, sizeof(frame_t), 1, out) < 1)
			return -1;
	}

	return 0;
}

void stream_destroy(stream_t* stream)
{
	if (stream->frames)
		free(stream->frames);
	free(stream);
}
