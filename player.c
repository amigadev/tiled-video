#include <SDL2/SDL.h>
#include <unistd.h>

#include "video.h"

#define FRAME_RATE 30
#define TICK_RATE (1000/30)

int main(int argc, char* argv[])
{
	int ret = 1;

	SDL_Window* win = NULL;
	SDL_Renderer* rdr = NULL;
	SDL_Surface* work = NULL;

	do
	{
		if (isatty(fileno(stdin)))
		{
			fprintf(stderr, "No file specified as piped input\n");
			break;
		}

		if (SDL_Init(SDL_INIT_EVERYTHING))
		{
			fprintf(stderr, "Could not initialize SDL\n");
			break;
		}	

		if (!(win = SDL_CreateWindow("Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0))) {
			fprintf(stderr, "Could not create window\n");
			break;
		}		

		if (!(rdr = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC)))
		{
			fprintf(stderr, "Could not create renderer\n");
			break;
		}

		if (!(work = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000)))
		{
			fprintf(stderr, "Could not create RGB surface\n");
			break;
		}

		ret = 0;

		size_t read = 0;
		header_t header;
		read += fread(&header, 1, sizeof(header), stdin);

		uint32_t tile_count = ntohl(header.tiles);
		uint32_t frame_count = ntohl(header.frames);
		uint32_t stream_count = ntohl(header.stream);

		uint32_t tile_size = ((tile_count * sizeof(tile_t)) + 511) & ~511;
		tile_t* tiles = malloc(tile_size);
		read += fread(tiles, 1, tile_size, stdin);

		uint8_t* stream = malloc(stream_count);
		fread(stream, 1, stream_count, stdin);

		fprintf(stderr, "%u tiles, %u frames, %u stream bytes\n", tile_count, frame_count, stream_count);

		frame_t prev_frame = { 0 }, curr_frame = { 0 };

		SDL_Texture* textures[4] = { 0 };
		int alpha[4] = { 16, 32, 64, 255 };

		uint8_t* stream_begin = stream;
		uint8_t* stream_end = stream + stream_count;

		uint32_t next_frame_tick = SDL_GetTicks();

		int quit = 0;
		for (int frame_index = 0; !quit && (frame_index < frame_count); ++frame_index)
		{
			SDL_Event event;
			if (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
					case SDL_QUIT: quit = 1; break;
				}
			}

			uint32_t curr_frame_tick = SDL_GetTicks();
			if (curr_frame_tick < next_frame_tick)
			{
				SDL_Delay(next_frame_tick - curr_frame_tick);
				--frame_index;
				continue;
			}
			next_frame_tick += TICK_RATE;

			SDL_RenderClear(rdr);

			for (unsigned int i = 0, n = (SCREEN_WIDTH / TILE_WIDTH) * (SCREEN_HEIGHT / TILE_HEIGHT); i < n;)
			{
				if (i > n)
				{
					fprintf(stderr, "BUFFER OVERRUN\n");
					break;
				}

				uint8_t length = *stream_begin;
				uint8_t dlen = length & 0x7f;
				if (length & 0x80)
				{
					memcpy(&curr_frame.tiles[i], stream_begin + 1, dlen * sizeof(tile_index_t));
					stream_begin += (dlen * sizeof(tile_index_t));	
				}

				++ stream_begin;
				i += dlen;
			}

			SDL_LockSurface(work);
			{
				tile_index_t* curr_tile_index = curr_frame.tiles;

				for (unsigned int y = 0; y < work->h; y += TILE_HEIGHT)
				{
					for (unsigned int x = 0; x < work->w; x += TILE_WIDTH)
					{
						uint8_t* pixels = ((uint8_t*)work->pixels) + x * 4 + y * work->pitch;
						tile_index_t tindex = ntohs(*curr_tile_index++);
						tile_t* curr_tile = &tiles[tindex & ~TILE_BITS_MASK];

						switch (tindex & TILE_BITS_MASK)
						{
							case 0:
							{ 
								for (unsigned int ty = 0; ty < TILE_HEIGHT; ++ty)
								{
									uint8_t* pixel = pixels;
									uint8_t* row = &curr_tile->data[ty * (TILE_WIDTH / 8)];

									for (unsigned int tx = 0; tx < TILE_WIDTH; ++tx)
									{
										uint8_t result = row[tx / 8] & (1 << ((tx & 7) ^ 7)) ? 0xff : 0x00;
										pixel[0] = pixel[1] = pixel[2] = pixel[3] = result;
										pixel += 4;
									}

									pixels += work->pitch;
								}
							}
							break;

							case TILE_FLIP_X:
							{ 
								for (unsigned int ty = 0; ty < TILE_HEIGHT; ++ty)
								{
									uint8_t* pixel = pixels;
									uint8_t* row = &curr_tile->data[ty * (TILE_WIDTH / 8)];

									for (unsigned int tx = 0; tx < TILE_WIDTH; ++tx)
									{
										uint8_t result = row[((tx ^ 15) / 8)] & (1 << (tx & 7)) ? 0xff : 0x00; 
										pixel[0] = pixel[1] = pixel[2] = pixel[3] = result;
										pixel += 4;
									}

									pixels += work->pitch;
								}
							}
							break;

							case TILE_FLIP_Y:
							{
								for (unsigned int ty = 0; ty < TILE_HEIGHT; ++ty)
								{
									uint8_t* pixel = pixels;
									uint8_t* row = &curr_tile->data[((TILE_HEIGHT-1)-ty) * (TILE_WIDTH / 8)];

									for (unsigned int tx = 0; tx < TILE_WIDTH; ++tx)
									{
										uint8_t result = row[tx / 8] & (1 << ((tx & 7) ^ 7)) ? 0xff : 0x00;
										pixel[0] = pixel[1] = pixel[2] = pixel[3] = result;
										pixel += 4;
									}

									pixels += work->pitch;
								}
							}
							break;

							case TILE_FLIP_X|TILE_FLIP_Y:
							{
								for (unsigned int ty = 0; ty < TILE_HEIGHT; ++ty)
								{
									uint8_t* pixel = pixels;
									uint8_t* row = &curr_tile->data[((TILE_HEIGHT-1)-ty) * (TILE_WIDTH / 8)];

									for (unsigned int tx = 0; tx < TILE_WIDTH; ++tx)
									{
										uint8_t result = row[((tx ^ 15) / 8)] & (1 << (tx & 7)) ? 0xff : 0x00; 
										pixel[0] = pixel[1] = pixel[2] = pixel[3] = result;
										pixel += 4;
									}

									pixels += work->pitch;
								}
							}
							break;

						}

					}
				}

				prev_frame = curr_frame;
			}
			SDL_UnlockSurface(work);

			do
			{
				SDL_Texture* tex = NULL;
				if (!(tex = SDL_CreateTextureFromSurface(rdr, work)))
				{
					fprintf(stderr, "Could not create texture\n");
					break;
				}

				SDL_DestroyTexture(textures[0]);
				textures[0] = textures[1];
				textures[1] = textures[2];
				textures[2] = textures[3];
				textures[3] = tex;
			}
			while (0);

			for (unsigned int i = 0; i < 4; ++i)
			{
				SDL_Texture* texture = textures[i];
				if (!texture)
				{
					continue;
				}

				SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
				SDL_SetTextureAlphaMod(texture, alpha[i]);
				SDL_RenderCopy(rdr, texture, NULL, NULL);
			}

			SDL_RenderPresent(rdr);
			SDL_Delay(16);
		}
	}
	while (0);

	SDL_DestroyRenderer(rdr);
	SDL_DestroyWindow(win);

	SDL_Quit();

	return ret;
}

