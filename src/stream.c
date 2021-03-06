#include "stream.h"

#include "frames.h"
#include "tiles.h"
#include "blocks.h"
#include "buffer.h"
#include "bits.h"

#include "../external/fastlz/fastlz.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <arpa/inet.h>

stream_t* stream_create()
{
	stream_t* stream = malloc(sizeof(stream_t));

	frames_init(&(stream->frames));
	tiles_init(&(stream->tiles));

	return stream;
}

uint8_t bits_needed(uint32_t value)
{
    uint8_t r = floor(log(value) / log(2)) + 1;
    return r;
}

uint32_t u32be(uint32_t in)
{
    return htonl(in);
}

uint16_t u16be(uint16_t in)
{
    return htons(in);
}

static void compress_buffer(buffer_t* out, const buffer_t* in)
{
    char tempbuf[STREAM_BLOCK_MAX_SIZE * 2];

    for (size_t offset = 0, n = buffer_count(in); offset < n;)
    {
        size_t block_size = (n-offset) > STREAM_BLOCK_MAX_SIZE ? STREAM_BLOCK_MAX_SIZE : (n-offset);

        size_t compressed_size = fastlz_compress(buffer_get(in, offset), block_size, tempbuf);
        if (compressed_size < block_size)
        {
            stream_block_t block_header;
            block_header.inlen = u16be(block_size-1);
            block_header.outlen = u16be((compressed_size-1)|STREAM_BLOCK_COMPRESSED);

            buffer_add(out, &block_header, sizeof(block_header));
            buffer_add(out, tempbuf, compressed_size);
        }
        else
        {
            stream_block_t block_header;
            block_header.inlen = u16be(block_size-1);
            block_header.outlen = u16be(block_size-1);

            buffer_add(out, &block_header, sizeof(block_header));
            buffer_add(out, buffer_get(in, offset), block_size);
        }

        offset += block_size;
    }
}

static void write_buffer(const char* filename, const buffer_t* in)
{
	FILE* out = fopen(filename, "wb");
	fwrite(buffer_get(in, 0), 1, buffer_count(in), out);
	fclose(out);
}


int stream_save(const stream_t* stream, FILE* out)
{
	buffer_t outbuf;
	buffer_init(&outbuf, 1);

	uint8_t tile_bits = bits_needed(buffer_count(&(stream->tiles.buffer))) + 3;
	uint8_t block_bits = bits_needed(buffer_count(&(stream->tiles.blocks.buffer))) + 3;

    buffer_t block_buffer;
    buffer_init(&block_buffer, 1);
	blocks_save(&block_buffer, &(stream->tiles.blocks));

    buffer_t tile_buffer;
    buffer_init(&tile_buffer, 1);
	tiles_save(&tile_buffer, &(stream->tiles), block_bits);

    buffer_t frame_buffer;
    buffer_init(&frame_buffer, 1);
	frames_save(&frame_buffer, &(stream->frames), tile_bits);

	size_t blocks_start = buffer_count(&outbuf);
        write_buffer("anim.blocks", &block_buffer);
    	compress_buffer(&outbuf, &block_buffer);
	size_t tiles_start = buffer_count(&outbuf);
    	write_buffer("anim.tiles", &tile_buffer);
	compress_buffer(&outbuf, &tile_buffer);
	size_t frames_start = buffer_count(&outbuf);
    	write_buffer("anim.frames", &frame_buffer);
	compress_buffer(&outbuf, &frame_buffer);
	size_t stream_end = buffer_count(&outbuf);

	fprintf(stderr, "blocks: %lu, (%lu -> %lu bytes)\ntiles: %lu (%lu -> %lu bytes)\nframes: %lu (%lu -> %lu bytes)\n",
		buffer_count(&(stream->tiles.blocks.buffer)), buffer_count(&block_buffer), tiles_start - blocks_start,
		buffer_count(&(stream->tiles.buffer)), buffer_count(&tile_buffer), frames_start - tiles_start,
		buffer_count(&(stream->frames.buffer)), buffer_count(&frame_buffer), stream_end - frames_start);

	stream_header_t header;
	header.blocks = u32be(buffer_count(&(stream->tiles.blocks.buffer)));
	header.tiles = u32be(buffer_count(&(stream->tiles.buffer)));
	header.frames = u32be(buffer_count(&(stream->frames.buffer)));
    header.size = u32be(buffer_count(&block_buffer) + buffer_count(&tile_buffer) + buffer_count(&frame_buffer));
    header.compressed_size = u32be(stream_end);
    header.tile_bits = u16be(tile_bits);
    header.block_bits = u16be(block_bits);

    int ret = 0;
    do
    {
        if ((ret = fwrite(&header, sizeof(header), 1, out)) < 1)
        {
            fprintf(stderr, "failed writing header\n");
            break;
        }

	    ret = fwrite(outbuf.data, outbuf.size, 1, out);
    }
    while (0);

    buffer_release(&block_buffer);
    buffer_release(&tile_buffer);
    buffer_release(&frame_buffer);

    buffer_release(&outbuf);

	return ret < 1 ? -1 : 0;
}

static int decompress_buffer(buffer_t* out, const buffer_t* in)
{
    char tempbuf[STREAM_BLOCK_MAX_SIZE];

    for (size_t offset = 0, n = buffer_count(in); offset < n;)
    {
        stream_block_t block_header;

        memcpy(&block_header, buffer_get(in, offset), sizeof(block_header));
        block_header.inlen = u16be(block_header.inlen);
        block_header.outlen = u16be(block_header.outlen);
        offset += sizeof(block_header);

        size_t insize = (block_header.outlen & ~STREAM_BLOCK_COMPRESSED) + 1;
        size_t outsize = block_header.inlen + 1;

        if (block_header.outlen & STREAM_BLOCK_COMPRESSED)
        {
            int ret = fastlz_decompress(buffer_get(in, offset), insize, tempbuf, sizeof(tempbuf));

            if (ret != outsize)
                return -1;

            buffer_add(out, tempbuf, outsize);
        }
        else
        {
            buffer_add(out, buffer_get(in, offset), insize);
        }

        offset += insize;
    }

    return 0;
}

int stream_load(stream_t* stream, FILE* in)
{
	stream_header_t header;
	if (fread(&header, sizeof(header), 1, in) < 1)
		return -1;

	header.blocks = u32be(header.blocks);
	header.tiles = u32be(header.tiles);
	header.frames = u32be(header.frames);
	header.size = u32be(header.size);
	header.compressed_size = u32be(header.compressed_size);
	header.tile_bits = u16be(header.tile_bits);
	header.block_bits = u16be(header.block_bits);

    fprintf(stderr, "blocks: %u, tiles: %u, frames: %u, size: %u (%u)\ntile bits: %u, block bits: %u\n",
                    header.blocks,
                    header.tiles,
                    header.frames,
                    header.size,
                    header.compressed_size,
                    header.tile_bits,
                    header.block_bits);

    buffer_t inbuf;
    buffer_init(&inbuf, 1);
    uint8_t* data = buffer_alloc(&inbuf, header.compressed_size);

    if (fread(data, header.compressed_size, 1, in) < 1)
    {
        buffer_release(&inbuf);
    }

    buffer_t temp;
    buffer_init(&temp, 1);

    if (decompress_buffer(&temp, &inbuf) < 0)
    {
        buffer_release(&temp);
        buffer_release(&inbuf);

        fprintf(stderr, "Failed to decompress buffer\n");
        return -1;
    }

    if (buffer_count(&temp) != header.size)
    {
        buffer_release(&temp);
        buffer_release(&inbuf);

        fprintf(stderr, "Decompressed buffer size mismatch\n");
        return -1;
    }

    size_t current = 0;
    current = blocks_load(&temp, current, header.blocks, &(stream->tiles.blocks));
    current = tiles_load(&temp, current, header.tiles, &(stream->tiles), header.block_bits);
    current = frames_load(&temp, current, header.frames, &(stream->frames), header.tile_bits);

    buffer_release(&temp);
    buffer_release(&inbuf);

    if (current != header.size)
    {
        fprintf(stderr, "Not all data in buffer consumed\n");
        return -1;
    }

	return 0;
}

int stream_dump(FILE* in, const char* basename)
{
	stream_header_t header;
	if (fread(&header, sizeof(header), 1, in) < 1)
		return -1;

	header.blocks = u32be(header.blocks);
	header.tiles = u32be(header.tiles);
	header.frames = u32be(header.frames);
	header.size = u32be(header.size);
	header.compressed_size = u32be(header.compressed_size);
	header.tile_bits = u16be(header.tile_bits);
	header.block_bits = u16be(header.block_bits);

    fprintf(stderr, "blocks: %u, tiles: %u, frames: %u, size: %u (%u)\ntile bits: %u, block bits: %u\n",
                    header.blocks,
                    header.tiles,
                    header.frames,
                    header.size,
                    header.compressed_size,
                    header.tile_bits,
                    header.block_bits);

    buffer_t inbuf;
    buffer_init(&inbuf, 1);
    uint8_t* data = buffer_alloc(&inbuf, header.compressed_size);

    if (fread(data, header.compressed_size, 1, in) < 1)
    {
        buffer_release(&inbuf);
    }

    buffer_t temp;
    buffer_init(&temp, 1);

    if (decompress_buffer(&temp, &inbuf) < 0)
    {
        buffer_release(&temp);
        buffer_release(&inbuf);

        fprintf(stderr, "Failed to decompress buffer\n");
        return -1;
    }

    if (buffer_count(&temp) != header.size)
    {
        buffer_release(&temp);
        buffer_release(&inbuf);

        fprintf(stderr, "Decompressed buffer size mismatch\n");
        return -1;
    }

    size_t offset = 0;
    {
        char namebuf[512];
        sprintf(namebuf, "%s.blocks", basename);
        size_t block_size = (BLOCK_WIDTH / 8) * BLOCK_HEIGHT;

        FILE* out = fopen(namebuf, "wb");
        fwrite(temp.data + offset, block_size, header.blocks, out);

        fclose(out);

        fprintf(stderr, "written blocks to %s (%lu bytes)\n", namebuf, block_size * header.blocks);

        offset += block_size * header.blocks;
    }

    {
        char namebuf[512];
        sprintf(namebuf, "%s.tiles", basename);
        size_t tile_size = (header.block_bits > 16 ? sizeof(uint32_t) : sizeof(uint16_t)) * TILE_INDEX_COUNT;

        FILE* out = fopen(namebuf, "wb");
        fwrite(temp.data + offset, tile_size, header.tiles, out);

        fclose(out);

        fprintf(stderr, "written tiles to %s (%lu bytes)\n", namebuf, tile_size * header.tiles);

        offset += tile_size * header.tiles;
    }

    {
        char namebuf[512];
        sprintf(namebuf, "%s.frames", basename);

        FILE* out = fopen(namebuf, "wb");
        fwrite(temp.data + offset, temp.size - offset, 1, out);

        fprintf(stderr, "written frames to %s (%lu bytes)\n", namebuf, temp.size - offset);

        fclose(out);
    }

    {
        char namebuf[512];
        sprintf(namebuf, "%s.asm", basename);

        FILE* out = fopen(namebuf, "w");

        fprintf(out, "anim_blocks       equ     %u\n", header.blocks);
        fprintf(out, "anim_tiles        equ     %u\n", header.tiles);
        fprintf(out, "anim_frames       equ     %u\n", header.frames);
        fprintf(out, "anim_tile_bits    equ     %u\n", header.tile_bits);
        fprintf(out, "anim_block_bits   equ     %u\n", header.block_bits);

        fprintf(stderr, "written header to %s\n", namebuf);

        fclose(out);
    }


    buffer_release(&temp);
    buffer_release(&inbuf);

	return 0;
}

void stream_destroy(stream_t* stream)
{
	frames_release(&(stream->frames));
	tiles_release(&(stream->tiles));
}

void stream_optimize_blocks(stream_t* stream, size_t passes, size_t max_error)
{
/*
    pass 1: identify blocks within a 8 pixel difference that can substitute (use the one with the highest count), add to tile weight
*/
    fprintf(stderr, "optimizing blocks...\n");

    for (size_t i = 0; i < passes; ++i)
    {
        blocks_find_matches(&(stream->tiles.blocks), max_error);
        blocks_reduce(&(stream->tiles.blocks));

        block_index_t* remaps = malloc(sizeof(block_index_t) * buffer_count(&(stream->tiles.blocks.buffer)));
        blocks_rebuild(&(stream->tiles.blocks), remaps);
        tiles_remap_blocks(&(stream->tiles), remaps);
        free(remaps);
    }
}

void stream_optimize_tiles(stream_t* stream, size_t max_error)
{
    fprintf(stderr, "optimizing tiles...\n");

    tiles_dedupe(&(stream->tiles));

    tile_index_t* remaps = malloc(sizeof(tile_index_t) * buffer_count(&(stream->tiles.buffer)));
    tiles_rebuild(&(stream->tiles), remaps);
    frames_remap_tiles(&(stream->frames), remaps);
    free(remaps);
}

void stream_optimize_frames(stream_t* stream)
{
    fprintf(stderr, "optimizing frames...\n");
/*

idea: identify tiles which are only briefly updated with hardly any difference, and see if they are then
updated the frame after - allow for some of them to go "missing"

- then we can also optimize away all tiles / blocks no longer in use

*/
}


void stream_shrink(stream_t* stream)
{
/*
shrink blocks and tiles while remapping users to remove dead space
*/
}
