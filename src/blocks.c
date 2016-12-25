#include "blocks.h"

#include <string.h>
#include <stdio.h>

static uint32_t block_variants[] = {
    // blitter friendly
    0,
    BLOCK_FLIP_Y,
    BLOCK_INVERT,
    BLOCK_INVERT|BLOCK_FLIP_Y,
    // cpu
    BLOCK_FLIP_X,
    BLOCK_FLIP_X|BLOCK_FLIP_Y,
    BLOCK_INVERT|BLOCK_FLIP_X,
    BLOCK_INVERT|BLOCK_FLIP_X|BLOCK_FLIP_Y
};



void blocks_init(blocks_t* blocks)
{
	buffer_init(&(blocks->buffer), sizeof(block_t));

	for (size_t i = 0; i < BLOCK_HASH_SIZE; ++i)
		blocks->hash[i] = NO_BLOCK;
}

void blocks_release(blocks_t* blocks)
{
	buffer_release(&(blocks->buffer));
}

block_t build_block(const uint8_t* pixels, uint8_t threshold, int32_t pitch)
{
	block_t temp;

	for (size_t y = 0; y < BLOCK_HEIGHT; ++y)
	{
		for (size_t x = 0; x < BLOCK_WIDTH; x += 8)
		{
			uint8_t bits = 0;
			for(size_t i = 0; i < 8; ++i)
			{
				bits = (bits << 1) | ((pixels[x + (7 - i) + (y * pitch)] > threshold) ? 1 : 0);
			}
			temp.bits[(x / 8) + y * (BLOCK_WIDTH / 8)] = bits;
		}
	}

	temp.count = 1;
    temp.next = NO_BLOCK;
	temp.remap = NO_BLOCK;
    temp.incoming = 0;

	return temp;
}

static uint32_t count_bits(uint8_t x)
{
	x = (x & 0x55) + ((x >> 1) & 0x55);
	x = (x & 0x33) + ((x >> 2) & 0x33);
	return (x & 0x0f) + ((x >> 4) & 0x0f);
}

uint32_t hash_block(const block_t* block)
{
	uint32_t temp = 0;
	for (const uint8_t* begin = block->bits, *end = block->bits + sizeof(block->bits); begin != end; ++begin)
	{
		temp += count_bits(*begin);
	}

	return temp;
}

static block_t block_flip_x(const block_t* in)
{
	block_t out;

	for (size_t y = 0; y < BLOCK_HEIGHT; ++y)
	{
		const uint8_t* iny = &(in->bits[y * (BLOCK_WIDTH / 8)]);
		uint8_t* outy = &(out.bits[BLOCK_HEIGHT - (y + 1) * (BLOCK_WIDTH / 8)]);

		for (size_t x = 0; x < BLOCK_WIDTH / 8; ++x)
		{
			uint8_t t = iny[x];
			t = ((t & 0x55) << 1) | ((t & 0xaa) >> 1);
			t = ((t & 0x33) << 2) | ((t & 0xcc) >> 2);
			t = ((t & 0x0f) << 4) | ((t & 0xf0) >> 4);
			outy[(BLOCK_WIDTH / 8) - (x + 1)] = t;
		}
	}
	return out;
}

static block_t block_flip_y(const block_t* in)
{
	block_t out;
	for (size_t y = 0; y < BLOCK_HEIGHT; ++y)
	{
		const uint8_t* iny = &(in->bits[y * (BLOCK_WIDTH / 8)]);
		uint8_t* outy = &(out.bits[(BLOCK_HEIGHT - (y + 1)) * (BLOCK_WIDTH / 8)]);
		memcpy(outy, iny, BLOCK_WIDTH / 8);
	}
	return out;
}

block_t block_invert(const block_t* in)
{
	block_t out;
	for (size_t i = 0; i < sizeof(out.bits); ++i)
		out.bits[i] = ~(in->bits[i]);
	return out;
}

block_t blocks_get(const blocks_t* blocks, block_index_t index)
{
	uint32_t offset = index & ~BLOCK_BITS_MASK;
	block_t in = *((const block_t*)buffer_get(&(blocks->buffer), offset));

    block_t temp = in;
	if (index & BLOCK_FLIP_X)
		temp = block_flip_x(&temp);
	if (index & BLOCK_FLIP_Y)
		temp = block_flip_y(&temp);
	if (index & BLOCK_INVERT)
		temp = block_invert(&temp);

	return temp;
}

size_t block_diff(const block_t* a, const block_t* b)
{
    size_t count = 0;
    for (size_t i = 0; i < sizeof(a->bits); ++i)
    {
        uint8_t t = a->bits[i] ^ b->bits[i];
        count += count_bits(t);
    }

    return count;
}

#define sizeof_array(x) (sizeof(x) / sizeof(x[0]))

block_index_t blocks_match(blocks_t* blocks, const block_t* block)
{
	for (size_t i = 0; i < sizeof_array(block_variants); ++i)
	{
		uint32_t variant = block_variants[i];

		block_t temp = *block;

		if (variant & BLOCK_FLIP_X)
			temp = block_flip_x(&temp);
		if (variant & BLOCK_FLIP_Y)
			temp = block_flip_y(&temp);
		if (variant & BLOCK_INVERT)
			temp = block_invert(&temp);

		uint32_t hash = hash_block(&temp) & (BLOCK_HASH_SIZE-1);
		uint32_t index = blocks->hash[hash];

		while (index != NO_BLOCK)
		{
			const block_t* candidate = buffer_get(&(blocks->buffer), index);
			if (!memcmp(&(candidate->bits), &(temp.bits), sizeof(temp.bits)))
				break;
			index = candidate->next;
		}

		if (index != NO_BLOCK)
		{
			block_t* match = buffer_get(&(blocks->buffer), index);
			return variant | index;
		}
	}

	return NO_BLOCK;
}

block_index_t blocks_insert(blocks_t* blocks, const block_t* block)
{
	block_index_t index = blocks_match(blocks, block);
	if (index != NO_BLOCK)
	{
	    block_t* found = buffer_get(&(blocks->buffer), index & ~BLOCK_BITS_MASK);
	    found->count++;
		return index;
	}

	block_t* temp = buffer_alloc(&(blocks->buffer), 1);
	memcpy(&(temp->bits), &(block->bits), sizeof(temp->bits));

	uint32_t offset = buffer_offset(&(blocks->buffer), temp);

	uint32_t hash = hash_block(temp) & (BLOCK_HASH_SIZE-1);
	temp->next = blocks->hash[hash];
	blocks->hash[hash] = offset;

	temp->count = 1;
    temp->remap = NO_BLOCK;
    temp->incoming = 0;

	return offset;
}

void blocks_find_matches(blocks_t* blocks, size_t max_error)
{
    size_t matches = 0;

    for (size_t i = 0, n = buffer_count(&(blocks->buffer)); i < n; ++i)
    {
        block_t* curr = buffer_get(&(blocks->buffer), i);
        uint32_t hash = hash_block(curr);

        if (curr->count == 0)
            continue;

        int max_diff = max_error;
        uint32_t best_match = NO_BLOCK;

        for (size_t j = 0; j < 1/*sizeof_array(block_variants)*/; ++j)
        {
            uint32_t flags = block_variants[j];

            block_t temp = *curr;
            if (flags & BLOCK_FLIP_X)
                temp = block_flip_x(&temp);
            if (flags & BLOCK_FLIP_Y)
                temp = block_flip_y(&temp);
            if (flags & BLOCK_INVERT)
                temp = block_invert(&temp);

            for (size_t k = 0; k < max_error * 2; ++k)
            {
                uint32_t key = (hash + k - max_error) & (BLOCK_HASH_SIZE-1);

                uint32_t index = blocks->hash[key];

                while (index != NO_BLOCK)
                {
                    uint32_t curr_index = index;
                    const block_t* candidate = buffer_get(&(blocks->buffer), index);
                    index = candidate->next;

                    if (curr_index == i)
                        continue;

                    if (curr->count >= candidate->count)
                        continue;

                    int diff = block_diff(&temp, candidate);
                    if (diff > max_diff)
                        continue;

                    max_diff = diff;
                    best_match = curr_index|flags;
                }
            }
        }

        if (best_match != NO_BLOCK)
        {
            curr->remap = best_match;
            ++matches;
        }

        fprintf(stderr, "\rmatching blocks: %lu/%lu, matches: %lu", i + 1, n, matches);
    }
    fprintf(stderr, "\n");
}

void blocks_reduce(blocks_t* blocks)
{
    size_t reductions = 0;

    for (size_t i = 0, n = buffer_count(&(blocks->buffer)); i < n; ++i)
    {
        block_t* root = (block_t*)buffer_get(&(blocks->buffer), i);
        if (root->count == 0 || root->remap == NO_BLOCK)
            continue;

        block_t* curr = root;
        uint32_t best_index = i;
        uint32_t count = 0;
        while (curr->remap != NO_BLOCK)
        {
            count += curr->count;

            uint32_t index = curr->remap & ~BLOCK_BITS_MASK;
            block_t* next = (block_t*)buffer_get(&(blocks->buffer), index);
            if (next->count < count)
            {
                curr->count = count;
                curr->remap = NO_BLOCK;
                break;
            }

            best_index = curr->remap;
            curr = next;
        }

        uint32_t active_flags = 0;
        while (root != curr)
        {
            uint32_t index = root->remap & ~BLOCK_BITS_MASK;
            uint32_t flags = root->remap & BLOCK_BITS_MASK;
            block_t* next = (block_t*)buffer_get(&(blocks->buffer), index);

            active_flags ^= flags;
            root->remap = best_index^active_flags;
            root->count = 0;

            root = next;
            reductions++;
        }

        fprintf(stderr, "\rreducing blocks: %lu/%lu, reductions: %lu", i + 1, n, reductions);
    }
    fprintf(stderr, "\n");
}

void block_render(uint8_t* pixels, const block_t* block, uint32_t pitch)
{
    for (size_t y = 0; y < BLOCK_HEIGHT; ++y)
    {
        for (size_t x = 0; x < BLOCK_WIDTH; x += 8)
        {
            uint8_t bits = block->bits[x + y * (BLOCK_WIDTH / 8)];
            uint8_t* out = &pixels[x + y * pitch];
            for (size_t i = 0; i < 8; ++i)
            {
                *out++ = (bits >> i) & 1 ? 255 : 0;
            }
        }
    }
}

void blocks_rebuild(blocks_t* blocks, block_index_t* remaps)
{
    buffer_t old = blocks->buffer;
    buffer_init(&(blocks->buffer), old.elemsize);

    for (size_t i = 0; i < BLOCK_HASH_SIZE; ++i)
    {
        blocks->hash[i] = NO_BLOCK;
    }

    for (size_t i = 0, n = buffer_count(&old); i < n; ++i)
    {
        block_t* old_block = (block_t*)buffer_get(&old, i);
        if (old_block->count == 0)
        {
            remaps[i] = NO_BLOCK;
            continue;
        }

        block_t* new_block = (block_t*)buffer_alloc(&(blocks->buffer), 1);

        uint32_t hash = hash_block(old_block) & (BLOCK_HASH_SIZE-1);
        uint32_t offset = buffer_offset(&(blocks->buffer), new_block);

        memcpy(&(new_block->bits), &(old_block->bits), sizeof(new_block->bits));
        new_block->count = old_block->count;

        new_block->next = blocks->hash[hash];
        blocks->hash[hash] = offset;

        new_block->remap = NO_BLOCK;
        old_block->remap = offset;
        new_block->incoming = 0;

        remaps[i] = offset;
    }

    for (size_t i = 0, n = buffer_count(&old); i < n; ++i)
    {
        const block_t* old_block = (const block_t*)buffer_get(&old, i);
        if (old_block->count > 0)
            continue;

        uint32_t index = old_block->remap & ~BLOCK_BITS_MASK;
        uint32_t flags = old_block->remap & BLOCK_BITS_MASK;

        block_t* target_block = (block_t*)buffer_get(&old, index);
        remaps[i] = target_block->remap | flags;
    }

    buffer_release(&old);
}

#define sizeof_member(type, member) sizeof(((type *)0)->member)
static const size_t BLOCK_DATA_SIZE = sizeof_member(block_t, bits);

size_t blocks_load(const buffer_t* in, size_t offset, size_t count, blocks_t* blocks)
{
    for (size_t i = 0; i < BLOCK_HASH_SIZE; ++i)
    {
        blocks->hash[i] = NO_BLOCK;
    }

    for (size_t i = 0, n = count; i < n; ++i)
    {
        block_t* block = buffer_alloc(&(blocks->buffer), 1);
        uint8_t* data = buffer_get(in, offset);

        memcpy(block->bits, data, BLOCK_DATA_SIZE);

        uint32_t hash = hash_block(block) & (BLOCK_HASH_SIZE-1);

        block->next = blocks->hash[hash];
        blocks->hash[hash] = i;

        block->count = 0;
        block->remap = NO_BLOCK;
        block->incoming = 0;

        offset += BLOCK_DATA_SIZE;
    }

    return offset;
}

void blocks_save(buffer_t* out, const blocks_t* blocks)
{
	for (size_t i = 0, n = buffer_count(&(blocks->buffer)); i < n; ++i)
	{
		const block_t* block = buffer_get(&(blocks->buffer), i);
		buffer_add(out, &(block->bits), sizeof(block->bits));
	}
}
