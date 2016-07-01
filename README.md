# png_reader
Simple command-line tool that verifies the structural integrity of a PNG graphics file.

Operations Performed:
  1. Verifies correct 8-byte PNG file header.
  2. Verify structural integrity of each chunk (checks chunk length and chunk CRC-32).
  3. Displays width, height, bit depth, color type, etc. of IHDR chunk for debugging purposes.
  4. Verifies FCHECK checksum in IDAT chunk.
  5. Verifies ADLER-32 checksum in IDAT chunk (currently, for uncompressed data only).
  6. Displays CMF, FLG, FCHECK, FDICT, FLEVEL values in IDAT chunk for debugging purposes.
  7. Stops parsing file when it encounters a valid IEND chunk.

Compiled and tested on Mac OS X El Capitan 10.11.5.

TODO: Does not currently do an integrity check on Huffman encoding for compressed blocks in IDAT chunk.
