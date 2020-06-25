LIBS=libpng

INSTALL_DIR=$(HOME)/.local/bin

CFLAGS=-Wall -Wextra -Wpedantic -ansi -pedantic \
	   $(shell for lib in $(LIBS); do pkg-config --cflags $$lib; done) \
	   -march=native

CLIBS=$(shell for lib in $(LIBS); do pkg-config --libs $$lib; done)

OPT=-O3 -s -flto

DEBUG=-Og -g -DDEBUG

CSOURCE=main.c

.PHONY: all
all: sub-cropper debug

debug: $(CSOURCE)
	$(CC) $(CFLAGS) $(CLIBS) $(DEBUG) $^ -o $@

sub-cropper: $(CSOURCE)
	$(CC) $(CFLAGS) $(CLIBS) $(OPT) $^ -o $@

.PHONY: install
install: sub-cropper
	install ./stack-png $(INSTALL_DIR)/stack-png

.PHONY: clean
clean:
	rm sub-cropper debug
