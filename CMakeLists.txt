cmake_minimum_required(VERSION 3.14)

project(dg 
    VERSION 1.1.0 
    DESCRIPTION "dg/dgz dynamic group I/O library"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")

# Platform-specific settings
if(APPLE)
    add_definitions(-Dunix)
elseif(WIN32)
    # Windows-specific settings
else()
    # Linux/Unix
    add_definitions(-Dunix -DLINUX -fPIC)
endif()

# Core library sources
set(DG_CORE_SOURCES
    src/core/df.c
    src/core/dfutils.c
    src/core/dynio.c
    src/core/flipfuncs.c
    src/core/lz4utils.c
)

# LZ4 sources
set(LZ4_SOURCES
    src/lz4/lz4.c
    src/lz4/lz4hc.c
    src/lz4/lz4frame.c
    src/lz4/xxhash.c
)

# Public headers
set(DG_PUBLIC_HEADERS
    src/core/df.h
    src/core/dynio.h
)

# Create the library
add_library(dg ${DG_CORE_SOURCES} ${LZ4_SOURCES})

target_include_directories(dg 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lz4>
        $<INSTALL_INTERFACE:include>
)

# Link zlib
find_package(ZLIB REQUIRED)
target_link_libraries(dg PRIVATE ZLIB::ZLIB)

# Set library properties
set_target_properties(dg PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${DG_PUBLIC_HEADERS}"
)

# Installation
include(GNUInstallDirs)

install(TARGETS dg
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Optional: build test executable
option(BUILD_TESTS "Build test executable" ON)
if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()

# pkg-config file generation
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dg.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dg.pc"
    @ONLY
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dg.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
