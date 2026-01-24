# Migration Guide: Old Structure → Clean Monorepo

This document describes how to migrate your existing dgread repository to the new clean structure.

## New Structure

```
dgread/
├── src/
│   ├── core/           # Shared C sources (ONE copy!)
│   │   ├── df.c
│   │   ├── df.h
│   │   ├── dfutils.c
│   │   ├── dynio.c
│   │   ├── dynio.h
│   │   ├── flipfuncs.c
│   │   ├── flipfuncs.h
│   │   ├── lz4utils.c
│   │   └── utilc.h
│   └── lz4/            # LZ4 library sources
│       ├── lz4.c
│       ├── lz4.h
│       ├── lz4hc.c
│       ├── lz4hc.h
│       ├── lz4frame.c
│       ├── lz4frame.h
│       ├── lz4frame_static.h
│       ├── lz4opt.h
│       ├── xxhash.c
│       └── xxhash.h
├── python/
│   ├── dgread.c        # Python-specific wrapper only
│   ├── setup.py
│   ├── pyproject.toml
│   ├── src/
│   │   └── dgread_utils.py
│   └── README.md
├── matlab/
│   ├── dg_read.c       # MATLAB-specific wrapper only
│   ├── build_dgread.m
│   └── README.md
├── R/
│   ├── src/
│   │   ├── dgread.c    # R-specific wrapper only
│   │   └── Makevars
│   ├── R/
│   │   └── dgread.R
│   ├── DESCRIPTION
│   └── README.md
├── tests/
│   └── data/           # Shared test files
│       └── *.dgz
├── .github/
│   └── workflows/
│       └── python_wheels.yml
├── .gitignore
├── LICENSE
└── README.md
```

## Migration Steps

### 1. Create the new directory structure

```bash
mkdir -p dgread-new/{src/core,src/lz4,python/src,matlab,R/src,R/R,R/man,tests/data,.github/workflows}
```

### 2. Copy shared C sources (ONE TIME, from your best/newest copy)

```bash
# From your existing c/src/ directory:
cp c/src/df.c c/src/df.h dgread-new/src/core/
cp c/src/dfutils.c dgread-new/src/core/
cp c/src/dynio.c c/src/dynio.h dgread-new/src/core/
cp c/src/flipfuncs.c c/src/flipfuncs.h dgread-new/src/core/
cp c/src/lz4utils.c dgread-new/src/core/

# From your existing python/dgread/ for LZ4:
cp python/dgread/lz4.c python/dgread/lz4.h dgread-new/src/lz4/
cp python/dgread/lz4hc.c python/dgread/lz4hc.h dgread-new/src/lz4/
cp python/dgread/lz4frame.c python/dgread/lz4frame.h dgread-new/src/lz4/
cp python/dgread/lz4frame_static.h python/dgread/lz4opt.h dgread-new/src/lz4/
cp python/dgread/xxhash.c python/dgread/xxhash.h dgread-new/src/lz4/
```

### 3. Copy language-specific wrappers

```bash
# Python wrapper
cp python/dgread/dgread.c dgread-new/python/

# MATLAB wrapper  
cp MATLAB/dg_read/dg_read.c dgread-new/matlab/

# R wrapper
cp R/dgread/src/dgread.c dgread-new/R/src/

# R package files
cp R/dgread/R/dgread.R dgread-new/R/R/
cp R/dgread/man/*.Rd dgread-new/R/man/
```

### 4. Copy test data

```bash
cp c/data/*.dgz dgread-new/tests/data/
```

### 5. Copy new config files (from the template)

The template provides:
- `python/pyproject.toml`
- `python/setup.py`
- `matlab/build_dgread.m`
- `R/DESCRIPTION`
- `R/src/Makevars`
- `.github/workflows/python_wheels.yml`
- `.gitignore`
- All README files

### 6. Verify build works

```bash
# Python
cd dgread-new/python
pip install -e .
python -c "import dgread; print('OK')"

# MATLAB
cd dgread-new/matlab
matlab -batch "build_dgread"

# R
cd dgread-new/R
R CMD build .
R CMD check dgread_*.tar.gz
```

### 7. Delete old cruft

The following can be deleted from the old repo:
- `python/dgread/build/` (all build artifacts)
- `python/dgread/dist/` (old distributions)
- `python/dgread/old/`
- `python/dgread/old_setups/`
- `python/dgread/*.egg-info/`
- `MATLAB/dg_read/*.mex*` (pre-built binaries - CI builds these now)
- All duplicate C source files in Python/MATLAB/R directories
- `setup.cfg` (merged into pyproject.toml)

## Key Differences

| Aspect | Old | New |
|--------|-----|-----|
| C sources | Duplicated in each language dir | Single copy in `src/` |
| Python build | Manual, platform-specific setup.py files | CI builds wheels for all platforms |
| Distribution | Manual upload | GitHub Actions → PyPI on release |
| Test data | Scattered | Centralized in `tests/data/` |

## Troubleshooting

### "Can't find df.h"

Make sure the include paths in setup.py/Makevars point to `../src/core/`.

### Python sdist doesn't include C sources

The GitHub Actions workflow handles this by copying sources into the sdist build directory.

### MATLAB can't find zlib

On Windows, you may need to include the bundled `z.lib`. Update `build_dgread.m` to check for it.
