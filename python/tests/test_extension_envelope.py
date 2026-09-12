"""The skippable extension envelope, dg tag 250.

tests/data/ext_envelope.dg carries an envelope at every level of the file:
a 17-byte payload at the top level (id 7), an empty one inside the group
(id 8), one before the data of list `a` (id 9) and one after the data of
list `b` (id 1).  No reader knows any of those ids, and none needs to: the
record carries its own length, so the file must read exactly as if the
envelopes were not there.  (Contrast unknown_tag.dg: a bare unknown tag
still aborts, which is the whole reason the envelope exists.)

The .dg copy exercises the file parser, the .dgz copy the buffer parser.
"""

from pathlib import Path

import numpy as np
import pytest

import dgread

DATA = Path(__file__).resolve().parents[2] / "tests" / "data"


def _check(g):
    assert list(g) == ["a", "b"]
    assert g["a"].dtype == np.int32 and list(g["a"]) == [1, 2, 3]
    assert g["b"].dtype == np.float64 and list(g["b"]) == [1.5, 2.5]


def test_envelopes_are_skipped_file_parser():
    _check(dgread.dgread(str(DATA / "ext_envelope.dg")))


def test_envelopes_are_skipped_buffer_parser():
    _check(dgread.dgread(str(DATA / "ext_envelope.dgz")))


def test_envelopes_are_skipped_from_bytes():
    _check(dgread.fromString((DATA / "ext_envelope.dg").read_bytes()))


def test_oversize_envelope_raises():
    raw = bytearray((DATA / "ext_envelope.dg").read_bytes())
    # magic(4) version tag(1) float(4) ext tag(1) id(4) -> length field
    off = 4 + 1 + 4 + 1 + 4
    raw[off:off + 4] = (100_000_000).to_bytes(4, "little", signed=True)
    with pytest.raises(ValueError):
        dgread.fromString(bytes(raw))


def test_negative_envelope_raises():
    raw = bytearray((DATA / "ext_envelope.dg").read_bytes())
    off = 4 + 1 + 4 + 1 + 4
    raw[off:off + 4] = (-1).to_bytes(4, "little", signed=True)
    with pytest.raises(ValueError):
        dgread.fromString(bytes(raw))
