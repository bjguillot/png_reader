CC = cc
CFLAGS = -std=c99 -pedantic -Wall

all: png_reader

png_reader: png_reader.c png_utils.c
	$(CC) $(CFLAGS) -o png_reader png_reader.c png_utils.c
