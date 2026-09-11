#!/usr/bin/env bash
#
# sync-core.sh -- keep the one copy of the dg core the one copy.
#
#   scripts/sync-core.sh pull [path/to/dlsh]   copy lablib from a dlsh checkout
#                                              into src/core (default ../dlsh)
#   scripts/sync-core.sh r                     copy src/core + src/lz4 into R/src
#   scripts/sync-core.sh check                 fail if R/src has drifted from
#                                              src/core + src/lz4
#
# Why R/src holds a copy at all: R CMD build/INSTALL compiles only what is
# inside the package directory, and the built tarball contains nothing else,
# so R cannot reference ../../src/core the way python/setup.py and
# matlab/build_dgread.m do.  The copy is therefore unavoidable; this script
# makes it a generated artifact instead of a hand-maintained fork.  Run
# `sync-core.sh r` after any change to src/core and commit the result;
# `sync-core.sh check` is cheap enough to run before every release.
#
# The upstream source of truth is dlsh/src/lablib.  `pull` copies exactly the
# files the readers compile (see CMakeLists.txt DG_CORE_SOURCES and
# python/setup.py); nothing in dgread edits these files directly.

set -euo pipefail

here="$(cd "$(dirname "$0")/.." && pwd)"
core_files="df.c df.h dfutils.c dynio.c dynio.h flipfuncs.c flipfuncs.h utilc.h lz4utils.c"
lz4_files="lz4.c lz4.h lz4hc.c lz4hc.h lz4frame.c lz4frame.h lz4frame_static.h xxhash.c xxhash.h"

cmd="${1:-}"
case "$cmd" in
  pull)
    dlsh="${2:-$here/../dlsh}"
    src="$dlsh/src/lablib"
    [ -d "$src" ] || { echo "sync-core: no lablib at $src" >&2; exit 1; }
    for f in $core_files; do
      cp "$src/$f" "$here/src/core/$f"
    done
    echo "sync-core: src/core updated from $src"
    ;;
  r)
    for f in $core_files; do cp "$here/src/core/$f" "$here/R/src/$f"; done
    for f in $lz4_files;  do cp "$here/src/lz4/$f"  "$here/R/src/$f"; done
    echo "sync-core: R/src updated from src/core and src/lz4"
    ;;
  check)
    rc=0
    for f in $core_files; do
      cmp -s "$here/src/core/$f" "$here/R/src/$f" || { echo "R/src/$f differs from src/core/$f"; rc=1; }
    done
    for f in $lz4_files; do
      cmp -s "$here/src/lz4/$f" "$here/R/src/$f" || { echo "R/src/$f differs from src/lz4/$f"; rc=1; }
    done
    [ $rc -eq 0 ] && echo "sync-core: R/src is in sync"
    exit $rc
    ;;
  *)
    sed -n '2,20p' "$0"
    exit 2
    ;;
esac
