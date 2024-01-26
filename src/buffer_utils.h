/*
 * Copyright (c) 2024 1BitSquared <info@1bitsquared.com>
 * Written by Rachel Mant <git@dragonmux.network>
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

#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stdint.h>
#include <stddef.h>

static inline void write_le2(uint8_t *const buffer, const size_t offset, const uint16_t value)
{
	buffer[offset + 0U] = value & 0xffU;
	buffer[offset + 1U] = (value >> 8U) & 0xffU;
}

static inline void write_le4(uint8_t *const buffer, const size_t offset, const uint32_t value)
{
	buffer[offset + 0U] = value & 0xffU;
	buffer[offset + 1U] = (value >> 8U) & 0xffU;
	buffer[offset + 2U] = (value >> 16U) & 0xffU;
	buffer[offset + 3U] = (value >> 24U) & 0xffU;
}

static inline void write_be4(uint8_t *const buffer, const size_t offset, const uint32_t value)
{
	buffer[offset + 0U] = (value >> 24U) & 0xffU;
	buffer[offset + 1U] = (value >> 16U) & 0xffU;
	buffer[offset + 2U] = (value >> 8U) & 0xffU;
	buffer[offset + 3U] = value & 0xffU;
}

static inline uint16_t read_le2(const uint8_t *const buffer, const size_t offset)
{
	return buffer[offset + 0U] | ((uint16_t)buffer[offset + 1U] << 8U);
}

static inline uint16_t read_be2(const uint8_t *const buffer, const size_t offset)
{
	return ((uint16_t)buffer[offset + 0U] << 8U) | buffer[offset + 1U];
}

static inline uint32_t read_le4(const uint8_t *const buffer, const size_t offset)
{
	return buffer[offset + 0U] | ((uint32_t)buffer[offset + 1U] << 8U) | ((uint32_t)buffer[offset + 2U] << 16U) |
		((uint32_t)buffer[offset + 3U] << 24U);
}

static inline uint32_t read_be4(const uint8_t *const buffer, const size_t offset)
{
	return ((uint32_t)buffer[offset + 0U] << 24U) | ((uint32_t)buffer[offset + 1U] << 16U) |
		((uint32_t)buffer[offset + 2U] << 8U) | buffer[offset + 3U];
}

#endif /*BUFFER_UTILS_H*/
