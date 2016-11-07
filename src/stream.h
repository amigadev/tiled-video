#pragma once

#include <stdint.h>
#include <stdio.h>

struct frames_t;
struct tiles_t;

typedef struct stream_header_t
{
	uint32_t tiles;
	uint32_t frames;
} stream_header_t;

typedef struct stream_t
{
	struct frames_t* frames;
	struct tiles_t* tiles;
} stream_t;

stream_t* stream_create();
int stream_save(const stream_t* stream, FILE* fp);
int stream_load(stream_t* stream, FILE* fp);
void stream_destroy(stream_t* stream);
