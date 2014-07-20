#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <unistd.h>

#include "video.h"

//#define DEBUG_RENDER
#define WHITE_FILTER 2
#define FILTER_THRESHOLD 36

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

		SDL_Rect rect = { 0, 0, 1440, 1080 };

		SDL_BlitScaled(temp, &rect, result, NULL);
	}
	while (0);

	SDL_FreeSurface(temp);

	return result;
}

tile_t* tiles = NULL;
uint32_t tile_offset = 0, tile_reserve = 0;

raw_frame_t* frames = NULL;
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

uint8_t count_bits(uint8_t x)
{
	x = (x & 0x55) + ((x >> 1) & 0x55);
	x = (x & 0x33) + ((x >> 2) & 0x33);
	return (x & 0x0f) + ((x >> 4) & 0x0f);
}

uint32_t tile_compare(tile_t* a, tile_t* b, uint32_t mode)
{
	uint8_t* ta = a->data;
	uint8_t* tb = b->data;

	uint32_t value = 0;

	switch (mode)
	{
		case 0:
		{
			for (unsigned int y = 0; y < TILE_HEIGHT; ++y)
			{
				uint8_t* ta_row = ta;
				uint8_t* tb_row = tb;

				for (unsigned int x = 0; x < TILE_WIDTH; x += 8)
				{
					uint8_t da = *ta_row++;
					uint8_t db = *tb_row++;

					uint8_t x = da ^ db;
					value += count_bits(x) + WHITE_FILTER * count_bits(db & x);
				}

				ta += (TILE_WIDTH / 8);
				tb += (TILE_WIDTH / 8);
			}
		}
		break;

		case TILE_FLIP_X:
		{
			for (unsigned int y = 0; y < TILE_HEIGHT; ++y)
			{
				uint8_t* ta_row = ta;
				uint8_t* tb_row = tb + (TILE_WIDTH/8) - 1;

				for (unsigned int x = 0; x < TILE_WIDTH; x += 8)
				{
					uint8_t da = *ta_row++;
					uint8_t db = *tb_row--;

					db = ((db & 0x55) << 1) | ((db & 0xaa) >> 1);
					db = ((db & 0x33) << 2) | ((db & 0xcc) >> 2);
					db = ((db & 0x0f) << 4) | ((db & 0xf0) >> 4);

					uint8_t x = da ^ db;
					value += count_bits(x) + WHITE_FILTER * count_bits(db & x);
				}

				ta += (TILE_WIDTH / 8);
				tb += (TILE_WIDTH / 8);
			}
		}
		break;

		case TILE_FLIP_Y:
		{
			tb += (TILE_WIDTH/8) * (TILE_HEIGHT-1);

			for (unsigned int y = 0; y < TILE_HEIGHT; ++y)
			{
				uint8_t* ta_row = ta;
				uint8_t* tb_row = tb;

				for (unsigned int x = 0; x < TILE_WIDTH; x += 8)
				{
					uint8_t da = *ta_row++;
					uint8_t db = *tb_row++;

					uint8_t x = da ^ db;
					value += count_bits(x) + WHITE_FILTER * count_bits(db & x);
				}

				ta += (TILE_WIDTH / 8);
				tb -= (TILE_WIDTH / 8);
			}
		}
		break;

		case TILE_FLIP_X|TILE_FLIP_Y:
		{
			tb += (TILE_WIDTH/8) * (TILE_HEIGHT-1);

			for (unsigned int y = 0; y < TILE_HEIGHT; ++y)
			{
				uint8_t* ta_row = ta;
				uint8_t* tb_row = tb + (TILE_WIDTH/8) - 1;

				for (unsigned int x = 0; x < TILE_WIDTH; x += 8)
				{
					uint8_t da = *ta_row++;
					uint8_t db = *tb_row--;

					db = ((db & 0x55) << 1) | ((db & 0xaa) >> 1);
					db = ((db & 0x33) << 2) | ((db & 0xcc) >> 2);
					db = ((db & 0x0f) << 4) | ((db & 0xf0) >> 4);

					uint8_t x = da ^ db;
					value += count_bits(x) + WHITE_FILTER * count_bits(db & x);
				}

				ta += (TILE_WIDTH / 8);
				tb -= (TILE_WIDTH / 8);
			}
		}
		break;
	}
	return value * value;
}

raw_frame_t* frame_alloc()
{
	if (frame_offset == frame_reserve)
	{
		uint32_t new_reserve = frame_reserve > 0 ? frame_reserve * 2 : 16;
		raw_frame_t* new_frames = malloc(new_reserve * sizeof(raw_frame_t));
		memcpy(new_frames, frames, frame_offset * sizeof(raw_frame_t));

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
#if defined(DEBUG_RENDER)
		if (SDL_Init(SDL_INIT_EVERYTHING))
#else
		if (SDL_Init(SDL_INIT_EVENTS))
#endif
		{
			fprintf(stderr, "Could not initialize SDL\n");
			break;
		}	

		if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG)
		{
			fprintf(stderr, "Could not initialize image: %s\n", IMG_GetError());
			break;
		}

#if defined(DEBUG_RENDER)
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
#endif
		ret = 0;

		const int first_frame = 1;
		const int last_frame = 6572;

		raw_frame_t prev_frame = { 0 };

		unsigned int tile_collisions = 0;
		header_t header = { 0 };
		if (fread(&header, 1, sizeof(header), stdin) > 0)
		{
			tile_offset = tile_reserve = ntohl(header.tiles);
			tiles = malloc(tile_offset * sizeof(tile_t));
			fread(tiles, sizeof(tile_t), tile_offset, stdin);
		}
		else
		{
			tile_t* black_tile = tile_alloc();
			memset(black_tile, 0, sizeof(tile_t));

			tile_t* white_tile = tile_alloc();
			memset(white_tile, 0xff, sizeof(tile_t));
		}

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

			if (rdr)
			{
				SDL_RenderClear(rdr);
			}

			char path[256];
			sprintf(path, "./images/image_%05d.png", frame_index);

			raw_frame_t curr_frame = prev_frame;

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

							const unsigned int thres = FILTER_THRESHOLD;
							unsigned int max_diff = thres * thres;


							for (uint32_t i = 0; i < tile_offset && (max_diff > 0); ++i)
							{
								uint32_t modes[4] = { 0, TILE_FLIP_X, TILE_FLIP_Y, TILE_FLIP_X|TILE_FLIP_Y };
								for (unsigned int j = 0; j < sizeof(modes) / sizeof(uint32_t); ++j)
								{
									unsigned int diff = tile_compare(&tile, &tiles[i], modes[j]);
									if (diff < max_diff)
									{
										max_diff = diff;
										tile_index = i | (modes[j] << TILE_BITS_SHIFT);
									}
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
							tile_index_t curr_tile = curr_frame.tiles[tile_address] & ~(TILE_BITS_MASK << TILE_BITS_SHIFT), prev_tile = prev_frame.tiles[tile_address] & ~(TILE_BITS_MASK << TILE_BITS_SHIFT);

							pixel[1] = curr_tile >= frametile_offset ? pixel[0] : 0x00;
							pixel[2] = curr_tile != prev_tile ? pixel[0] : 0x00;
						}
					}
				}
				SDL_UnlockSurface(image);

				if (rdr)
				{
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
			}

			prev_frame = curr_frame;

			raw_frame_t* new_frame = frame_alloc();
			(*new_frame) = curr_frame;

			fprintf(stderr, "frame %u: %u tiles (%lu bytes)\n", frame_index, tile_offset, tile_offset * sizeof(tile_t));
			fprintf(stderr, "   %u tiles saved (%lu bytes)\n", tile_collisions, tile_collisions * sizeof(tile_t));

			if (rdr)
			{
				SDL_RenderPresent(rdr);
			}
		}

		{
			frame_t prev_frame = { 0 };

			for (unsigned int i = 0; i < frame_offset; ++i)
			{
				raw_frame_t* curr_raw_frame = &frames[i];
				frame_t curr_frame;

				for (unsigned int j = 0; j < (SCREEN_WIDTH / TILE_WIDTH) * (SCREEN_HEIGHT / TILE_HEIGHT); ++j)
				{
					if (sizeof(tile_index_t) == sizeof(uint16_t))
					{
						curr_frame.tiles[j] = htons((curr_raw_frame->tiles[j] & ~ (TILE_BITS_MASK << TILE_BITS_SHIFT)) | ((curr_raw_frame->tiles[j] & (TILE_BITS_MASK << TILE_BITS_SHIFT)) >> TILE_BITS_SHIFT));
					}
					else
					{
						curr_frame.tiles[j] = htonl((curr_raw_frame->tiles[j] & ~ (TILE_BITS_MASK << TILE_BITS_SHIFT)) | ((curr_raw_frame->tiles[j] & (TILE_BITS_MASK << TILE_BITS_SHIFT)) >> TILE_BITS_SHIFT));
					}
				}

				tile_index_t* curr = curr_frame.tiles;
				tile_index_t* prev = prev_frame.tiles;

				tile_index_t* first = NULL;
				int skipping = 0;

				for (unsigned int j = 0, n = (SCREEN_WIDTH / TILE_WIDTH) * (SCREEN_HEIGHT / TILE_HEIGHT); j < n; ++j, ++curr, ++prev)
				{

					if (first && (curr - first) == 127)
					{
						if (skipping)
						{
							uint8_t* s = stream_alloc(1);
							*s = (uint8_t)(curr-first);
						}
						else
						{
							uint8_t* s = stream_alloc(1 + (curr-first) * sizeof(tile_index_t));
							*s = ((uint8_t)(curr-first))|0x80;
							memcpy(s+1, first, (curr-first) * sizeof(tile_index_t));
						}
						first = NULL; skipping = 0;
					}

					if (!first)
					{
						first = curr;
						skipping = (*curr == *prev) ? 1 : 0;
					}
					else
					{
						if (skipping && (*curr != *prev))
						{
							uint8_t* s = stream_alloc(1);
							*s = (uint8_t)(curr-first); 

							first = curr; skipping = 0;
						}
						else if (!skipping && (*curr == *prev))
						{
							uint8_t* s = stream_alloc(1 + (curr-first) * sizeof(tile_index_t));
							*s = ((uint8_t)(curr-first))|0x80;
							memcpy(s+1, first, (curr-first) * sizeof(tile_index_t));

							first = curr; skipping = 1;
						}
					}
				}

				if (first)
				{
					if (skipping)
					{
						uint8_t* s = stream_alloc(1);
						*s = (uint8_t)(curr-first);
					}
					else
					{
						uint8_t* s = stream_alloc(1 + (curr-first) * sizeof(tile_index_t));
						*s = ((uint8_t)(curr-first))|0x80;
						memcpy(s+1, first, (curr-first) * sizeof(tile_index_t));
					}
				}

				prev_frame = curr_frame;
			}
		}
	}
	while (0);

	SDL_DestroyRenderer(rdr);
	SDL_DestroyWindow(win);

	IMG_Quit();
	SDL_Quit();

	if (tile_offset > MAX_TILE_INDEX)
	{
		fprintf(stderr, "More than %u tiles allocated, out of room in tile table\n", MAX_TILE_INDEX);
		ret = 1;
	}

	if (isatty(fileno(stdout)))
	{
		fprintf(stderr, "Cannot write result to terminal.\n");
		ret = 1;
	}

	if (ret == 0)
	{
		size_t written = 0;
		header_t header = { 0 };
		header.tiles = htonl(tile_offset);
		header.frames = htonl(frame_offset);
		header.stream = htonl(stream_offset);
		written += fwrite(&header, 1, sizeof(header_t), stdout);
		fprintf(stderr, "header: %lu\n", written);


		written += fwrite(tiles, 1, sizeof(tile_t) * tile_offset, stdout);
		uint8_t pad[512] = { 0 };
		written += fwrite(pad, 1, 512 - (written & 511), stdout);
		fprintf(stderr, "tiles: %lu\n", written);
		written += fwrite(stream, 1, stream_offset, stdout);
		fprintf(stderr, "stream: %lu\n", written);

		fprintf(stderr, "%u tiles (%lu bytes), %u frames, (%lu bytes), %u stream bytes\n", tile_offset, tile_offset * sizeof(tile_t), frame_offset, frame_offset * sizeof(frame_t), stream_offset);
	}

	return ret;
}

