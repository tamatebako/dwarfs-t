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
if(DEFINED ENV{DWARFS_BUILD_DIR})
  # Use local dwarfs build directly
  set(DWARFS_LIBRARIES
    $ENV{DWARFS_BUILD_DIR}/libdwarfs_common.a
    $ENV{DWARFS_BUILD_DIR}/libdwarfs_reader.a
    $ENV{DWARFS_BUILD_DIR}/libdwarfs_metadata_legacy.a
    $ENV{DWARFS_BUILD_DIR}/libdwarfs_compressor.a
    $ENV{DWARFS_BUILD_DIR}/libdwarfs_decompressor.a
    $ENV{DWARFS_BUILD_DIR}/libdwarfs_writer.a
  )
  set(DWARFS_INCLUDE_DIRS
    $ENV{DWARFS_BUILD_DIR}/../include
    $ENV{DWARFS_BUILD_DIR}
  )
  message(STATUS "Using local dwarfs build from: $ENV{DWARFS_BUILD_DIR}")
else()
  find_package(dwarfs CONFIG REQUIRED)
endif()

# Check if we're using vcpkg
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake")
  message(STATUS "Building with vcpkg - using vcpkg dependencies")

  # Find libmicrohttpd via pkg-config (vcpkg only provides pkg-config module)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(MICROHTTPD REQUIRED IMPORTED_TARGET libmicrohttpd>=0.9.70)

  # Find Boost from vcpkg
  find_package(Boost REQUIRED COMPONENTS
    program_options
    filesystem
    iostreams
    thread
    chrono
    context
  )

  # vcpkg provides fmt via CONFIG
  find_package(fmt CONFIG REQUIRED)
else()
  message(STATUS "Building without vcpkg - using Homebrew dependencies")

  # Find libmicrohttpd via pkg-config (vcpkg only provides pkg-config module)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(MICROHTTPD REQUIRED IMPORTED_TARGET libmicrohttpd>=0.9.70)

  # Find Boost
  find_package(Boost REQUIRED COMPONENTS
    program_options
    filesystem
    iostreams
    thread
    chrono
    context
  )

  # Find fmt
  find_package(fmt CONFIG REQUIRED)
endif()

# Executable
add_executable(static-site-server
  main.cpp
  dwarfs_loader.cpp
  http_server.cpp
)

# Link libraries (dwarfs targets bring in all transitive dependencies)
if(DEFINED ENV{DWARFS_BUILD_DIR})
  # Using local dwarfs build - link directly to libraries
  target_include_directories(static-site-server PRIVATE
    ${DWARFS_INCLUDE_DIRS}
    $ENV{DWARFS_BUILD_DIR}/include
  )

  if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake")
    # vcpkg build - use vcpkg libraries
    # Need to link against vcpkg libraries that libdwarfs depends on
    # Get the vcpkg installed directory
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(VCPKG_LIB_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/debug/lib")
    else()
      set(VCPKG_LIB_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib")
    endif()

    target_link_libraries(static-site-server PRIVATE
      ${DWARFS_LIBRARIES}
      PkgConfig::MICROHTTPD
      Boost::program_options
      Boost::filesystem
      Boost::iostreams
      Boost::thread
      Boost::chrono
      Boost::context
      fmt::fmt
      # vcpkg compression and crypto libraries
      "${VCPKG_LIB_DIR}/libzstd.a"
      "${VCPKG_LIB_DIR}/liblz4.a"
      "${VCPKG_LIB_DIR}/liblzma.a"
      "${VCPKG_LIB_DIR}/libbrotlienc.a"
      "${VCPKG_LIB_DIR}/libbrotlidec.a"
      "${VCPKG_LIB_DIR}/libbrotlicommon.a"
      "${VCPKG_LIB_DIR}/libFLAC++.a"
      "${VCPKG_LIB_DIR}/libFLAC.a"
      "${VCPKG_LIB_DIR}/libogg.a"
      "${VCPKG_LIB_DIR}/libxxhash.a"
      "${VCPKG_LIB_DIR}/libarchive.a"
      "${VCPKG_LIB_DIR}/libcrypto.a"
      "${VCPKG_LIB_DIR}/libssl.a"
      Threads::Threads
    )
  else()
    # Homebrew build - use system libraries
    target_include_directories(static-site-server PRIVATE /opt/homebrew/include)
    target_link_directories(static-site-server PRIVATE /opt/homebrew/lib /opt/homebrew/opt/flac/lib)
    target_link_libraries(static-site-server PRIVATE
      ${DWARFS_LIBRARIES}
      PkgConfig::MICROHTTPD
      Boost::program_options
      Boost::filesystem
      Boost::iostreams
      Boost::thread
      Boost::chrono
      Boost::context
      fmt::fmt
      "-lzstd" "-llz4" "-llzma"
      "-lbrotlienc" "-lbrotlidec"
      "-lFLAC++" "-lFLAC"
      "-lxxhash"
      "-larchive"
      "-lssl" "-lcrypto"
      "-lpthread"
    )
  endif()
else()
  target_link_libraries(static-site-server PRIVATE
    dwarfs::dwarfs_common
    dwarfs::dwarfs_reader
    PkgConfig::MICROHTTPD
  )
endif()

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
