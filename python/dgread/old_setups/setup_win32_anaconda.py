#!/usr/bin/env python

from distutils.core import setup, Extension

MOD = 'dgread'
setup(name=MOD, version='1.0',ext_modules=[
    Extension(MOD, sources=['dgread.c','df.c','dfutils.c','dynio.c','flip.c'], include_dirs=['c:/usr/local/include','C:/anaconda/Lib/site-packages/numpy/core/include'], library_dirs=['c:/usr/local/lib','.'], libraries=['zdll'], define_macros=[('WIN64', '1')])])
