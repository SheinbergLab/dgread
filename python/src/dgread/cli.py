"""Command-line entry points.

``dg2parquet`` turns a .dg/.dgz file into a Parquet file, which every
analysis tool opens without dgread::

    dg2parquet session.dgz                 # writes session.parquet
    dg2parquet session.dgz out.parquet
    dg2parquet session.dgz --n-rows 97     # the other table in a mixed file
    dg2parquet a.dgz b.dgz -o all.parquet  # concatenated, in order
"""

from __future__ import annotations

import argparse
import sys
import warnings
from pathlib import Path
from typing import List, Optional


def dg2parquet(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        prog="dg2parquet",
        description="Convert dg/dgz dynamic group files to Parquet.",
    )
    ap.add_argument("inputs", nargs="+", help="input .dg/.dgz file(s)")
    ap.add_argument(
        "-o", "--output",
        help="output .parquet path (default: the input's name with .parquet; "
             "required when more than one input is given)",
    )
    ap.add_argument(
        "--n-rows", type=int, default=None,
        help="row count to keep when the group is not rectangular "
             "(default: the most common column length)",
    )
    ap.add_argument(
        "--drop-nested", action="store_true",
        help="leave out ragged (per-sample) columns",
    )
    ap.add_argument(
        "--compression", default="snappy",
        help="parquet codec: snappy (default), zstd, gzip, brotli, lz4, none",
    )
    ap.add_argument(
        "-q", "--quiet", action="store_true",
        help="do not report dropped columns",
    )
    args = ap.parse_args(argv)

    if args.output is None:
        if len(args.inputs) > 1:
            ap.error("-o/--output is required with more than one input")
        src = Path(args.inputs[0])
        stem = src.name
        for suffix in (".dgz", ".dg"):
            if stem.endswith(suffix):
                stem = stem[: -len(suffix)]
                break
        args.output = str(src.with_name(stem + ".parquet"))

    import dgread

    sources = [args.inputs[0]] if len(args.inputs) == 1 else list(args.inputs)
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        table = dgread.to_parquet(
            sources,
            args.output,
            n_rows=args.n_rows,
            nested="drop" if args.drop_nested else "keep",
            compression=None if args.compression == "none" else args.compression,
        )
    if not args.quiet:
        for w in caught:
            print(f"dg2parquet: {w.message}", file=sys.stderr)
        print(f"{args.output}: {table.num_rows} rows x {table.num_columns} columns")
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(dg2parquet())
