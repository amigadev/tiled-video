#include <SDL2/SDL.h>

#include "renderer.h"

SDL_Window* window = NULL;
SDL_Surface* buffer = NULL;

int renderer_create(uint32_t width, uint32_t height, uint32_t flags)
{
	if (SDL_Init((flags == RENDER_VISIBLE) ? SDL_INIT_EVERYTHING : SDL_INIT_EVENTS) < 0)
	{
		fprintf(stderr, "Could not initialize SDL\n");
		return -1;
	}

	if (flags == RENDER_VISIBLE)
	{
		if (!(window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, 0)))
		{
			fprintf(stderr, "Could not create window\n");
			return -1;
		}

		if (!(buffer = SDL_CreateRGBSurface(0,width,height,8,0,0,0,0)))
		{
			fprintf(stderr, "Could not create texture\n");
			return -1;
		}

		SDL_Color palette[256];
		for (int i = 0; i < 256; ++i)
		{
			palette[i].a = 255;
			palette[i].r = palette[i].g = palette[i].b = i;
		}

		SDL_SetPaletteColors(buffer->format->palette, palette, 0, 256);
	}

	return 0;
}

int renderer_update(uint32_t width, uint32_t height, uint8_t* bytes)
{
	SDL_Event event;
	if (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT: return -1;
		}
	}

	if (!window)
		return 0;

	if (SDL_LockSurface(buffer) < 0)
		return -1;

	memcpy(buffer->pixels, bytes, width * height);

	SDL_UnlockSurface(buffer);

	SDL_Surface* screen = SDL_GetWindowSurface(window);
	SDL_BlitSurface(buffer, NULL, screen, NULL);
	SDL_UpdateWindowSurface(window);

	return 0;
}

void renderer_destroy()
{
	if (buffer)
	{
		SDL_FreeSurface(buffer);
		buffer = NULL;
	}

	if (window)
	{
		SDL_DestroyWindow(window);
		window = NULL;
	}

	SDL_Quit();
}
