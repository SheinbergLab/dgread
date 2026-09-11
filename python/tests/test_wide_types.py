"""The 8-byte element types: DF_INT64 (dg tag 11) and DF_DOUBLE (tag 12).

tests/data/wide_types.dgz was written by dlsh with
    ts     = dl_wlist 1757606400123456 -9007199254740993 42
    x      = dl_dlist 3.141592653589793 1e300 -0.5
    i      = dl_ilist 1 2 3
    nested = dl_llist [dl_wlist 5000000000] [dl_dlist 0.1 0.2]
The int64 values are chosen so a float32 or int32 (or even a double, for
-2^53-1) could not have carried them.
"""

from pathlib import Path

import numpy as np
import pytest

import dgread

DATA = Path(__file__).resolve().parents[2] / "tests" / "data"


@pytest.mark.parametrize("name", ["wide_types.dgz", "wide_types.dg"])
def test_wide_columns(name):
    g = dgread.dgread(str(DATA / name))
    assert g["ts"].dtype == np.int64
    assert list(g["ts"]) == [1757606400123456, -9007199254740993, 42]
    assert g["x"].dtype == np.float64
    assert list(g["x"]) == [3.141592653589793, 1e300, -0.5]
    assert g["i"].dtype == np.int32 and list(g["i"]) == [1, 2, 3]


def test_wide_nested():
    g = dgread.dgread(str(DATA / "wide_types.dgz"))
    rows = g["nested"]
    assert rows[0].dtype == np.int64 and list(rows[0]) == [5000000000]
    assert rows[1].dtype == np.float64 and list(rows[1]) == [0.1, 0.2]


def test_wide_via_buffer():
    raw = (DATA / "wide_types.dg").read_bytes()
    g = dgread.fromString(raw)
    assert list(g["ts"]) == [1757606400123456, -9007199254740993, 42]
