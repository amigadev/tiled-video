#include <SDL2/SDL.h>
#include <unistd.h>

#include "video.h"

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

		uint32_t tile_count = 0;
		fread(&tile_count, sizeof(tile_count), 1, stdin);

		tile_t* tiles = malloc(tile_count * sizeof(tile_t));
		fread(tiles, sizeof(tile_t), tile_count, stdin);

		uint32_t frame_count = 0;
		fread(&frame_count, sizeof(frame_count), 1, stdin);

		frame_t* frames = malloc(frame_count * sizeof(frame_t));
		fread(frames, sizeof(frame_t), frame_count, stdin);

		fprintf(stderr, "%u tiles, %u frames\n", tile_count, frame_count);

		frame_t prev_frame = { 0xff };

		SDL_Texture* textures[4] = { 0 };
		int alpha[4] = { 32, 64, 128, 255 };

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

			SDL_RenderClear(rdr);

			SDL_LockSurface(work);
			{
				frame_t* curr_frame = &frames[frame_index];
				tile_index_t* curr_tile_index = curr_frame->tiles;

				for (unsigned int y = 0; y < work->h; y += TILE_HEIGHT)
				{
					for (unsigned int x = 0; x < work->w; x += TILE_WIDTH)
					{
						uint8_t* pixels = ((uint8_t*)work->pixels) + x * 4 + y * work->pitch;
						tile_index_t tindex = *curr_tile_index++;
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

				prev_frame = *curr_frame;
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

