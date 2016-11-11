#include <SDL2/SDL.h>

#include "renderer.h"

SDL_Window* window = NULL;
SDL_Surface* buffer = NULL;

#define DEBUG_COLORS

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

#if defined(DEBUG_COLORS)
/*
1000 8 248 = 111 x y invert 
1001 9 249 = 110 x y
1010 a 250 = 101 x invert
1011 b 251 = 100 x
1100 c 252 = 011 y invert
1101 d 253 = 010 y
1110 e 254 = 001 invert
1111 f 255 = 000
*/
		palette[248].r = 0x7f; palette[248].g = 0x7f; palette[248].g = 0x7f;
		palette[249].r = 0x7f; palette[249].g = 0x7f; palette[249].g = 0xff;
		palette[250].r = 0x7f; palette[250].g = 0xff; palette[250].g = 0x7f;
		palette[251].r = 0x7f; palette[251].g = 0xff; palette[251].g = 0xff;
		palette[252].r = 0xff; palette[252].g = 0x7f; palette[252].g = 0x7f;
		palette[253].r = 0xff; palette[253].g = 0x7f; palette[253].g = 0xff;
		palette[254].r = 0xff; palette[254].g = 0xff; palette[254].g = 0x7f;
		palette[255].r = 0xff; palette[255].g = 0xff; palette[255].g = 0xff;
#endif

		SDL_SetPaletteColors(buffer->format->palette, palette, 0, 256);
	}

	return 0;
}

int renderer_update(uint32_t width, uint32_t height, uint8_t* bytes, uint32_t sleepTime)
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

	if (sleepTime > 0)
		SDL_Delay(sleepTime);

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
