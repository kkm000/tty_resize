# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019 Cy 'kkm' K'Nelson

PREFIX ?= /usr/local
CFLAGS ?= -Os -s -Wall -Wextra
# Add '-static' if you prefer.

.PHONY: all clean dist-clean install

all: tty_resize

tty_resize: tty_resize.c

clean: ; rm -f tty_resize tty_resize.o

dist-clean: clean

install: all ; install tty_resize $(PREFIX)/bin/
