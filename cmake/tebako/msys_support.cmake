#
# Copyright (c) Ribose Inc.
#
# This file is part of dwarfs tebako integration.
#
# SPDX-License-Identifier: MIT
#

# MSYS Support Module
#
# Configures Windows/MSYS-specific settings for tebako builds
# Handles winsock2, utfcpp, brotli, and other MSYS-specific requirements


message(STATUS "Configuring MSYS-specific settings")

# MSYS-specific compile definitions
# GFLAGS as static library on MSYS
add_compile_definitions(GFLAGS_IS_A_DLL=0)
message(STATUS "MSYS: Set GFLAGS_IS_A_DLL=0 for static linking")

# Winsock2 configuration for Windows networking
if(WIN32 OR MINGW OR MSYS)
  # Add winsock2 library for network functionality
  link_libraries(ws2_32)
  message(STATUS "MSYS: Added ws2_32 (winsock2) library")
endif()

# UTF-CPP configuration
# MSYS builds may need explicit UTF-8 support library
if(EXISTS "${DEPS_INCLUDE_DIR}/utf8cpp")
  include_directories(BEFORE SYSTEM "${DEPS_INCLUDE_DIR}/utf8cpp")
  message(STATUS "MSYS: Added utf8cpp include directory")
elseif(EXISTS "${DEPS_INCLUDE_DIR}/utf8")
  include_directories(BEFORE SYSTEM "${DEPS_INCLUDE_DIR}/utf8")
  message(STATUS "MSYS: Added utf8 include directory")
endif()

# Brotli configuration
# Brotli is typically disabled on MSYS per platform patches
# This is handled through the dependency build system, but we
# document it here for clarity
if(NOT DEFINED WITH_BROTLI)
  set(WITH_BROTLI OFF CACHE BOOL "Disable brotli on MSYS" FORCE)
  message(STATUS "MSYS: Disabled brotli compression (not supported)")
endif()

# MSYS-specific path handling
# MSYS uses Unix-style paths internally but may need conversion
# for native Windows tools
if(MINGW OR MSYS)
  # Ensure proper path separators
  file(TO_CMAKE_PATH "${DEPS}" DEPS)
  file(TO_CMAKE_PATH "${TOOLS}" TOOLS)
  file(TO_CMAKE_PATH "${DEPS_INCLUDE_DIR}" DEPS_INCLUDE_DIR)
  file(TO_CMAKE_PATH "${DEPS_LIB_DIR}" DEPS_LIB_DIR)
  message(STATUS "MSYS: Normalized paths to CMake format")
endif()

# Windows-specific threading
# MSYS/MinGW may need explicit pthread or Windows threading
if(MINGW)
  # MinGW typically uses winpthreads
  set(CMAKE_THREAD_PREFER_PTHREAD ON)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads)
  if(Threads_FOUND)
    message(STATUS "MSYS: Found threading support: ${CMAKE_THREAD_LIBS_INIT}")
  endif()
endif()

# Static runtime linking preference
# MSYS builds often prefer static C/C++ runtime to avoid DLL dependencies
if(MINGW AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING
      "MSVC runtime library" FORCE)
  message(STATUS "MSYS: Set static MSVC runtime library")
endif()

message(STATUS "MSYS-specific configuration complete")