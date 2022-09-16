#!/usr/bin/env python

from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext
import numpy as np                           # <---- New line

MOD = 'dgread'
setup(name=MOD, version='1.0',ext_modules=[
        Extension(MOD, sources=['dgread.c'], include_dirs=['./',np.get_include()], library_dirs=['/usr/local/lib'], libraries=['dg'])])
