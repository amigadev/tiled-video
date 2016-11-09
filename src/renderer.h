#pragma once

#include <stdint.h>

#define RENDER_VISIBLE (1)

int renderer_create(uint32_t width, uint32_t height, uint32_t flags);
int renderer_update(uint32_t width, uint32_t height, uint8_t* bytes, uint32_t sleepTime);
void renderer_destroy();
