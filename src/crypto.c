/*
 * Copyright (c) 2018 Jean THOMAS.
 * Copyright (c) 2024 1BitSquared <info@1bitsquared.com>
 * Modified by Rachel Mant <git@dragonmux.network>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "../tiny-AES-c/aes.h"
#include "crypto.h"
#include "buffer_utils.h"

void stlink_aes(const void *const key, uint8_t *const data, const size_t length)
{
	uint8_t key_be[16U] = {0};
	/* Read the key (as 4-byte little endian) and write it to our internal buffer as 4-byte big endian */
	for (size_t offset = 0U; offset < 16U; offset += 4U)
		write_be4(key_be, offset, read_le4(key, offset));

	/* Initialise the cryptography subsystem */
	struct AES_ctx ctx;
	AES_init_ctx(&ctx, key_be);

	/* Iterate through the data, 1 AES block size at a time */
	for (size_t block_offset = 0U; block_offset < length; block_offset += 16U) {
		/* Convert the input data chunk from 4-byte LE to 4-byte BE */
		for (size_t offset = 0; offset < length; offset += 4U)
			write_be4(data, block_offset + offset, read_le4(data, block_offset + offset));
		/* Run the operation (due to a bug in ST's impl, it's encrypt both ways) */
		AES_ECB_encrypt(&ctx, data + block_offset);
		/* Convert the buffer back */
		for (size_t offset = 0; offset < length; offset += 4U)
			write_le4(data, block_offset + offset, read_be4(data, block_offset + offset));
	}
}
