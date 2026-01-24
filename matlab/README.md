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

Or manually:

```matlab
mex -R2018a -v dg_read.cpp ...
    ../src/core/df.c ...
    ../src/core/dfutils.c ...
    ../src/core/dynio.c ...
    ../src/core/flipfuncs.c ...
    ../src/core/lz4utils.c ...
    ../src/lz4/lz4.c ...
    ../src/lz4/lz4hc.c ...
    ../src/lz4/lz4frame.c ...
    ../src/lz4/xxhash.c ...
    -I../src/core -I../src/lz4 ...
    -lz
```

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

On Linux, install zlib development package:
```bash
sudo apt install zlib1g-dev  # Debian/Ubuntu
sudo yum install zlib-devel  # RHEL/CentOS
```

On macOS, zlib is included with Xcode Command Line Tools.
