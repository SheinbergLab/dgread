"""Reader error paths: a malformed or newer-format file must raise, never
crash the interpreter or come back half-parsed.

tests/data/unknown_tag.dg is tests/data/two_lists.dg with the LONG_DATA tag
of the first list (byte 46) rewritten to 11, a tag this build does not
define.  Before the fix the core's DF_ABORT status (3) passed an `if (!...)`
check as success and the half-built list segfaulted the converter.
"""

from pathlib import Path

import numpy as np
import pytest

import dgread

DATA = Path(__file__).resolve().parents[2] / "tests" / "data"


def test_plain_dg_reads():
    g = dgread.dgread(str(DATA / "two_lists.dg"))
    assert set(g) == {"a", "b"}
    assert g["a"].dtype == np.int32 and list(g["a"]) == [1, 2, 3]
    assert g["b"].dtype == np.float32 and list(g["b"]) == [1.5, 2.5]


def test_unknown_tag_raises():
    with pytest.raises(ValueError, match="not a valid dg file"):
        dgread.dgread(str(DATA / "unknown_tag.dg"))


def test_unknown_tag_buffer_raises():
    raw = (DATA / "unknown_tag.dg").read_bytes()
    with pytest.raises(ValueError):
        dgread.fromString(raw)


def test_missing_file_raises():
    with pytest.raises(ValueError):
        dgread.dgread(str(DATA / "does_not_exist.dgz"))
