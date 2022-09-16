#!/usr/bin/env python

from distutils.core import setup, Extension

MOD = 'dgread'
setup(name=MOD, version='1.0',ext_modules=[
    Extension(MOD, sources=['dgread.c','df.c','dfutils.c','lz4utils.c','dynio.c','flip.c','lz4.c','lz4frame.c','lz4hc.c','xxhash.c'], include_dirs=['c:/usr/local/include','C:/Python27/Lib/site-packages/numpy/core/include'], library_dirs=['c:/usr/local/lib'], libraries=['zdll'], define_macros=[('WIN32', '1')])])
