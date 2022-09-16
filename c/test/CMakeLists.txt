cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_BUILD_TYPE Release)

if(APPLE)
	add_definitions(-Dunix)
elseif(WIN32)

else()
	add_definitions(-Dunix -DLINUX -fPIC)
endif()

project( dg VERSION 0.95 DESCRIPTION "dg I/O test code")

if(WIN32)
  set(LIBZ zdll64.lib)
  set(LIBLZ4 liblz4_static64.lib)
else()
  find_library(LIBZ z)
  find_library(LIBLZ4 NAMES "liblz4.a")
endif()

include_directories( ../src )
link_directories( .. )
add_executable( testdgread src/testdgread.c )
target_link_libraries( testdgread dg ${LIBZ} ${LIBLZ4} )

