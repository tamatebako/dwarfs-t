# DwarFS Tools - Standalone Build
# Uses vcpkg-installed libdwarfs libraries via find_package()
#
# Build instructions:
#   1. Install libdwarfs: vcpkg install dwarfs --overlay-ports=vcpkg_ports
#   2. Configure: cmake -B build-vcpkg -S tools -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
#   3. Build: cmake --build build-vcpkg --parallel

cmake_minimum_required(VERSION 3.24)
project(dwarfs_tools)

# Set C++ standard to match DwarFS requirements
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add tools/include to include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Required for pkg-config
find_package(PkgConfig REQUIRED)

# Find jemalloc (required by dwarfs_tool_support)
pkg_check_modules(jemalloc REQUIRED IMPORTED_TARGET jemalloc>=5.2.1)
if(jemalloc_FOUND)
  add_library(jemalloc::jemalloc_static ALIAS PkgConfig::jemalloc)
endif()

# Find installed DwarFS libraries
find_package(dwarfs REQUIRED CONFIG)

# mkdwarfs - filesystem creator
add_executable(mkdwarfs
  src/mkdwarfs_main.cpp
  src/mkdwarfs.cpp
)
target_link_libraries(mkdwarfs PRIVATE
  dwarfs::dwarfs_tool_support
  dwarfs::dwarfs_writer
)

# dwarfsck - filesystem checker
add_executable(dwarfsck
  src/dwarfsck_main.cpp
  src/dwarfsck.cpp
)
target_link_libraries(dwarfsck PRIVATE
  dwarfs::dwarfs_tool_support
  dwarfs::dwarfs_reader
)

# dwarfsextract - filesystem extractor
add_executable(dwarfsextract
  src/dwarfsextract_main.cpp
  src/dwarfsextract.cpp
)
target_link_libraries(dwarfsextract PRIVATE
  dwarfs::dwarfs_tool_support
  dwarfs::dwarfs_extractor
)

# Install binaries
install(TARGETS mkdwarfs dwarfsck dwarfsextract
        RUNTIME DESTINATION bin)