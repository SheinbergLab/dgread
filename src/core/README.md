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

## Where these files come from

They are a verbatim copy of `dlsh/src/lablib` (the library dlsh, dserv and
stim2 build from).  Do not edit them here; fix upstream and pull:

```bash
scripts/sync-core.sh pull ../dlsh     # copies lablib -> src/core
```

## Usage by Language Bindings

- **Python**: `python/setup.py` compiles `../src/core/*.c` directly.
- **MATLAB**: `matlab/build_dgread.m` compiles `../src/core/*.c` directly.
- **R**: `R CMD build` only sees files inside `R/`, so `R/src/` holds a
  generated copy of `src/core` and `src/lz4`.  Regenerate it after every
  core change, and commit the result:

  ```bash
  scripts/sync-core.sh r        # src/core + src/lz4 -> R/src
  scripts/sync-core.sh check    # exit 1 if R/src has drifted
  ```

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
