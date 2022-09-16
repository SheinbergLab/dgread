cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_BUILD_TYPE Release)

if(APPLE)
	add_definitions(-Dunix)
	include_directories( /usr/local/include )
elseif(WIN32)

else()
	set (CMAKE_INSTALL_PREFIX /usr/local)
	add_definitions(-Dunix -DLINUX -DUSE_INTERP_RESULT -fPIC)
endif()

project( dg VERSION 1.00 DESCRIPTION "dg io library")

add_library(dg
  src/flipfuncs.c
  src/dfutils.c
  src/df.c
  src/dynio.c
  src/lz4utils.c
)

add_subdirectory( test )

if(WIN32)

else()
	set_target_properties(dg PROPERTIES PUBLIC_HEADER "src/dynio.h;src/df.h")
INSTALL(TARGETS dg
        LIBRARY DESTINATION /usr/local/lib
        ARCHIVE DESTINATION /usr/local/lib
        PUBLIC_HEADER DESTINATION /usr/local/include
)
endif()

