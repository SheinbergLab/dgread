## libdg.a - file I/O for dg and dgz files

The libdg library provides a C-API for reading and writing dynamic groups (dgs).  These files can be in multiple formats:

* `.dg`:   binary uncompressed dg file
* `.dgz`:  binary compressed dg file
* `.lz4`:  binary lz4 compressed files

Dg specific code for the library specific is found in the src directory.  Code from libz and liblz4 are in the zsrc folder (and these need to be properly updated with the appropriate LICENSE files)

To build the library:

```
mkdir build
cd build
cmake ..
make
```

This will build a static library `libdg.a` and the test programs in the test folder.

To read a test .dgz file:

```
./test/testdgread ../data/normals.dgz
```

### Dependencies
For compression, the library depends on the following open source libs:
* [libz](http://zlib.net/)
* [liblz4](https://lz4.github.io/lz4/)
