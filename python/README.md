# dgread - Python

Fast reader for dg/dgz dynamic group data files.

## Installation

```bash
pip install dgread
```

For pandas integration:
```bash
pip install dgread[pandas]
```

## Usage

### Basic

```python
import dgread

# Load file - returns dict of numpy arrays
data = dgread.read('session.dgz')

print(data.keys())
# dict_keys(['stimtype', 'response', 'rt', 'em', 'events', ...])

# Scalar arrays
print(data['rt'][:5])
# [342. 289. 456. 312. 378.]

# Ragged arrays (different length per trial)
for i, em in enumerate(data['em'][:3]):
    print(f"Trial {i}: {len(em)} samples")
# Trial 0: 1847 samples
# Trial 1: 923 samples
# Trial 2: 2104 samples
```

### With Pandas

```python
from dgread_utils import load_session, print_summary

# Quick overview
print_summary('session.dgz')
# session.dgz
#   12 lists, 847 trials
#   Rectangular: False
#   Lists:
#     stimtype: 847 x int32
#     response: 847 x int32
#     rt: 847 x float32
#     em: 847 x object (nested)

# Load as DataFrame (scalar columns only)
df = load_session('session.dgz')
print(df.head())
```

### Utility Functions

```python
from dgread_utils import (
    read,               # Read file → dict
    list_names,         # Get column names
    get_lengths,        # Get length of each column
    is_rectangular,     # Check if all same length
    get_scalar_columns, # Non-nested column names
    get_nested_columns, # Nested column names
    to_dataframe,       # Convert to DataFrame
    load_session,       # Read + to_dataframe
    summary,            # File summary as dict
    print_summary,      # Print formatted summary
)
```

## Building from Source

From the repository root:

```bash
cd python
pip install -e .
```

Requirements:
- C compiler
- Python 3.9+
- NumPy
- zlib (system library)

## Running Tests

```bash
cd python
pip install -e .[dev]
DG_TEST_FILE=/path/to/test.dgz pytest
```
