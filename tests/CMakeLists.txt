cmake_minimum_required(VERSION 3.14)

project(dg_tests VERSION 1.0 DESCRIPTION "dg I/O test code")

# Test executable
add_executable(testdgread src/testdgread.c)

# Link against the dg library (built by parent CMakeLists.txt)
target_link_libraries(testdgread PRIVATE dg)

# Copy test data files to build directory (if any exist)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/data" 
         DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()

# Optional: register as CTest
enable_testing()
add_test(NAME testdgread COMMAND testdgread)
