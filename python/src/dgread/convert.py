"""
Converters from the dict that dgread() returns to a pandas DataFrame or an
awkward Array.

Neither pandas nor awkward is imported at module load; each converter
imports its library on first call and raises an ImportError naming the
extra to install (``pip install dgread[pandas]`` / ``dgread[awkward]``).

Row selection
-------------
A dynamic group need not be rectangular: a file can carry a 1-element
``version`` list beside 847-trial columns. Both converters therefore pick
one row count and keep only the columns of that length. The default is
the *most common* column length (ties go to the longer one); pass
``n_rows`` to choose explicitly. Dropped columns are reported with a
``UserWarning`` so nothing disappears silently.
"""

from __future__ import annotations

import warnings
from collections import Counter
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Union

import numpy as np

from ._dgread import dgread as _dgread

PathLike = Union[str, Path]
Source = Union[PathLike, Dict[str, Any]]

__all__ = [
    "read",
    "to_pandas",
    "to_awkward",
    "to_arrow",
    "to_parquet",
    "row_count",
    "is_nested",
    "nested_columns",
    "scalar_columns",
]


# --------------------------------------------------------------------------
# reading
# --------------------------------------------------------------------------

def read(filename: PathLike) -> Dict[str, Any]:
    """Read a .dg/.dgz file into a dict of columns.

    Same as :func:`dgread.dgread` but accepts ``pathlib.Path`` and gives a
    ``FileNotFoundError`` with the path in it instead of a bare read error.
    """
    path = Path(filename)
    if not path.exists():
        raise FileNotFoundError(f"dg file not found: {path}")
    return _dgread(str(path))


def _load(source: Source) -> Dict[str, Any]:
    if isinstance(source, dict):
        return source
    if isinstance(source, (str, Path)):
        return read(source)
    raise TypeError(
        f"expected a path or a dict from dgread.read(), got {type(source).__name__}"
    )


def _sources(source: Union[Source, Sequence[Source]]) -> List[Dict[str, Any]]:
    """Normalise ``source`` to a list of column dicts.

    A single path or dict becomes a one-element list. A list/tuple of paths
    and/or dicts is loaded element by element. A dict is never treated as a
    sequence, so a raw dgread() result passes straight through.
    """
    if isinstance(source, (dict, str, Path)):
        return [_load(source)]
    if isinstance(source, (list, tuple)):
        if not source:
            raise ValueError("no sources given")
        return [_load(s) for s in source]
    raise TypeError(
        f"expected a path, a dict, or a list of those, got {type(source).__name__}"
    )


# --------------------------------------------------------------------------
# column classification
# --------------------------------------------------------------------------

def is_nested(column: Any) -> bool:
    """True if ``column`` is a ragged column: a Python list whose rows are
    arrays or lists rather than scalars/strings.

    dgread() returns flat numeric columns as ndarrays and everything else as
    Python lists; a list of strings is a scalar column for our purposes.
    """
    if isinstance(column, np.ndarray):
        return column.dtype == object and len(column) > 0 and isinstance(
            column[0], (np.ndarray, list)
        )
    if isinstance(column, list):
        return len(column) > 0 and isinstance(column[0], (np.ndarray, list))
    return False


def nested_columns(data: Dict[str, Any]) -> List[str]:
    """Names of the ragged columns in ``data``."""
    return [name for name, col in data.items() if is_nested(col)]


def scalar_columns(data: Dict[str, Any]) -> List[str]:
    """Names of the flat (one value per row) columns in ``data``."""
    return [name for name, col in data.items() if not is_nested(col)]


def row_count(data: Dict[str, Any]) -> int:
    """The row count a converter will use by default: the most common
    column length in ``data``. Ties go to the longer length."""
    lengths = Counter(len(col) for col in data.values())
    if not lengths:
        return 0
    return max(lengths.items(), key=lambda kv: (kv[1], kv[0]))[0]


def _select(
    data: Dict[str, Any],
    columns: Optional[Iterable[str]],
    n_rows: Optional[int],
    nested: str,
) -> Dict[str, Any]:
    """Apply the column/row/nested filters shared by both converters."""
    if nested not in ("keep", "drop"):
        raise ValueError(f"nested must be 'keep' or 'drop', got {nested!r}")

    if columns is None:
        names = list(data)
    else:
        names = list(columns)
        missing = [n for n in names if n not in data]
        if missing:
            raise KeyError(f"columns not in data: {missing}")

    if n_rows is None:
        n_rows = row_count({n: data[n] for n in names}) if names else 0

    kept: Dict[str, Any] = {}
    wrong_len: List[str] = []
    for name in names:
        col = data[name]
        if len(col) != n_rows:
            wrong_len.append(name)
            continue
        if nested == "drop" and is_nested(col):
            continue
        kept[name] = col

    if wrong_len:
        warnings.warn(
            f"dropped {len(wrong_len)} column(s) whose length is not {n_rows}: "
            f"{wrong_len} (pass n_rows= to choose a different row count)",
            UserWarning,
            stacklevel=3,
        )
    return kept


# --------------------------------------------------------------------------
# pandas
# --------------------------------------------------------------------------

def _pandas_column(col: Any, n_rows: int):
    """Build a Series-ready value for one column.

    Ragged columns become a 1-D object array holding one ndarray per row.
    Built explicitly so pandas never tries to stack equal-length rows into
    a 2-D block.
    """
    if is_nested(col):
        out = np.empty(n_rows, dtype=object)
        for i, row in enumerate(col):
            out[i] = row
        return out
    return col


def to_pandas(
    source: Union[Source, Sequence[Source]],
    *,
    columns: Optional[Iterable[str]] = None,
    n_rows: Optional[int] = None,
    nested: str = "keep",
):
    """Convert dg data to a pandas DataFrame, one row per trial.

    Parameters
    ----------
    source
        A path, a dict from :func:`read`, or a list of either. A list is
        loaded in order and concatenated with a fresh index.
    columns
        Subset of columns to keep (default: all).
    n_rows
        Row count to use; columns of any other length are dropped with a
        warning. Default: the most common column length.
    nested
        ``"keep"`` (default) stores each ragged column as an object column
        whose cells are numpy arrays. ``"drop"`` leaves them out.

    Returns
    -------
    pandas.DataFrame

    Notes
    -----
    Per-sample work on a ragged column goes through ``apply`` here. For a
    vectorised version of the same thing see :func:`to_awkward`.
    """
    try:
        import pandas as pd
    except ImportError as e:  # pragma: no cover - exercised only without pandas
        raise ImportError(
            "pandas is required for to_pandas(); install with: pip install dgread[pandas]"
        ) from e

    frames = []
    for data in _sources(source):
        kept = _select(data, columns, n_rows, nested)
        n = len(next(iter(kept.values()))) if kept else 0
        frames.append(
            pd.DataFrame({name: _pandas_column(col, n) for name, col in kept.items()})
        )
    if len(frames) == 1:
        return frames[0]
    return pd.concat(frames, ignore_index=True)


# --------------------------------------------------------------------------
# awkward
# --------------------------------------------------------------------------

def _awkward_column(col: Any, ak):
    """Build an awkward array for one column.

    A ragged column of 1-D ndarrays is assembled from one concatenation and
    an offsets list, which is the fast path and keeps the element dtype.
    Anything deeper or mixed goes through ``ak.from_iter``.
    """
    if isinstance(col, np.ndarray):
        if col.dtype == object:
            return ak.from_iter(list(col))
        return ak.from_numpy(col)

    if not is_nested(col):
        return ak.from_iter(col)  # strings, or a plain python list

    if all(isinstance(row, np.ndarray) and row.ndim == 1 for row in col):
        counts = np.fromiter((len(row) for row in col), dtype=np.int64, count=len(col))
        if counts.sum() == 0:
            content = np.empty(0, dtype=col[0].dtype)
        else:
            content = np.concatenate(col)
        return ak.unflatten(ak.from_numpy(content), counts)

    return ak.from_iter(col)


def to_awkward(
    source: Union[Source, Sequence[Source]],
    *,
    columns: Optional[Iterable[str]] = None,
    n_rows: Optional[int] = None,
    nested: str = "keep",
):
    """Convert dg data to an awkward Array of records, one record per trial.

    Ragged columns become variable-length axes, so numpy-style expressions
    work across every trial at once::

        a = dgread.to_awkward("session.dgz")
        dx = a.em_x[:, 1:] - a.em_x[:, :-1]      # per-sample diff, all trials
        ak.max(abs(dx), axis=1)                   # one value per trial
        ak.firsts(a.t[np.hypot(a.x, a.y) > 2])    # first crossing, None if never

    Parameters
    ----------
    source, columns, n_rows, nested
        As for :func:`to_pandas`. A list of sources is concatenated with
        ``ak.concatenate``.

    Returns
    -------
    awkward.Array
    """
    try:
        import awkward as ak
    except ImportError as e:  # pragma: no cover - exercised only without awkward
        raise ImportError(
            "awkward is required for to_awkward(); install with: pip install dgread[awkward]"
        ) from e

    arrays = []
    for data in _sources(source):
        kept = _select(data, columns, n_rows, nested)
        fields = {name: _awkward_column(col, ak) for name, col in kept.items()}
        arrays.append(ak.Array(fields))
    if len(arrays) == 1:
        return arrays[0]
    return ak.concatenate(arrays)


# --------------------------------------------------------------------------
# arrow / parquet
# --------------------------------------------------------------------------

def _pyarrow():
    try:
        import pyarrow as pa
    except ImportError as e:  # pragma: no cover - exercised only without pyarrow
        raise ImportError(
            "pyarrow is required for to_arrow()/to_parquet(); "
            "install with: pip install dgread[arrow]"
        ) from e
    return pa


def _arrow_column(col: Any, pa):
    """Build a pyarrow array for one column.

    A ragged column of 1-D ndarrays becomes ``list<element dtype>`` from one
    concatenation and an offsets array, keeping int32/float32 as they are.
    Deeper nesting and mixed rows go through pyarrow's own inference, which
    handles lists of lists of arrays (``list<list<...>>``).
    """
    if isinstance(col, np.ndarray) and col.dtype != object:
        return pa.array(col)

    if not is_nested(col):
        return pa.array(list(col))  # strings, or a plain python list

    if all(isinstance(row, np.ndarray) and row.ndim == 1 for row in col):
        counts = np.fromiter((len(row) for row in col), dtype=np.int64, count=len(col))
        offsets = np.zeros(len(col) + 1, dtype=np.int32)
        np.cumsum(counts, out=offsets[1:])
        if counts.sum() == 0:
            content = np.empty(0, dtype=col[0].dtype)
        else:
            content = np.concatenate(col)
        return pa.ListArray.from_arrays(pa.array(offsets), pa.array(content))

    return pa.array([_arrow_row(row) for row in col])


def _arrow_row(row: Any):
    """pyarrow infers types from python lists, not from nested ndarrays."""
    if isinstance(row, np.ndarray):
        return row.tolist()
    if isinstance(row, list):
        return [_arrow_row(r) for r in row]
    return row


def to_arrow(
    source: Union[Source, Sequence[Source]],
    *,
    columns: Optional[Iterable[str]] = None,
    n_rows: Optional[int] = None,
    nested: str = "keep",
):
    """Convert dg data to a pyarrow Table, one row per trial.

    Element types are kept: long -> int32, float -> float32, int64 and
    double as themselves, strings as ``string``, ragged columns as
    ``list<...>``. The table is what the rest of the Arrow ecosystem wants
    as input: ``pyarrow.parquet.write_table``, ``pyarrow.feather``,
    ``polars.from_arrow``, ``duckdb.from_arrow``.

    Parameters
    ----------
    source, columns, n_rows, nested
        As for :func:`to_pandas`. A list of sources is concatenated with
        ``pyarrow.concat_tables``.

    Returns
    -------
    pyarrow.Table
    """
    pa = _pyarrow()
    tables = []
    for data in _sources(source):
        kept = _select(data, columns, n_rows, nested)
        arrays = {name: _arrow_column(col, pa) for name, col in kept.items()}
        tables.append(pa.table(arrays))
    if len(tables) == 1:
        return tables[0]
    return pa.concat_tables(tables, promote_options="default")


def to_parquet(
    source: Union[Source, Sequence[Source]],
    path: PathLike,
    *,
    columns: Optional[Iterable[str]] = None,
    n_rows: Optional[int] = None,
    nested: str = "keep",
    **write_kwargs: Any,
):
    """Write dg data to a Parquet file, one row per trial.

    Parquet is the format every analysis tool opens without any dg-specific
    code (pandas, polars, DuckDB, R's arrow, Spark), so this is the way to
    hand a session to someone who does not have dgread. Nested columns are
    stored as Parquet lists; nothing is flattened or dropped beyond the
    row-count selection described for :func:`to_pandas`.

    Parameters
    ----------
    source, columns, n_rows, nested
        As for :func:`to_pandas`.
    path
        Output file. Created or overwritten.
    **write_kwargs
        Passed to ``pyarrow.parquet.write_table`` (``compression=``,
        ``row_group_size=``, ...). The default compression is snappy.

    Returns
    -------
    pyarrow.Table
        The table that was written.
    """
    _pyarrow()
    import pyarrow.parquet as pq

    table = to_arrow(source, columns=columns, n_rows=n_rows, nested=nested)
    pq.write_table(table, str(path), **write_kwargs)
    return table
