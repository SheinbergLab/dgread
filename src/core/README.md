# Core C Sources

This directory contains the shared C code used by all language bindings.

## Files

### Core dg/dgz functionality
- `df.c` / `df.h` - Dynamic file I/O, main read/write functions
- `dfutils.c` - Utility functions for df operations  
- `dynio.c` / `dynio.h` - Dynamic list I/O, serialization
- `flipfuncs.c` / `flipfuncs.h` - Byte order handling (endianness)
- `lz4utils.c` - LZ4 compression integration

### Utility headers
- `utilc.h` - Common utility macros and definitions

## Usage by Language Bindings

Each binding includes these sources in its build:

- **Python**: `python/setup.py` references `../src/core/*.c`
- **MATLAB**: MEX compilation includes these sources
- **R**: `R/src/Makevars` includes these sources

## Modifying Core Code

When you fix a bug or add a feature here, all language bindings benefit.
Run tests for all languages after changes:

```bash
# Python
cd python && pip install -e . && pytest

# MATLAB  
cd matlab && matlab -batch "run('test_dgread.m')"

# R
cd R && R CMD check dgread
```
