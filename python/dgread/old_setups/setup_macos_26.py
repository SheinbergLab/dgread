#!/usr/bin/env python

from distutils.core import setup, Extension

npydir = '/usr/local/lib/python2.7/site-packages/numpy/core/include';

npydir = '/System/Library/Frameworks/Python.framework/Versions/2.7/Extras/lib/python/numpy/numarray/include/numpy/';

MOD = 'dgread'
setup(name=MOD, version='1.0',ext_modules=[
    Extension(MOD, sources=['dgread.c','df.c','dfutils.c','dynio.c','flip.c'], include_dirs=['./',npydir], library_dirs=['/usr/local/lib'], libraries=['z'])])
