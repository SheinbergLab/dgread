#!/usr/bin/env python

from distutils.core import setup, Extension

npydir = '/usr/local/lib/python3.4/dist-packages/numpy/core/include';
MOD = 'dgread'
setup(name=MOD, version='1.0',ext_modules=[
        Extension(MOD, sources=['dgread.c','df.c','dfutils.c','lz4utils.c','dynio.c','flip.c','lz4.c','lz4frame.c','lz4hc.c','xxhash.c'], include_dirs=['./',npydir], library_dirs=['/usr/local/lib'], libraries=['z'])])
