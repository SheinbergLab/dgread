#!/usr/bin/env python

from distutils.core import setup, Extension

npydir = '/gpfs/runtime/opt/python/2.7.2/lib/python2.7/site-packages/numpy/core/include';
MOD = 'dgread'
setup(name=MOD, version='1.0',ext_modules=[
    Extension(MOD, sources=['dgread.c','df.c','dfutils.c','dynio.c','flip.c'], include_dirs=['./',npydir], library_dirs=['/usr/local/lib'], libraries=['z'])])
