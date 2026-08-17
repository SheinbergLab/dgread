# dgread - MATLAB

MEX reader for dg/dgz dynamic group data files.

## Installation

### Pre-built MEX Files

Download the appropriate MEX file for your platform from the [Releases](https://github.com/SheinbergLab/dgread/releases) page:

| Platform | File |
|----------|------|
| Windows 64-bit | `dg_read.mexw64` |
| macOS Intel | `dg_read.mexmaci64` |
| macOS Apple Silicon | `dg_read.mexmaca64` |
| Linux 64-bit | `dg_read.mexa64` |

Place the file in your MATLAB path.

### Building from Source

From MATLAB, run:

```matlab
cd matlab
build_dgread
```

`build_dgread` compiles in two steps, and both are necessary:

1. The C sources (`../src/core`, `../src/lz4`, and the vendored `../src/zlib`)
   are compiled to object files with the **C** compiler.
2. `dg_read.cpp` is compiled and linked against those objects with the **C++**
   compiler, using the R2018a MEX API.

Passing the `.c` files and `dg_read.cpp` to a single `mex` call does *not*
work: `mex` selects one compiler for the whole source list, so the C core gets
fed to `clang++`/`cl` as C++ and fails to compile (for example, `dfutils.c`
uses `new` as a variable name, and C's implicit `void *` conversions are
errors in C++).

zlib is vendored in `../src/zlib` and compiled in, so no system zlib is
required on any platform.

## Usage

```matlab
% Load a dgz file - returns struct
data = dg_read('session.dgz');

% Access fields directly
mean(data.rt)
std(data.response)

% Ragged data returned as cell arrays
for i = 1:3
    fprintf('Trial %d: %d samples\n', i, length(data.em{i}));
end
```

## Data Types

| dg Type | MATLAB Type |
|---------|-------------|
| Integer | double |
| Float | double |
| String | char/cell |
| List (nested) | cell array |

## Functions

### dg_read

```matlab
data = dg_read(filename)
```

Reads a dg or dgz file and returns a struct with fields corresponding to the list names in the file.

**Parameters:**
- `filename` - Path to dg or dgz file

**Returns:**
- `data` - Struct with named fields containing arrays

## Troubleshooting

### "Invalid MEX file" error

Make sure you're using the correct MEX file for your platform and MATLAB version.

### Missing zlib

Not applicable since 1.1.6: zlib is vendored in `../src/zlib` and compiled
directly into the MEX file, so no system or vcpkg zlib is needed.
