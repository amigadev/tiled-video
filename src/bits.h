#pragma once

#include "buffer.h"

#include <stdlib.h>

typedef struct bits_t
{
	buffer_t buf;

	uint8_t data;
	size_t count;
    size_t offset;
} bits_t;

void bits_init_write(bits_t* bits);
void bits_init_read(bits_t* bits, const uint8_t* data, size_t length);
void bits_flush(bits_t* bits);
void bits_release(bits_t* bits);
void bits_reset(bits_t* bits);

void bits_write(bits_t* bits, uint32_t data, size_t count);
uint32_t bits_read(bits_t* bits, size_t count);
