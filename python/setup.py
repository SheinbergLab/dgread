#!/usr/bin/env python
"""
Build script for dgread C extension.

Sources are pulled from the shared ../src/ directory.
"""

from setuptools import setup, Extension
import numpy as np
import platform
import os
import sys

# C source files - use relative paths from python/ directory
sources = [
    # Python wrapper (in this directory)
    'dgread.c',
    
    # Shared core sources
    '../src/core/df.c',
    '../src/core/dfutils.c',
    '../src/core/dynio.c',
    '../src/core/flipfuncs.c',
    '../src/core/lz4utils.c',
    
    # LZ4 sources
    '../src/lz4/lz4.c',
    '../src/lz4/lz4hc.c',
    '../src/lz4/lz4frame.c',
    '../src/lz4/xxhash.c',

    # zlib sources (vendored, ../src/zlib) -- compiled in so the wheel is
    # self-contained: no system/vcpkg zlib needed on any platform.
    '../src/zlib/adler32.c',
    '../src/zlib/compress.c',
    '../src/zlib/crc32.c',
    '../src/zlib/deflate.c',
    '../src/zlib/gzclose.c',
    '../src/zlib/gzlib.c',
    '../src/zlib/gzread.c',
    '../src/zlib/gzwrite.c',
    '../src/zlib/infback.c',
    '../src/zlib/inffast.c',
    '../src/zlib/inflate.c',
    '../src/zlib/inftrees.c',
    '../src/zlib/trees.c',
    '../src/zlib/uncompr.c',
    '../src/zlib/zutil.c',
]

# Include directories - also relative. ../src/zlib first so the vendored
# zlib.h is used (over any system zlib) for the core's <zlib.h> includes.
include_dirs = [
    '.',
    '../src/core',
    '../src/lz4',
    '../src/zlib',
    np.get_include(),
]

# Platform-specific configuration
library_dirs = []
libraries = []
extra_compile_args = []
extra_link_args = []
extra_objects = []
define_macros = []

# zlib is vendored (../src/zlib) and compiled in, so there is no external
# zlib to find or link on any platform.
if platform.system() == 'Windows':
    # MSVC: silence the usual C4244/4267/4996 noise from the vendored C.
    # Do NOT pass /DWINDOWS: it makes dynio.c define ZLIB_DLL, which would
    # pull zlib in via __declspec(dllimport) and fail to link against the
    # statically-vendored zlib objects.
    extra_compile_args = ['/wd4244', '/wd4267', '/wd4996']
else:
    # POSIX: zlib's gz*.c use lseek/read/write/close from <unistd.h>, which
    # zlib only includes when Z_HAVE_UNISTD_H is set (normally by zlib's
    # ./configure). We vendor the unconfigured zconf.h, so define it here.
    define_macros.append(('Z_HAVE_UNISTD_H', '1'))

dgread_ext = Extension(
    'dgread',
    sources=sources,
    include_dirs=include_dirs,
    library_dirs=library_dirs,
    libraries=libraries,
    define_macros=define_macros,
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
)

setup(
    ext_modules=[dgread_ext],
)
