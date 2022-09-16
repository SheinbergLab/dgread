#!/usr/bin/env python

from setuptools import setup
from setuptools import Extension

MOD = 'dgread'
setup(name=MOD, version='1.0',ext_modules=[
    Extension(MOD, sources=['dgread.c','df.c','dfutils.c','dynio.c','flip.c'], include_dirs=['c:/usr/local/include','c:/users/sheinb/Anaconda3/Lib/site-packages/numpy/core/include'], library_dirs=['c:/usr/local/lib'], libraries=['zlib64'], define_macros=[('WIN32', '1')])])
