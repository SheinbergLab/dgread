#!/usr/bin/env python

from distutils.core import setup
from distutils.extension import Extension
import numpy as np                           # <---- New line

MOD = 'dgread'
sources=['dgread.c','df.c','dfutils.c','dynio.c','flip.c','lz4utils.c', 'lz4frame.c', 'lz4hc.c', 'lz4.c', 'xxhash.c']
setup(name=MOD, version='1.0.1',ext_modules=[
        Extension(MOD, sources=sources, include_dirs=['./',np.get_include()], library_dirs=['/usr/local/lib', '.'], libraries=['z'])])
