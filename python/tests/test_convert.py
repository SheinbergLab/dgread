"""Tests for dgread.to_pandas / dgread.to_awkward and the package layout.

Runs against the fixtures in <repo>/tests/data plus synthetic dicts, so it
works both from a source checkout and inside cibuildwheel's test step.
"""

import warnings
from pathlib import Path

import numpy as np
import pytest

import dgread

DATA = Path(__file__).resolve().parents[2] / "tests" / "data"
FLAT = DATA / "testdata.dgz"                     # ints + floats, 10 rows
RAGGED = DATA / "j_cue_saccade_052209009.dgz"     # strings, nested, mixed lengths


def synthetic():
    """A rectangular group with every column kind dgread() produces."""
    return {
        "trial": np.arange(4, dtype=np.int32),
        "rt": np.array([10.0, 20.0, 30.0, 40.0], dtype=np.float32),
        "name": ["a", "b", "c", "d"],
        "samples": [
            np.array([1.0, 2.0, 4.0], dtype=np.float32),
            np.array([], dtype=np.float32),
            np.array([5.0], dtype=np.float32),
            np.array([0.0, 1.0], dtype=np.float32),
        ],
        "deep": [[np.array([1, 2])], [], [np.array([3]), np.array([4, 5])], []],
    }


# ---------------------------------------------------------------- package

def test_package_exports_c_functions():
    assert callable(dgread.dgread)
    assert callable(dgread.fromString)
    assert callable(dgread.fromString64)
    assert dgread.__version__


def test_read_accepts_path_and_matches_dgread():
    a = dgread.read(FLAT)
    b = dgread.dgread(str(FLAT))
    assert list(a) == list(b)
    np.testing.assert_array_equal(a["ints"], b["ints"])


def test_read_missing_file_names_path():
    with pytest.raises(FileNotFoundError, match="nope.dgz"):
        dgread.read(DATA / "nope.dgz")


def test_dgread_utils_shim_still_imports():
    import dgread_utils

    info = dgread_utils.summary(RAGGED)
    assert info["lists"]["ems"]["nested"] is True
    assert info["lists"]["target"]["nested"] is False


# ---------------------------------------------------------- classification

def test_is_nested_on_each_column_kind():
    d = synthetic()
    assert not dgread.is_nested(d["trial"])
    assert not dgread.is_nested(d["name"])
    assert dgread.is_nested(d["samples"])
    assert dgread.is_nested(d["deep"])
    assert dgread.nested_columns(d) == ["samples", "deep"]
    assert dgread.scalar_columns(d) == ["trial", "rt", "name"]


def test_row_count_is_most_common_length_ties_to_longer():
    assert dgread.row_count({"a": [1, 2], "b": [1, 2], "c": [1]}) == 2
    assert dgread.row_count({"a": [1, 2, 3], "b": [1]}) == 3
    assert dgread.row_count({}) == 0


def test_real_file_column_kinds():
    d = dgread.read(RAGGED)
    assert isinstance(d["target"], list) and isinstance(d["target"][0], str)
    assert isinstance(d["e_times"], list) and isinstance(d["e_times"][0], np.ndarray)
    # 54 per-trial columns of length 96 beside 11 per-obs event/spike
    # columns of length 97: the most common length wins.
    assert dgread.row_count(d) == 96


# ------------------------------------------------------------------ pandas

pd = pytest.importorskip("pandas")


def test_to_pandas_synthetic_keeps_nested_as_object_cells():
    df = dgread.to_pandas(synthetic())
    assert df.shape == (4, 5)
    assert df["trial"].dtype == np.int32
    assert isinstance(df["samples"][0], np.ndarray)
    assert len(df["samples"][1]) == 0
    assert df["name"].tolist() == ["a", "b", "c", "d"]
    # per-sample work via apply, the pandas idiom
    assert df["samples"].apply(len).tolist() == [3, 0, 1, 2]


def test_to_pandas_nested_drop_and_columns():
    df = dgread.to_pandas(synthetic(), nested="drop")
    assert list(df.columns) == ["trial", "rt", "name"]
    df = dgread.to_pandas(synthetic(), columns=["rt", "samples"])
    assert list(df.columns) == ["rt", "samples"]
    with pytest.raises(KeyError):
        dgread.to_pandas(synthetic(), columns=["nope"])
    with pytest.raises(ValueError):
        dgread.to_pandas(synthetic(), nested="explode")


def test_to_pandas_path_and_list_of_paths():
    one = dgread.to_pandas(FLAT)
    assert one.shape == (10, 2)
    two = dgread.to_pandas([FLAT, str(FLAT)])
    assert two.shape == (20, 2)
    assert two.index.tolist() == list(range(20))


def test_to_pandas_non_rectangular_warns_and_drops():
    with pytest.warns(UserWarning, match="e_names"):
        df = dgread.to_pandas(RAGGED)
    assert len(df) == 96
    assert "target" in df.columns
    assert "e_names" not in df.columns and "e_times" not in df.columns

    # the per-obs event stream is one row longer; ask for it explicitly
    with pytest.warns(UserWarning):
        df97 = dgread.to_pandas(RAGGED, n_rows=97)
    assert len(df97) == 97
    assert "e_times" in df97.columns and "target" not in df97.columns


def test_to_pandas_rejects_bad_source():
    with pytest.raises(TypeError):
        dgread.to_pandas(42)
    with pytest.raises(ValueError):
        dgread.to_pandas([])


# ----------------------------------------------------------------- awkward

ak = pytest.importorskip("awkward")


def test_to_awkward_synthetic_ragged_axes():
    a = dgread.to_awkward(synthetic())
    assert len(a) == 4
    assert set(a.fields) == {"trial", "rt", "name", "samples", "deep"}
    assert ak.num(a.samples, axis=1).tolist() == [3, 0, 1, 2]
    assert a.name.tolist() == ["a", "b", "c", "d"]
    # element dtype survives the fast path
    assert str(ak.type(a.samples.layout.content).content) == "float32"
    # the point of awkward: a per-sample diff with no loop
    d = a.samples[:, 1:] - a.samples[:, :-1]
    assert ak.max(d, axis=1).tolist() == [2.0, None, None, 1.0]
    # deeper nesting goes through from_iter and is still addressable
    assert ak.num(a.deep, axis=1).tolist() == [1, 0, 2, 0]


def test_to_awkward_matches_pandas_apply():
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        df = dgread.to_pandas(RAGGED, n_rows=97)
        a = dgread.to_awkward(RAGGED, n_rows=97)
    assert len(a) == len(df) == 97
    np.testing.assert_array_equal(
        ak.to_numpy(ak.num(a.e_times, axis=1)), df["e_times"].apply(len).to_numpy()
    )
    np.testing.assert_allclose(
        ak.to_numpy(ak.fill_none(ak.max(a.e_times, axis=1), -1)),
        df["e_times"].apply(lambda x: x.max() if len(x) else -1).to_numpy(),
    )


def test_to_awkward_list_of_sources_concatenates():
    a = dgread.to_awkward([FLAT, FLAT])
    assert len(a) == 20
    np.testing.assert_array_equal(ak.to_numpy(a.ints[:10]), ak.to_numpy(a.ints[10:]))


def test_to_awkward_all_empty_nested_column():
    d = {"x": np.arange(2), "s": [np.array([], dtype=np.int32), np.array([], dtype=np.int32)]}
    a = dgread.to_awkward(d)
    assert ak.num(a.s, axis=1).tolist() == [0, 0]
