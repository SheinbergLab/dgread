# dgread - Python

Fast reader for dg/dgz dynamic group data files.

## Installation

```bash
pip install dgread
```

Optional extras for the converters:

```bash
pip install "dgread[pandas]"    # dgread.to_pandas
pip install "dgread[awkward]"   # dgread.to_awkward
```

## Usage

### Basic

```python
import dgread

# Load file - returns a dict of columns
data = dgread.read('session.dgz')

print(data.keys())
# dict_keys(['stimtype', 'response', 'rt', 'em', 'events', ...])

# Flat numeric columns are numpy arrays
print(data['rt'][:5])
# [342. 289. 456. 312. 378.]

# Ragged columns are Python lists with one numpy array per row
for i, em in enumerate(data['em'][:3]):
    print(f"Trial {i}: {len(em)} samples")
# Trial 0: 1847 samples
# Trial 1: 923 samples
# Trial 2: 2104 samples
```

What each column kind comes back as:

| in the file              | in Python                      |
|--------------------------|--------------------------------|
| flat numeric list        | `numpy.ndarray`                |
| list of strings          | `list[str]`                    |
| nested list (one level)  | `list[numpy.ndarray]`          |
| nested deeper            | `list[list[...]]`              |

### pandas: one row per trial

```python
df = dgread.to_pandas('session.dgz')

# Several sessions, concatenated in order
df = dgread.to_pandas(['s1.dgz', 's2.dgz', 's3.dgz'])

df.groupby('stimtype')['rt'].median()

# A ragged column is an object column whose cells are numpy arrays.
# Per-sample work goes through apply:
df['n_samples'] = df['em'].apply(len)
df['peak_v'] = df['em'].apply(lambda em: abs(np.diff(em)).max())
```

Options: `columns=[...]` to keep a subset, `nested="drop"` to leave the
ragged columns out, `n_rows=` to choose the row count explicitly (see
below).

### awkward: ragged columns as real axes

When the question lives *inside* a ragged column, [Awkward
Array](https://awkward-array.org) does the per-sample work across every
trial at once, in numpy spelling, with no loop:

```python
import awkward as ak
import numpy as np

a = dgread.to_awkward('session.dgz')       # or a list of files

dx = a.em_x[:, 1:] - a.em_x[:, :-1]        # per-sample diff, all trials
peak = ak.max(np.hypot(dx, dy), axis=1)    # one value per trial

ecc = np.hypot(a.em_x, a.em_y)
t_cross = ak.firsts(a.t[ecc > 2])          # first crossing; None if never

correct = a[a.correct == 1]                # trial masks work on records
```

Compute the per-trial scalar in awkward, then hand it back to pandas for
the grouping and plotting:

```python
df['peak_v'] = ak.to_numpy(peak)
```

### Non-rectangular groups

A dynamic group need not be rectangular: a file can carry a 1-element
`version` list, per-trial columns of length 96, and per-observation
event columns of length 97, side by side. Both converters keep the
columns of one length and drop the rest with a `UserWarning` naming
them. The default is the most common length; pass `n_rows=97` to pick
the other table.

```python
trials = dgread.to_pandas('cells.dgz')             # 96 rows, warns
events = dgread.to_pandas('cells.dgz', n_rows=97)  # 97 rows, warns
```

Helpers: `dgread.row_count(data)`, `dgread.nested_columns(data)`,
`dgread.scalar_columns(data)`, `dgread.is_nested(column)`.

### Compatibility

`dgread_utils` (`load_session`, `to_dataframe`, `print_summary`, ...)
still imports and now delegates to the package. New code should use
`dgread.to_pandas` / `dgread.to_awkward` directly.

The C extension is `dgread._dgread`; `dgread.dgread`, `dgread.fromString`
and `dgread.fromString64` are unchanged.

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

zlib and lz4 are vendored under `../src` and compiled in.

## Running Tests

```bash
cd python
pip install -e ".[dev,pandas,awkward]"
pytest
```

The tests use the fixtures in `../tests/data`. The pandas and awkward
tests skip themselves if the extra is not installed. The same test suite
runs inside `cibuildwheel` against every built wheel.
