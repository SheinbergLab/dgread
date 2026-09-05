"""
dgread_utils - compatibility layer over the dgread package.

Everything here now lives in ``dgread`` itself::

    import dgread
    df  = dgread.to_pandas("session.dgz")
    arr = dgread.to_awkward("session.dgz")

This module keeps the old names importable. The earlier implementation
assumed every column was an ndarray and crashed on the Python lists that
dgread() returns for strings and ragged data; these versions delegate to
the package and work on real files.
"""

from pathlib import Path
from typing import Any, Dict, List, Optional, Union

import numpy as np

import dgread as _dg
from dgread import read, is_nested, nested_columns, scalar_columns, row_count

__all__ = [
    "read",
    "list_names",
    "get_lengths",
    "is_rectangular",
    "get_scalar_columns",
    "get_nested_columns",
    "to_dataframe",
    "load_session",
    "summary",
    "print_summary",
]


def list_names(filename: Union[str, Path]) -> List[str]:
    """Column names in a dg/dgz file."""
    return list(read(filename).keys())


def get_lengths(data: Dict[str, Any]) -> Dict[str, int]:
    """Length of each column."""
    return {name: len(col) for name, col in data.items()}


def is_rectangular(data: Dict[str, Any]) -> bool:
    """True if every column has the same length."""
    return len(set(get_lengths(data).values())) <= 1


def get_scalar_columns(data: Dict[str, Any]) -> List[str]:
    """Names of flat (one value per row) columns."""
    return scalar_columns(data)


def get_nested_columns(data: Dict[str, Any]) -> List[str]:
    """Names of ragged columns."""
    return nested_columns(data)


def to_dataframe(
    data: Dict[str, Any],
    columns: Optional[List[str]] = None,
    include_nested: bool = False,
):
    """Convert dg data to a DataFrame. See :func:`dgread.to_pandas`.

    Kept for compatibility: ``include_nested=False`` (the old default)
    drops ragged columns, the opposite of ``dgread.to_pandas``.
    """
    return _dg.to_pandas(
        data, columns=columns, nested="keep" if include_nested else "drop"
    )


def load_session(
    filename: Union[str, Path],
    columns: Optional[List[str]] = None,
    include_nested: bool = False,
):
    """Read a file straight into a DataFrame. See :func:`dgread.to_pandas`."""
    return to_dataframe(read(filename), columns=columns, include_nested=include_nested)


def _dtype_name(col: Any) -> str:
    if isinstance(col, np.ndarray):
        return str(col.dtype)
    if is_nested(col):
        first = col[0]
        inner = str(first.dtype) if isinstance(first, np.ndarray) else "list"
        return f"list[{inner}]"
    if len(col) and isinstance(col[0], str):
        return "str"
    return "object"


def summary(filename: Union[str, Path]) -> Dict[str, Any]:
    """Summary of a dg/dgz file: column names, lengths, dtypes, nesting."""
    path = Path(filename)
    data = read(path)
    lists_info = {
        name: {"length": len(col), "dtype": _dtype_name(col), "nested": is_nested(col)}
        for name, col in data.items()
    }
    return {
        "filename": path.name,
        "n_lists": len(data),
        "n_trials": row_count(data),
        "rectangular": is_rectangular(data),
        "lists": lists_info,
    }


def print_summary(filename: Union[str, Path]) -> None:
    """Print a formatted summary of a dg/dgz file."""
    info = summary(filename)
    print(f"\n{info['filename']}")
    print(f"  {info['n_lists']} lists, {info['n_trials']} trials")
    print(f"  Rectangular: {info['rectangular']}")
    print("\n  Lists:")
    for name, linfo in info["lists"].items():
        nested_str = " (nested)" if linfo["nested"] else ""
        print(f"    {name}: {linfo['length']} x {linfo['dtype']}{nested_str}")
