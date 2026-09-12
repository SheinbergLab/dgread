"""
dgread - reader for dg/dgz dynamic group files.

    import dgread

    data = dgread.read("session.dgz")        # dict: name -> column
    df   = dgread.to_pandas("session.dgz")   # one row per trial; ragged
                                             # columns are arrays in the cell
    arr  = dgread.to_awkward("session.dgz")  # ragged columns as real axes:
                                             # arr.em_x[:, 1:] - arr.em_x[:, :-1]

Column shapes coming out of read()/dgread():
    flat numeric list      -> numpy.ndarray
    list of strings        -> list[str]
    nested list (depth 2)  -> list[numpy.ndarray], one array per row
    nested list (deeper)   -> list[list[...]]

to_pandas keeps the trial table in pandas, where groupby/merge/plotting
live. to_awkward is for the questions that live *inside* a ragged column:
a diff along the sample axis, a first crossing, a per-row reduction with
no Python loop. Both accept a path, a dict from read(), or a list of
either (sessions are concatenated in order).
"""

from ._dgread import dgread, fromString, fromString64
from .convert import (
    read,
    to_pandas,
    to_awkward,
    to_arrow,
    to_parquet,
    row_count,
    is_nested,
    nested_columns,
    scalar_columns,
)

try:
    from importlib.metadata import version as _version

    __version__ = _version("dgread")
except Exception:  # pragma: no cover - source checkout without metadata
    __version__ = "0.0.0"

__all__ = [
    "dgread",
    "fromString",
    "fromString64",
    "read",
    "to_pandas",
    "to_awkward",
    "to_arrow",
    "to_parquet",
    "row_count",
    "is_nested",
    "nested_columns",
    "scalar_columns",
    "__version__",
]
