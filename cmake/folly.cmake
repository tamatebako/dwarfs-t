#
# Copyright (c) Marcus Holland-Moritz
#
# This file is part of dwarfs.
#
# dwarfs is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# dwarfs is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# dwarfs.  If not, see <https://www.gnu.org/licenses/>.
#

#
# VCPKG-ONLY FOLLY CONFIGURATION
#
# This file enforces use of Folly from vcpkg overlay ports (Tebako fork).
# Submodule-based builds are NO LONGER SUPPORTED.
#

# Enforce vcpkg toolchain
if(NOT VCPKG_TOOLCHAIN AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR
    "Folly MUST be provided by vcpkg. "
    "Set CMAKE_TOOLCHAIN_FILE to vcpkg.cmake or use -DCMAKE_TOOLCHAIN_FILE=\${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  )
endif()

# Configuration for Folly (applied before find_package)
set(FOLLY_NO_EXCEPTION_TRACER ON CACHE BOOL "disable exception tracer")

# Disable zstd in Folly (DwarFS uses its own zstd)
set(ZSTD_INCLUDE_DIR "" CACHE PATH "don't build folly with zstd" FORCE)
set(ZSTD_LIBRARY_RELEASE "ZSTD_LIBRARY_RELEASE-NOTFOUND" CACHE FILEPATH "don't build folly with zstd" FORCE)
set(ZSTD_LIBRARY_DEBUG "ZSTD_LIBRARY_DEBUG-NOTFOUND" CACHE FILEPATH "don't build folly with zstd" FORCE)

# CRITICAL: Do NOT force FOLLY_USE_JEMALLOC OFF
# Per critical-rules.md RULE 1: We MUST use Tebako's jemalloc fork
# Let Folly auto-detect and use jemalloc if available

add_compile_definitions(FOLLY_CFG_NO_COROUTINES)
add_compile_definitions(GLOG_NO_ABBREVIATED_SEVERITIES NOMINMAX NOGDI)

# Temporary workaround for glog export issue
# See: https://github.com/facebook/folly/issues/2149
add_compile_definitions(GLOG_USE_GLOG_EXPORT)

set(CXX_STD "gnu++${DWARFS_CXX_STANDARD}" CACHE STRING "The C++ standard argument to pass to the compiler.")
set(MSVC_LANGUAGE_VERSION "c++${DWARFS_CXX_STANDARD}" CACHE STRING "The C++ standard argument to pass to the compiler.")

# Disable unnecessary dependencies in Folly
# NOTE: When Modern Thrift (fbthrift) is enabled, we need ZLIB and Libsodium
# for the fizz dependency chain. Only disable these when NOT using Modern Thrift.
if(NOT DWARFS_WITH_THRIFT)
  set(CMAKE_DISABLE_FIND_PACKAGE_ZLIB ON)
  set(CMAKE_DISABLE_FIND_PACKAGE_Libsodium ON)
endif()
set(CMAKE_DISABLE_FIND_PACKAGE_BZip2 ON)
set(CMAKE_DISABLE_FIND_PACKAGE_Snappy ON)
set(CMAKE_DISABLE_FIND_PACKAGE_LibAIO ON)
set(CMAKE_DISABLE_FIND_PACKAGE_LibUring ON)
set(CMAKE_DISABLE_FIND_PACKAGE_LibDwarf ON)

# jemalloc configuration (CRITICAL - RULE 1)
# Only force FOLLY_USE_JEMALLOC OFF if explicitly disabled
# Let Folly detect and use jemalloc if available (e.g., from Tebako vcpkg overlay)
if(DEFINED USE_JEMALLOC AND NOT USE_JEMALLOC)
  set(FOLLY_USE_JEMALLOC OFF CACHE BOOL "Disable jemalloc per USE_JEMALLOC option" FORCE)
  set(FOLLY_HAVE_MALLOC_USABLE_SIZE OFF CACHE BOOL "No malloc_usable_size without jemalloc" FORCE)
  add_compile_definitions(FOLLY_ASSUME_NO_JEMALLOC=1 FOLLY_ASSUME_NO_TCMALLOC=1)
else()
  # When jemalloc IS enabled (or not explicitly disabled), configure it
  # This applies to ALL builds, not just Thrift builds
  if(TARGET jemalloc::jemalloc OR TARGET PkgConfig::JEMALLOC)
    # Get jemalloc include directories (works for both vcpkg and pkg-config)
    set(_jemalloc_includes "")
    if(TARGET jemalloc::jemalloc)
      get_target_property(_jemalloc_includes jemalloc::jemalloc INTERFACE_INCLUDE_DIRECTORIES)
    elseif(TARGET PkgConfig::JEMALLOC)
      get_target_property(_jemalloc_includes PkgConfig::JEMALLOC INTERFACE_INCLUDE_DIRECTORIES)
    endif()

    if(_jemalloc_includes)
      # Add to CMAKE_REQUIRED_INCLUDES for Folly's try_compile checks
      list(APPEND CMAKE_REQUIRED_INCLUDES ${_jemalloc_includes})
      # Add as compiler flags so Folly's configure checks can find jemalloc headers
      foreach(_inc ${_jemalloc_includes})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${_inc}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${_inc}")
      endforeach()
    endif()

    # Get libraries and add to linker flags (vcpkg only)
    if(TARGET jemalloc::jemalloc)
      get_target_property(_jemalloc_libs jemalloc::jemalloc INTERFACE_LINK_LIBRARIES)
      if(_jemalloc_libs)
        foreach(_lib ${_jemalloc_libs})
          set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${_lib}")
        endforeach()
      endif()
    endif()

    # CRITICAL: Define USE_JEMALLOC globally for ALL libdwarfs code
    # This ensures consistent ABI between Folly and DwarFS libraries
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_JEMALLOC=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_JEMALLOC=1")
  endif()
endif()

# Find Folly from vcpkg
find_package(folly CONFIG REQUIRED)

message(STATUS "Using vcpkg-provided Folly library")

# Verify Folly::folly target exists
if(NOT TARGET Folly::folly)
  message(FATAL_ERROR "Folly::folly target not found. Ensure vcpkg overlay ports are configured correctly.")
endif()
