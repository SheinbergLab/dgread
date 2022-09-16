#!/usr/bin/env python

from distutils.core import setup, Extension

npydir = '/Library/Frameworks/Python.framework/Versions/3.6/lib/python3.6/site-packages/numpy/core/include'
MOD = 'dgread'
setup(name=MOD, version='1.0',ext_modules=[
    Extension(MOD, sources=['dgread.c','df.c','dfutils.c','dynio.c','flip.c','lz4utils.c'], include_dirs=['./',npydir], library_dirs=['/Library/Frameworks/Python.framework/Versions/3.6/lib/python3.6/config-3.6m'], libraries=['z','lz4'])])
