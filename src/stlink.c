/*
 * Copyright (c) 2018 Jean THOMAS.
 * Copyright (c) 2022-2024 1BitSquared <info@1bitsquared.com>
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

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_MEAN_AND_LEAN
#include <io.h>
#include <windows.h>
#define mssleep(ms) Sleep(ms)
#else
#include <sys/mman.h>
#include <unistd.h>
#define O_BINARY O_NOCTTY
#define mssleep(ms) usleep(ms * 1000U)
#endif
#include <string.h>
#include <errno.h>
#include <libusb.h>

#include "crypto.h"
#include "stlink.h"
#include "buffer_utils.h"

#define USB_TIMEOUT 5000U

#define DFU_DETACH    0x00U
#define DFU_DNLOAD    0x01U
#define DFU_UPLOAD    0x02U
#define DFU_GETSTATUS 0x03U
#define DFU_CLRSTATUS 0x04U
#define DFU_GETSTATE  0x05U
#define DFU_ABORT     0x06U
#define DFU_EXIT      0x07U
#define ST_DFU_INFO   0xF1U
#define ST_DFU_MAGIC  0xF3U

#define GET_COMMAND                 0x00U
#define SET_ADDRESS_POINTER_COMMAND 0x21U
#define ERASE_COMMAND               0x41U
#define ERASE_SECTOR_COMMAND        0x42U
#define READ_UNPROTECT_COMMAND      0x92U

static int stlink_erase(stlink_info_s *info, uint32_t address);
static int stlink_set_address(stlink_info_s *info, uint32_t address);
static bool stlink_dfu_status(stlink_info_s *info, dfu_status_s *status);

uint16_t stlink_dfu_mode(libusb_device_handle *const dev_handle, const bool trigger)
{
	uint8_t data[16] = {0xf9U};
	if (trigger)
		data[1] = DFU_DNLOAD;

	int rw_bytes = 0;
	/* Write */
	int res = libusb_bulk_transfer(dev_handle, 1 | LIBUSB_ENDPOINT_OUT, data, sizeof(data), &rw_bytes, USB_TIMEOUT);
	if (res) {
		fprintf(stderr, "USB transfer failure\n");
		return -1;
	}
	if (!trigger) {
		/* Read */
		libusb_bulk_transfer(dev_handle, 1 | LIBUSB_ENDPOINT_IN, data, 2, &rw_bytes, USB_TIMEOUT);
		if (res) {
			fprintf(stderr, "stlink_read_info() failure\n");
			return -1;
		}
	}
	return read_be2(data, 0);
}

bool stlink_read_info(stlink_info_s *info)
{
	uint8_t data[20] = {
		ST_DFU_INFO,
		0x80U,
	};

	int rw_bytes = 0;
	/* Write */
	int res = libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_out, data, 16, &rw_bytes, USB_TIMEOUT);
	if (res) {
		fprintf(stderr, "stlink_read_info out transfer failure\n");
		return false;
	}

	/* Read */
	res = libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_in, data, 6, &rw_bytes, USB_TIMEOUT);
	if (res) {
		fprintf(stderr, "stlink_read_info in transfer failure\n");
		return false;
	}

	info->stlink_version = data[0] >> 4;

	if (info->stlink_version < 3) {
		info->jtag_version = (data[0] & 0x0F) << 2 | (data[1] & 0xC0) >> 6;
		info->swim_version = data[1] & 0x3F;
		info->loader_version = read_le2(data, 4);

	} else {
		//info->product_id = data[3] << 8 | data[2];

		memset(data, 0, sizeof(data));

		data[0] = 0xfbU;
		data[1] = 0x80U;

		/* Write */
		res = libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_out, data, 16, &rw_bytes, USB_TIMEOUT);
		if (res) {
			fprintf(stderr, "USB transfer failure\n");
			return false;
		}

		/* Read */
		res = libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_in, data, 12, &rw_bytes, USB_TIMEOUT);
		if (res) {
			fprintf(stderr, "USB transfer failure\n");
			return false;
		}

		info->jtag_version = data[2];
		info->swim_version = data[1];
		info->loader_version = read_le2(data, 10);
	}

	memset(data, 0, sizeof(data));

	data[0] = ST_DFU_MAGIC;
	data[1] = 0x08U;

	/* Write */
	res = libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_out, data, 16, &rw_bytes, USB_TIMEOUT);
	if (res) {
		fprintf(stderr, "USB transfer failure\n");
		return false;
	}

	/* Read */
	libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_in, data, 20, &rw_bytes, USB_TIMEOUT);
	if (res) {
		fprintf(stderr, "USB transfer failure\n");
		return false;
	}

	memcpy(info->id, data + 8, 12);

	/* Firmware encryption key generation */
	memcpy(info->firmware_key, data, 4);
	memcpy(info->firmware_key + 4, data + 8, 12);
	if (info->stlink_version < 3)
		stlink_aes((unsigned char *)"I am key, wawawa", info->firmware_key, 16);
	else
		stlink_aes((unsigned char *)" found...STlink ", info->firmware_key, 16);
	return true;
}

uint16_t stlink_current_mode(stlink_info_s *info)
{
	uint8_t data[16] = {0xf5U};

	int rw_bytes = 0;
	/* Write */
	int res =
		libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_out, data, sizeof(data), &rw_bytes, USB_TIMEOUT);
	if (res) {
		fprintf(stderr, "USB transfer failure\n");
		return UINT16_MAX;
	}

	/* Read */
	res = libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_in, data, 2, &rw_bytes, USB_TIMEOUT);
	if (res) {
		fprintf(stderr, "stlink_read_info() failure\n");
		return UINT16_MAX;
	}

	return read_be2(data, 0);
}

uint16_t stlink_checksum(const uint8_t *const firmware, const size_t len)
{
	uint16_t ret = 0;
	for (size_t offset = 0; offset < len; ++offset)
		ret += firmware[offset];
	return ret;
}

int stlink_dfu_download(stlink_info_s *info, unsigned char *data, const size_t data_len, const uint16_t wBlockNum)
{
	if (wBlockNum >= 2 && info->stlink_version == 3)
		stlink_aes((uint8_t *)" .ST-Link.ver.3.", data, data_len);

	uint8_t download_request[16] = {
		ST_DFU_MAGIC,
		DFU_DNLOAD,
	};
	write_le2(download_request, 2, wBlockNum);                       /* wValue */
	write_le2(download_request, 4, stlink_checksum(data, data_len)); /* wIndex */
	write_le2(download_request, 6, data_len);                        /* wLength */

	if (wBlockNum >= 2)
		stlink_aes(info->firmware_key, data, data_len);

	int rw_bytes = 0;
	int res = libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_out, download_request,
		sizeof(download_request), &rw_bytes, USB_TIMEOUT);
	if (res || rw_bytes != sizeof(download_request)) {
		fprintf(stderr, "USB transfer failure\n");
		return -1;
	}
	res = libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_out, data, data_len, &rw_bytes, USB_TIMEOUT);
	if (res || rw_bytes != (int)data_len) {
		fprintf(stderr, "USB transfer failure 1\n");
		return -1;
	}

	dfu_status_s dfu_status;
	if (!stlink_dfu_status(info, &dfu_status))
		return -1;

	if (dfu_status.bState != dfuDNBUSY) {
		fprintf(stderr, "Unexpected DFU state : %d\n", dfu_status.bState);
		return -2;
	}

	if (dfu_status.bStatus != OK) {
		fprintf(stderr, "Unexpected DFU status : %d\n", dfu_status.bStatus);
		return -3;
	}

	mssleep(dfu_status.bwPollTimeout);

	if (!stlink_dfu_status(info, &dfu_status))
		return -1;

	if (dfu_status.bState == dfuDNLOAD_IDLE)
		return 0;

	if (dfu_status.bStatus == errVENDOR)
		fprintf(stderr, "Read-only protection active\n");
	else if (dfu_status.bStatus == errTARGET)
		fprintf(stderr, "Invalid address error\n");
	else
		fprintf(stderr, "Unknown error : %d\n", dfu_status.bStatus);
	return -3;
}

bool stlink_dfu_status(stlink_info_s *const info, dfu_status_s *const status)
{
	uint8_t data[16] = {
		ST_DFU_MAGIC,
		DFU_GETSTATUS,
	};
	data[6] = 0x06; /* wLength */

	int rw_bytes = 0;
	int res = libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_out, data, 16, &rw_bytes, USB_TIMEOUT);
	if (res || rw_bytes != 16) {
		fprintf(stderr, "USB transfer failure\n");
		return false;
	}
	res = libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_in, data, 6, &rw_bytes, USB_TIMEOUT);
	if (res || rw_bytes != 6) {
		fprintf(stderr, "USB transfer failure\n");
		return false;
	}

	status->bStatus = data[0];
	status->bwPollTimeout = data[1] | ((uint32_t)data[2] << 8U) | ((uint32_t)data[3] << 16U);
	status->bState = data[4];
	status->iString = data[5];
	return true;
}

int stlink_erase(stlink_info_s *const info, const uint32_t address)
{
	uint8_t command[5] = {ERASE_COMMAND};
	write_le4(command, 1, address);
	return stlink_dfu_download(info, command, sizeof(command), 0);
}

int stlink_sector_erase(stlink_info_s *const info, const uint32_t sector)
{
	uint8_t command[5] = {
		ERASE_SECTOR_COMMAND,
		sector & 0xffU,
	};
	return stlink_dfu_download(info, command, sizeof(command), 0);
}

int stlink_set_address(stlink_info_s *const info, const uint32_t address)
{
	uint8_t command[5] = {SET_ADDRESS_POINTER_COMMAND};
	write_le4(command, 1, address);
	return stlink_dfu_download(info, command, sizeof(command), 0);
}

int stlink_flash(stlink_info_s *info, const char *filename)
{
	const int fd = open(filename, O_RDONLY | O_BINARY);
	if (fd == -1) {
		const int error = errno;
		fprintf(stderr, "Opening file failed (%d): %s\n", error, strerror(error));
		return -1;
	}

	struct stat file_stat;
	if (fstat(fd, &file_stat) < 0 || file_stat.st_size <= 0) {
		const int error = errno;
		fprintf(stderr, "Failed to get firmware file length (%d): %s\n", error, strerror(error));
		return -1;
	}
	const size_t file_size = (size_t)file_stat.st_size;

#ifdef _WIN32
	const HANDLE file_handle = (HANDLE)_get_osfhandle(fd);
	HANDLE mapping =
		CreateFileMappingA(file_handle, NULL, PAGE_READONLY, file_size >> 32U, file_size & UINT32_MAX, NULL);
	if (mapping == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		fprintf(stderr, "Failed to memory map firmware (%lu)\n", error);
		return -1;
	}

	const uint8_t *const firmware = (const uint8_t *)MapViewOfFile(mapping, FILE_MAP_READ, 0U, 0U, 0U);
	if (firmware == NULL) {
		DWORD error = GetLastError();
		fprintf(stderr, "Failed to memory map firmware (%lu)\n", error);
		return -1;
	}
#else
	const uint8_t *const firmware = (const uint8_t *)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (firmware == MAP_FAILED) {
		const int error = errno;
		fprintf(stderr, "Failed to memory map firmware (%d): %s\n", error, strerror(error));
		return -1;
	}
#endif

	printf("Type %s\n", info->stinfo_bl_type == STLINK_BL_V3 ? "V3" : "V2");
	uint32_t base_offset = info->stinfo_bl_type == STLINK_BL_V3 ? 0x08020000U : 0x08004000U;
	const size_t chunk_size = 1U << 10U;
	uint8_t chunk_buffer[1024U];
	size_t amount = chunk_size;
	for (size_t flashed_bytes = 0; flashed_bytes < file_size; flashed_bytes += amount) {
		if (flashed_bytes + chunk_size > file_size)
			amount = file_size - flashed_bytes;

		if (info->stinfo_bl_type == STLINK_BL_V3) {
			uint32_t address = base_offset + flashed_bytes;
			static const uint32_t sector_start[8] = {
				0x08000000U,
				0x08004000U,
				0x08008000U,
				0x0800C000U,
				0x08010000U,
				0x08020000U,
				0x08040000U,
				0x08060000U,
			};
			size_t sector = SIZE_MAX;
			size_t i;
			for (i = 0; i < 8; i++) {
				if (sector_start[i] == address) {
					sector = i;
					break;
				}
			}
			if (i < 8) {
				const int res = stlink_sector_erase(info, sector);
				if (res) {
					fprintf(stderr, "Erase sector %zu failed\n", sector);
					return res;
				}
				printf("Erase sector %zu done\n", sector);
			}
		} else {
			const int res = stlink_erase(info, base_offset + flashed_bytes);
			if (res) {
				fprintf(stderr, "Erase error at 0x%08zx\n", base_offset + flashed_bytes);
				return res;
			}
		}
		int res = stlink_set_address(info, base_offset + flashed_bytes);
		if (res) {
			fprintf(stderr, "set address error at 0x%08zx\n", base_offset + flashed_bytes);
			return res;
		}
		memcpy(chunk_buffer, firmware + flashed_bytes, chunk_size);
		res = stlink_dfu_download(info, chunk_buffer, chunk_size, 2U);
		if (res) {
			fprintf(stderr, "Download error at 0x%08zx\n", base_offset + flashed_bytes);
			return res;
		}

		printf(".");
		fflush(stdout); /* Flush stdout buffer */
	}

#ifdef _WIN32
	UnmapViewOfFile(firmware);
	CloseHandle(mapping);
#else
	munmap((void *)firmware, file_size);
#endif
	close(fd);

	printf("\n");
	return 0;
}

bool stlink_exit_dfu(stlink_info_s *const info)
{
	uint8_t data[16] = {
		ST_DFU_MAGIC,
		DFU_EXIT,
	};

	int rw_bytes = 0;
	const int res =
		libusb_bulk_transfer(info->stinfo_dev_handle, info->stinfo_ep_out, data, 16, &rw_bytes, USB_TIMEOUT);
	if (res || rw_bytes != 16) {
		fprintf(stderr, "USB transfer failure\n");
		return false;
	}
	return true;
}
