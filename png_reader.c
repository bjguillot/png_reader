/*	PNG Header Reader
*	by B.J. Guillot (bguillot@acm.org)
*	Copyright (C) 2016
*	All Rights Reserved
*	Released under MIT License
*	Version 1.0
*	2016-06-30
*/

/*
*	This is a simple command-line tool that verifies the structural integrity of a PNG graphics file.
*
*	Operations Performed:
*		1. Verifies correct 8-byte PNG file header.
*		2. Verify structural integrity of each chunk (checks chunk length and chunk CRC-32).
*		3. Displays width, height, bit depth, color type, etc. of IHDR chunk for debugging purposes.
*		4. Verifies FCHECK checksum in IDAT chunk.
*		5. Verifies ADLER-32 checksum in IDAT chunk (currently, for uncompressed data only).
*		6. Displays CMF, FLG, FCHECK, FDICT, FLEVEL values in IDAT chunk for debugging purposes.
*		7. Stops parsing file when it encounters a valid IEND chunk.
*
*	Compiled and tested on Mac OS X El Capitan 10.11.5.
*
*	TODO: Does not currently do an integrity check on Huffman encoding for compressed blocks in IDAT chunk.
*/

#include <stdio.h>	//for printf, etc.
#include <stdint.h>	//for uint32_t, etc.
#include <stdlib.h>	//for malloc,free
#include <string.h>	//for memcpy
#include "png_utils.h"	//for crc32, adler32, swap_uint32

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("\a!!! ERROR !!! Please supply filename on the command line as first argument\n\n");
		return 1;
	}
	printf("Input=[%s]\n", argv[1]);

	FILE *pngfile;
	pngfile = fopen(argv[1], "rb");
	if (!pngfile) {
		printf("\a!!! ERROR !!! Can't open file!\n\n");
		return 1;
	}

	unsigned char header_expected[] = {137, 80, 78, 71, 13, 10, 26, 10};
	unsigned char header_file[8] = {0};
	int status = fread(header_file, 8, 1, pngfile);
	for (int i = 0; i < 8; i++) {
		if (header_file[i] != header_expected[i]) {
			printf("\a!!! ERROR !!! Header Byte %d Mismatch; Expected=%d, but Found=%d\n\n", i+1, 
				header_expected[i], header_file[i]);
		}
	}

	ihdr_t ihdr;
	unsigned char chunk_type[5] = {0};
	do {
		uint32_t length = 0;
		status = fread(&length, 4, 1, pngfile);
		if (status <= 0) {
			printf("\a!!! ERROR !!! Premature end of file encountered\n");
			return 1;
		}
		length = swap_uint32(length);

		unsigned char* buffer = (unsigned char*) malloc(length + 4);

		status = fread(buffer, 4+length, 1, pngfile);
		memcpy(chunk_type, buffer, 4);

		uint32_t crc_computed = crc32(buffer, 4+length);
		uint32_t crc_file = 0;
		status = fread(&crc_file, 4, 1, pngfile);
		crc_file = swap_uint32(crc_file);

		int ancillary = (buffer[0] & 32) >> 5;
		int private_bit = (buffer[1] & 32) >> 5;
		int reserved = (buffer[2] & 32) >> 5;
		int safe_to_copy = (buffer[3] & 32) >> 5;

		printf("Chunk=%s  Ancillary=%d Private=%d Reserved=%d SafeToCopy=%d  Length=%u  FileCRC=%u  ComputedCRC=%u\n",
			chunk_type, ancillary, private_bit, reserved, safe_to_copy, length, crc_file, crc_computed);
		if (crc_file != crc_computed) {
			printf("\a!!! ERROR !!! CRC MISMATCH\n\n");
		}

		if (strcmp((const char*)chunk_type, "IHDR") == 0) {
			memcpy(&ihdr, buffer+4, length);

			ihdr.width = swap_uint32(ihdr.width);
			ihdr.height = swap_uint32(ihdr.height);
			printf("\t width=%d\n", ihdr.width);
			printf("\t height=%d\n", ihdr.height);
			printf("\t bit_depth=%d\n", ihdr.bit_depth);
			printf("\t color_type=%d\n", ihdr.color_type);
			printf("\t comp_method=%d\n", ihdr.comp_method);
			// compression method 0 (deflate/inflate compression with a sliding window of at most 32768 bytes) is defined
			printf("\t filter_method=%d\n", ihdr.filter_method);
			printf("\t interlace_method=%d\n", ihdr.interlace_method);

		} else if (strcmp((const char*)chunk_type, "IDAT") == 0) {
			unsigned char cmf = buffer[4];
			unsigned char flg = buffer[5];
			unsigned char block_format = buffer[6]; 

			printf("\t CMF=%d\n", cmf);

			int cm = cmf & 15;		// CMF bits 0,1,2,3
			int ci = (cmf & 240) >> 4;	// CMF bits 4,5,6,7

			printf("\t\t Compression Method=%d  (should always be 8 for PNG; 8=deflate)\n", cm);
			printf("\t\t Compression Info=%d  (7=32K window size)\n", ci);

			printf("\t FLG=%d\n", flg);

			int fcheck = flg & 31;		// FLG bits 0,1,2,3,4
			int fdict = (flg & 32) >> 5;	// FLG bits 5
			int flevel = (flg & 129) >> 6;	// FLG bits 6,7

			printf("\t\t FCHECK=%d  (check bits for CMF and FLG)\n", fcheck);

			int fcheck_calc = (cmf*256 + flg) % 31;
			if (fcheck_calc != 0) {
				printf("\a!!! ERROR !!! FCHECK checksum mismatch, not multiple of 31\n\n");
			}

			printf("\t\t FDICT=%d  (0=no preset dictionary)\n", fdict);
			printf("\t\t FLEVEL=%d  (2=use default algorithm)\n", flevel);

			printf("\t Block Format: First Byte=%d   First 3-bits that matter=%d\n", block_format, block_format & 7);

			int bfinal = block_format & 1;		// Bits 0
			int btype = (block_format & 6) >> 1;	// Bits 1,2

			printf("\t\t BFINAL=%d  (0=more blocks follow; 1=final block)\n", bfinal);
			printf("\t\t BTYPE=%d  (0=no compression; 1=fixed Huffman; 2=dynamic Huffman; 3=error)\n", btype);

			if (btype == 0) { // no compression
				uint16_t len = buffer[7] + buffer[8] * 256;
				uint16_t nlen = buffer[9] + buffer[10] * 256;

				printf("\t\t LEN=%u\n", len);
				printf("\t\t NLEN=%u\n", nlen);

				uint16_t ones_complement = ~len;

				if (ones_complement != nlen) {
					printf("\a!!! ERROR !!! One's complement of LEN (%u) is not equal to NLEN (%u)\n\n", ones_complement, nlen); 
				}

				int expected_len = ihdr.width;
				switch (ihdr.color_type) {
					case 2: expected_len *= 3; break; // RGB triple
					case 4: expected_len *= 2; break; // grayscale + alpha
					case 6: expected_len *= 4; break; // RGB triple + alpha
					default: break;
				}

				int need_filler = 0;
				switch (ihdr.bit_depth) {
					case 1:
						if ((expected_len % 8) != 0)
							need_filler = 1;
						expected_len = expected_len >> 3; // need 8x fewer bytes for 1-bit depth 
						break;
					case 2: 
						if ((expected_len % 4) != 0)
							need_filler = 1;
						expected_len = expected_len >> 2;
						break;
					case 4: 
						if ((expected_len % 2) != 0)
							need_filler = 1;
						expected_len = expected_len >> 1;
						break;
					case 16: expected_len *= 2; break;
					default: break;
				}

				expected_len += need_filler;
				expected_len++; // add one byte for each row for filtering
				expected_len *= ihdr.height;

				if (len != expected_len) {
					printf("\a!!! ERROR !!! Mismatch with uncompressed data length (%u) and expected length (%u)\n\n", len, expected_len);
				}

				// The Adler-32 checksum is at the end of the block, but it is the checksum for
				// uncompressed data.  We would have to inflate the data to check this if the
				// BTYPE is anything other than 0 ("no compression").

				uint32_t adler32_file = 0;
				memcpy(&adler32_file, buffer + length, 4);
				adler32_file = swap_uint32(adler32_file);
				uint32_t adler32_computed = adler32(&buffer[11], len);
				printf("\t\t FileAdler32=%u  ComputedAdler32=%u\n", adler32_file, adler32_computed);

				if (adler32_file != adler32_computed) {
					printf("\a!!! ERROR !!! ADLER-32 MISMATCH\n\n");
				}

			}

		}

		free(buffer);

	} while (strcmp((const char*)chunk_type, "IEND") != 0);

}
