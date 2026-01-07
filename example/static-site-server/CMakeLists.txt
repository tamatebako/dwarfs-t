# Static Site Server Example
# Demonstrates libdwarfs C++ API integration
#
# This example is designed to be standalone and relocatable,
# it uses find_package() to locate installed DwarFS libraries.

cmake_minimum_required(VERSION 3.24)

# Use new Boost CMake config (CMP0167)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(static-site-server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find DwarFS via CMake package config
find_package(dwarfs CONFIG REQUIRED)

# Find libmicrohttpd via pkg-config (vcpkg only provides pkg-config module)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MICROHTTPD REQUIRED IMPORTED_TARGET libmicrohttpd>=0.9.70)

# Executable
add_executable(static-site-server
  main.cpp
  dwarfs_loader.cpp
  http_server.cpp
)

# Link libraries (dwarfs targets bring in all transitive dependencies)
target_link_libraries(static-site-server PRIVATE
  dwarfs::dwarfs_common
  dwarfs::dwarfs_reader
  PkgConfig::MICROHTTPD
)

# Compiler flags
target_compile_options(static-site-server PRIVATE
  -Wall -Wextra
  $<$<CONFIG:Release>:-O3>
  $<$<CONFIG:Debug>:-g -O0>
)

# Install target
install(TARGETS static-site-server
  RUNTIME DESTINATION bin
)

# Display configuration
message(STATUS "Static Site Server Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  DwarFS: Found via find_package")
message(STATUS "  libmicrohttpd: Found")
