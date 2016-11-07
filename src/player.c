#include "renderer.h"

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
}
