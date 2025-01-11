#!/usr/bin/env python

from setuptools import setup, Extension
import platform
import numpy as np

MOD = 'dgread'
version = "1.0.2"
sources=['dgread.c','df.c','dfutils.c','dynio.c','flip.c','lz4utils.c', 'lz4frame.c', 'lz4hc.c', 'lz4.c', 'xxhash.c']
library_dirs = ['/usr/local/lib']

if platform.system() == 'Windows':
    library_dirs.append('.')

setup(name=MOD,
      version=version ,
      ext_modules=[
        Extension(MOD,
                  sources=sources,
                  include_dirs=['./',np.get_include()],
                  library_dirs=library_dirs, libraries=['z'])
      ])
