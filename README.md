# dgread

Fast readers for dg/dgz dynamic group data files.

[![Python](https://img.shields.io/pypi/v/dgread.svg)](https://pypi.org/project/dgread/)
[![Build](https://github.com/SheinbergLab/dgread/actions/workflows/python_wheels.yml/badge.svg)](https://github.com/SheinbergLab/dgread/actions)

## Overview

`dgread` provides high-performance readers for dg (dynamic group) and dgz (compressed) data files. These files store collections of named arrays with native support for **nested and ragged (variable-length) data** - essential for neuroscience and behavioral research where trials have different numbers of samples.

## Language Support

| Language | Install | Status |
|----------|---------|--------|
| **Python** | `pip install dgread` | [![PyPI](https://img.shields.io/pypi/v/dgread.svg)](https://pypi.org/project/dgread/) |
| **MATLAB** | See [matlab/README.md](matlab/README.md) | MEX files for Windows/macOS/Linux |
| **R** | See [R/README.md](R/README.md) | CRAN package |

## Quick Start

### Python

```python
import dgread

# Load a session - returns dict of numpy arrays
data = dgread.read('session.dgz')

# Access data
print(data.keys())  # ['stimtype', 'response', 'rt', 'em', ...]
print(f"Mean RT: {data['rt'].mean():.1f} ms")

# Ragged data works naturally
for i, trial_em in enumerate(data['em'][:3]):
    print(f"Trial {i}: {len(trial_em)} eye samples")
```

### MATLAB

```matlab
data = dg_read('session.dgz');

% Access as struct fields
mean(data.rt)
length(data.em{1})  % First trial's eye data
```

### R

```r
library(dgread)

data <- read.dgz('session.dgz')
mean(data$rt)
```

## File Formats

### dg (Dynamic Group)
Uncompressed binary format. Fast reads, larger files.

### dgz (Compressed)  
LZ4-compressed format. Good balance of speed and size. Recommended for storage.

## Repository Structure

```
dgread/
├── src/
│   ├── core/       # Shared C code (df.c, dynio.c, etc.)
│   └── lz4/        # LZ4 compression library
├── python/         # Python package (→ PyPI)
├── matlab/         # MATLAB MEX files
├── R/              # R package
└── tests/          # Test data files
```

## Building from Source

See language-specific READMEs:
- [Python](python/README.md)
- [MATLAB](matlab/README.md)
- [R](R/README.md)

## License

MIT License - see [LICENSE](LICENSE)

## Citation

If you use this software in your research, please cite:

```
Sheinberg Lab Data Tools
Brown University
https://github.com/SheinbergLab/dgread
```
