"""dgread.to_arrow / dgread.to_parquet and the dg2parquet entry point.

Skipped entirely when pyarrow is not installed (it is an optional extra).
"""

from pathlib import Path

import numpy as np
import pytest

pa = pytest.importorskip("pyarrow")
pq = pytest.importorskip("pyarrow.parquet")

import dgread
from dgread.cli import dg2parquet

DATA = Path(__file__).resolve().parents[2] / "tests" / "data"
RAGGED = DATA / "j_cue_saccade_052209009.dgz"
WIDE = DATA / "wide_types.dgz"


def synthetic():
    return {
        "trial": np.arange(4, dtype=np.int32),
        "rt": np.array([10.0, 20.0, 30.0, 40.0], dtype=np.float32),
        "ts": np.array([1757606400123456, -1, 0, 2**53 + 1], dtype=np.int64),
        "x": np.array([0.1, 1e300, -0.5, 3.141592653589793], dtype=np.float64),
        "name": ["a", "b", "c", "d"],
        "samples": [
            np.array([1.0, 2.0, 4.0], dtype=np.float32),
            np.array([], dtype=np.float32),
            np.array([5.0], dtype=np.float32),
            np.array([0.0, 1.0], dtype=np.float32),
        ],
        "deep": [[np.array([1, 2])], [], [np.array([3]), np.array([4, 5])], []],
    }


def test_to_arrow_keeps_types():
    t = dgread.to_arrow(synthetic())
    assert t.num_rows == 4
    s = t.schema
    assert s.field("trial").type == pa.int32()
    assert s.field("rt").type == pa.float32()
    assert s.field("ts").type == pa.int64()
    assert s.field("x").type == pa.float64()
    assert s.field("name").type == pa.string()
    assert s.field("samples").type == pa.list_(pa.float32())
    assert pa.types.is_list(s.field("deep").type)
    assert pa.types.is_list(s.field("deep").type.value_type)


def test_to_arrow_values_exact():
    t = dgread.to_arrow(synthetic())
    assert t.column("ts").to_pylist() == [1757606400123456, -1, 0, 2**53 + 1]
    assert t.column("x").to_pylist()[3] == 3.141592653589793
    assert t.column("samples").to_pylist() == [[1.0, 2.0, 4.0], [], [5.0], [0.0, 1.0]]
    assert t.column("deep").to_pylist() == [[[1, 2]], [], [[3], [4, 5]], []]
    assert t.column("name").to_pylist() == ["a", "b", "c", "d"]


def test_to_arrow_all_empty_ragged_keeps_dtype():
    data = {"a": np.arange(2, dtype=np.int32),
            "s": [np.array([], dtype=np.float32), np.array([], dtype=np.float32)]}
    t = dgread.to_arrow(data)
    assert t.schema.field("s").type == pa.list_(pa.float32())
    assert t.column("s").to_pylist() == [[], []]


def test_to_arrow_real_wide_file():
    t = dgread.to_arrow(WIDE, n_rows=3)
    assert t.schema.field("ts").type == pa.int64()
    assert t.schema.field("x").type == pa.float64()
    assert t.column("ts").to_pylist()[0] == 1757606400123456


def test_to_arrow_ragged_file_selects_rows():
    with pytest.warns(UserWarning, match="dropped"):
        t = dgread.to_arrow(RAGGED)
    assert t.num_rows == dgread.row_count(dgread.read(RAGGED))
    assert t.num_columns > 10


def test_to_arrow_concatenates_sources():
    t = dgread.to_arrow([synthetic(), synthetic()])
    assert t.num_rows == 8


def test_to_parquet_round_trip(tmp_path):
    out = tmp_path / "s.parquet"
    written = dgread.to_parquet(synthetic(), out, compression="zstd")
    assert out.exists()
    back = pq.read_table(out)
    assert back.equals(written)
    assert back.column("ts").to_pylist()[3] == 2**53 + 1


def test_dg2parquet_default_name(tmp_path, capsys):
    src = tmp_path / "wide_types.dgz"
    src.write_bytes(WIDE.read_bytes())
    rc = dg2parquet([str(src), "--n-rows", "3", "-q"])
    assert rc == 0
    out = tmp_path / "wide_types.parquet"
    assert out.exists()
    assert pq.read_table(out).num_rows == 3


def test_dg2parquet_reports(tmp_path, capsys):
    out = tmp_path / "r.parquet"
    rc = dg2parquet([str(RAGGED), "-o", str(out)])
    assert rc == 0
    captured = capsys.readouterr()
    assert "rows x" in captured.out
    assert "dropped" in captured.err
    assert pq.read_table(out).num_rows > 0


def test_dg2parquet_needs_output_for_many(tmp_path):
    with pytest.raises(SystemExit):
        dg2parquet([str(WIDE), str(WIDE)])
