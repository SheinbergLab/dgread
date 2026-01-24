# LZ4 Compression Library

This directory contains the LZ4 compression library sources.

LZ4 is used for dgz (compressed dynamic group) file support.

## Source

LZ4 by Yann Collet: https://github.com/lz4/lz4

## Files (current as of LZ4 v1.10.x)

- `lz4.c` / `lz4.h` - Core LZ4 compression/decompression
- `lz4hc.c` / `lz4hc.h` - High compression mode (includes optimal parser)
- `lz4frame.c` / `lz4frame.h` - Frame format support
- `xxhash.c` / `xxhash.h` - Hash function used by LZ4 frame format

Note: `lz4opt.h` was merged into `lz4hc.c` in recent versions.
Note: `lz4frame_static.h` is now just a wrapper - the content is in `lz4frame.h` 
      behind `#ifdef LZ4F_STATIC_LINKING_ONLY`.

## License

LZ4 is BSD-2-Clause licensed. See https://github.com/lz4/lz4/blob/dev/LICENSE
