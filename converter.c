#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "video.h"

SDL_Surface* load_image(const char* filename)
{
	SDL_Surface* result = NULL;
	SDL_Surface* temp = NULL;

	do
	{
		if (!(temp = IMG_Load(filename)))
		{
			fprintf(stderr, "Failed loading \"%s\"\n", filename);
			break;
		}

		if (!(result = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000)))
		{
			fprintf(stderr, "Could not create RGB surface\n");
			break;
		}

		SDL_Rect rect = { 0, 0, 640, 400 };

		SDL_BlitScaled(temp, &rect, result, NULL);
	}
	while (0);

	SDL_FreeSurface(temp);

	return result;
}

tile_t* tiles = NULL;
uint32_t tile_offset = 0, tile_reserve = 0;

frame_t* frames = NULL;
uint32_t frame_offset = 0, frame_reserve = 0;

uint8_t* stream = NULL;
uint32_t stream_offset = 0, stream_reserve = 0;

tile_t* tile_alloc()
{
	if (tile_offset == tile_reserve)
	{
		uint32_t new_reserve = tile_reserve > 0 ? tile_reserve * 2 : 16;
		tile_t* new_tiles = malloc(new_reserve * sizeof(tile_t));
		memcpy(new_tiles, tiles, tile_offset * sizeof(tile_t));

		free(tiles);
		tiles = new_tiles;
		tile_reserve = new_reserve;
	}

	return &tiles[tile_offset++];
}

int tile_compare(tile_t* a, tile_t* b, int flip)
{
	uint32_t* ta = (uint32_t*)a->data;
	uint32_t* tb = (uint32_t*)b->data;

	unsigned int value = 0;
	for (unsigned int i = 0; i < ((TILE_WIDTH / 8) * TILE_HEIGHT) / 4; ++i)
	{
		uint32_t da = *ta++;
		uint32_t db = *tb++;

		if (flip)
		{
			db = ((db & 0x55555555) << 1) | ((db & 0xaaaaaaaa) >> 1);
			db = ((db & 0x33333333) << 2) | ((db & 0xcccccccc) >> 2);
			db = ((db & 0x0f0f0f0f) << 4) | ((db & 0xf0f0f0f0) >> 4);
			db = ((db & 0x00ff00ff) << 8) | ((db & 0xff00ff00) >> 8);
		}

		uint32_t x = da ^ db;
		x = x - ((x >> 1) & 0x55555555);
		x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
		value += ((x + (x >> 4) & 0x0f0f0f0f) * 0x01010101) >> 24;
	}

	return value * value;
}

frame_t* frame_alloc()
{
	if (frame_offset == frame_reserve)
	{
		uint32_t new_reserve = frame_reserve > 0 ? frame_reserve * 2 : 16;
		frame_t* new_frames = malloc(new_reserve * sizeof(frame_t));
		memcpy(new_frames, frames, frame_offset * sizeof(frame_t));

		free(frames);
		frames = new_frames;
		frame_reserve = new_reserve;
	}

	return &frames[frame_offset++];
}

uint8_t* stream_alloc(uint32_t len)
{
	if (stream_offset + len > stream_reserve)
	{
		uint32_t new_reserve = stream_reserve * 2 < stream_reserve + len ? stream_reserve + len : stream_reserve * 2;
		uint8_t* new_stream = malloc(new_reserve);
		memcpy(new_stream, stream, stream_offset);

		free(stream);
		stream = new_stream;
		stream_reserve = new_reserve;
	}

	uint8_t* result = &stream[stream_offset];
	stream_offset += len;
	return result;
}


int main(int argc, char* argv[])
{
	int ret = 1;

	SDL_Window* win = NULL;
	SDL_Renderer* rdr = NULL;

	do
	{
		if (SDL_Init(SDL_INIT_EVERYTHING))
		{
			fprintf(stderr, "Could not initialize SDL\n");
			break;
		}	

		if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG)
		{
			fprintf(stderr, "Could not initialize image: %s\n", IMG_GetError());
			break;
		}

		if (!(win = SDL_CreateWindow("Converter", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0)))
		{
			fprintf(stderr, "Could not create window\n");
			break;
		}		

		if (!(rdr = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED)))
		{
			fprintf(stderr, "Could not create renderer\n");
			break;
		}

		ret = 0;

		const int first_frame = 1;
		const int last_frame = 5600;

		frame_t prev_frame = { 0 };

		unsigned int tile_collisions = 0;

		tile_t* black_tile = tile_alloc();
		memset(black_tile, 0, sizeof(tile_t));

		tile_t* white_tile = tile_alloc();
		memset(white_tile, 0xff, sizeof(tile_t));

		int quit = 0;
		for (int frame_index = first_frame; !quit && (frame_index <= last_frame); ++frame_index)
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

			char path[256];
			sprintf(path, "./images/image_%05d.png", frame_index);

			frame_t curr_frame = prev_frame;

			SDL_Surface* image = NULL;
			if ((image = load_image(path)) != NULL)
			{
				SDL_Texture* tex = NULL;

				SDL_LockSurface(image);
				{
					const uint8_t thres = 200;

				 	for (unsigned int y = 0; y < image->h; ++y)
					{
						for (unsigned int x = 0; x < image->w; ++x)
						{
							uint8_t* pixel = ((uint8_t*)image->pixels) + x * 4 + y * image->pitch;
							pixel[0] = pixel[1] = pixel[2] = pixel[0] >= thres ? 255 : 0;
						}
					}

					tile_index_t frametile_offset = tile_offset;
					for (unsigned int y = 0; y < image->h; y += TILE_HEIGHT)
					{
						for (unsigned int x = 0; x < image->w; x += TILE_WIDTH)
						{
							tile_t tile = { 0 };

							for (unsigned int ty = 0; ty < TILE_HEIGHT; ++ty)
							{
								for (unsigned int tx = 0; tx < TILE_WIDTH; ++tx)
								{
									uint8_t* pixel = ((uint8_t*)image->pixels) + (x + tx) * 4 + (y + ty) * image->pitch;
									tile.data[(tx / 8) + (TILE_WIDTH / 8) * ty] |= (pixel[0] & 0x80) >> (tx & 7); 
								}
							}

							tile_index_t tile_index = TILE_NO_TILE;

							const unsigned int thres = 32;
							unsigned int max_diff = thres * thres;


							for (uint32_t i = 0; i < tile_offset; ++i)
							{
								unsigned int diff = tile_compare(&tile, &tiles[i], 0);
								if (diff < max_diff)
								{
									max_diff = diff;
									tile_index = i;
								}

								unsigned int diff2 = tile_compare(&tile, &tiles[i], 1);
								if (diff2 < max_diff)
								{
									max_diff = diff;
									tile_index = i | TILE_FLIP_X;
								}
							}

							if (tile_index == TILE_NO_TILE)
							{
								tile_t* new_tile = tile_alloc();
								(*new_tile) = tile;

								tile_index = new_tile - tiles;
							}
							else
							{
								++ tile_collisions;
							}

							curr_frame.tiles[(x / TILE_WIDTH) + (y / TILE_HEIGHT) * (SCREEN_WIDTH / TILE_WIDTH)] = tile_index;
						}
					}

					for (unsigned int y = 0; y < image->h; ++y)
					{
						for (unsigned int x = 0; x < image->w; ++x)
						{
							uint8_t* pixel = ((uint8_t*)image->pixels) + x * 4 + y * image->pitch;
							unsigned int tile_address = (x / TILE_WIDTH) + (y / TILE_HEIGHT) * (SCREEN_WIDTH / TILE_WIDTH);
							tile_index_t curr_tile = curr_frame.tiles[tile_address] & ~TILE_FLIP_X, prev_tile = prev_frame.tiles[tile_address] & ~TILE_FLIP_X;

							pixel[1] = curr_tile >= frametile_offset ? pixel[0] : 0x00;
							pixel[2] = curr_tile != prev_tile ? pixel[0] : 0x00;
						}
					}
				}
				SDL_UnlockSurface(image);

				do
				{
					if (!(tex = SDL_CreateTextureFromSurface(rdr, image)))
					{
						fprintf(stderr, "Could not create texture\n");
						break;
					}

					SDL_RenderCopy(rdr, tex, NULL, NULL);
				}
				while (0);

				SDL_DestroyTexture(tex);
				SDL_FreeSurface(image);
			}

			prev_frame = curr_frame;

			frame_t* new_frame = frame_alloc();
			(*new_frame) = curr_frame;

			fprintf(stderr, "frame %u: %u tiles (%lu bytes)\n", frame_index, tile_offset, tile_offset * sizeof(tile_t));
			fprintf(stderr, "   %u tiles saved (%lu bytes)\n", tile_collisions, tile_collisions * sizeof(tile_t));
			SDL_RenderPresent(rdr);
		}
	}
	while (0);

	SDL_DestroyRenderer(rdr);
	SDL_DestroyWindow(win);

	IMG_Quit();
	SDL_Quit();

	FILE* fp = fopen("./anim.bin", "wb");
	if (fp)
	{
		fwrite(&tile_offset, sizeof(tile_offset), 1, fp);
		fwrite(tiles, sizeof(tile_t), tile_offset, fp);

		fwrite(&frame_offset, sizeof(frame_offset), 1, fp);
		fwrite(frames, sizeof(frame_t), frame_offset, fp);

		fclose(fp);
	}

	fprintf(stderr, "%u tiles (%lu bytes), %u frames, (%lu bytes)\n", tile_offset, tile_offset * sizeof(tile_t), frame_offset, frame_offset * sizeof(frame_t));

	return ret;
}

