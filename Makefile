# Copyright (c) 2018 Jean THOMAS.
# Copyright (c) 2022-2024 1BitSquared <info@1bitsquared.com>
# Modified by Rachel Mant <git@dragonmux.network>
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the Software
# is furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
# OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
# OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

CFLAGS := -Wall -Wextra -Werror $(shell pkg-config --cflags libusb-1.0) -g -Og
LDFLAGS := $(shell pkg-config --libs libusb-1.0)

ifeq ($(ASAN), 1)
CFLAGS += -fsanitize=address -Wno-format-truncation
LDFLAGS += -fsanitize=address
endif

OBJS := src/main.o src/stlink.o src/crypto.o tiny-AES-c/aes.o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

stlink-tool: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -f src/*.o
	rm -f stlink-tool
