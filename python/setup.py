#!/usr/bin/env python
"""
Build script for dgread C extension.

Sources are pulled from the shared ../src/ directory.
"""

from setuptools import setup, Extension
import numpy as np
import platform
import os

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
]

# Include directories - also relative
include_dirs = [
    '.',
    '../src/core',
    '../src/lz4',
    np.get_include(),
]

# Platform-specific configuration
library_dirs = []
libraries = []
extra_compile_args = []
extra_link_args = []

if platform.system() == 'Windows':
    libraries.append('z')
    extra_compile_args = ['/wd4244', '/wd4267']
elif platform.system() == 'Darwin':
    libraries.append('z')
    # Remove the old macOS version flag - let the system decide
else:
    libraries.append('z')

dgread_ext = Extension(
    'dgread',
    sources=sources,
    include_dirs=include_dirs,
    library_dirs=library_dirs,
    libraries=libraries,
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
)

setup(
    ext_modules=[dgread_ext],
)
