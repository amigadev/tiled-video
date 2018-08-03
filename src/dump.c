#include "renderer.h"
#include "stream.h"
#include "frames.h"

#include <string.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
	FILE* in = fopen("anim.bin", "rb");
	if (!in)
		return -1;

    if (stream_dump(in, "anim") < 0) {
		fprintf(stderr, "Could not dump stream\n");
		return -1;
    }

    fclose(in);
}
