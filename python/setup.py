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
extra_objects = []

if platform.system() == 'Windows':
    # Windows: use conda-forge zlib if available (installed by cibuildwheel)
    extra_compile_args = ['/DWINDOWS', '/wd4244', '/wd4267', '/wd4996']
    
    # Look for zlib in common locations
    conda_prefix = os.environ.get('CONDA_PREFIX', '')
    if conda_prefix:
        include_dirs.append(os.path.join(conda_prefix, 'Library', 'include'))
        library_dirs.append(os.path.join(conda_prefix, 'Library', 'lib'))
    
    # Also check for vcpkg
    vcpkg_root = os.environ.get('VCPKG_ROOT', r'C:\vcpkg')
    vcpkg_installed = os.path.join(vcpkg_root, 'installed', 'x64-windows')
    if os.path.exists(vcpkg_installed):
        include_dirs.append(os.path.join(vcpkg_installed, 'include'))
        library_dirs.append(os.path.join(vcpkg_installed, 'lib'))
    
    libraries.append('zlib')
    
elif platform.system() == 'Darwin':
    libraries.append('z')
else:
    # Linux
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
