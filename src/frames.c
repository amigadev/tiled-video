#include "frames.h"

#include <string.h>

frames_t* frames_create(stream_t* stream)
{
	frames_t* frames = malloc(sizeof(frames_t));
	memset(frames, 0, sizeof(frames_t));

	stream->frames = frames;
	return frames;
}

void frames_add(frames_t* frames, const frame_t* in)
{
	if (frames->size == frames->capacity)
	{
		size_t newCapacity = frames->capacity > 0 ? frames->capacity * 2 : 64;
		frame_t* newFrames = malloc(newCapacity * sizeof(frame_t));

		memcpy(newFrames, frames->frames, frames->size * sizeof(frame_t));
		memset(newFrames + frames->size, 0xff, (newCapacity-frames->size) * sizeof(frame_t));

		free(frames->frames);
		frames->frames = newFrames;
		frames->capacity = newCapacity;
	}

	memcpy(&(frames->frames[frames->size++]), in, sizeof(frame_t));
}
