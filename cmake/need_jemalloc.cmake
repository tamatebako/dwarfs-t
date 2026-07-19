#
# Copyright (c) 2025 Ribose Inc.
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

# Use tamatebako/jemalloc via vcpkg overlay port (ports/jemalloc/)
# The overlay port fetches from GitHub tag v5.5.0 and builds with CMake
# CRITICAL: This is the ONLY jemalloc with ARM64 support (Windows, Linux, macOS)

# Try to find jemalloc via CMake CONFIG first
find_package(jemalloc CONFIG QUIET)

if(NOT jemalloc_FOUND)
  # Fallback to pkg-config
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(jemalloc QUIET jemalloc)
    if(jemalloc_FOUND)
      # Create imported target for consistency
      if(NOT TARGET jemalloc::jemalloc)
        add_library(jemalloc::jemalloc INTERFACE IMPORTED)
        target_include_directories(jemalloc::jemalloc INTERFACE ${jemalloc_INCLUDE_DIRS})
        target_link_libraries(jemalloc::jemalloc INTERFACE ${jemalloc_LINK_LIBRARIES})
        target_link_directories(jemalloc::jemalloc INTERFACE ${jemalloc_LIBRARY_DIRS})
      endif()
      message(STATUS "Found jemalloc via pkg-config: ${jemalloc_VERSION}")
      set(JEMALLOC_FOUND TRUE)
    endif()
  endif()
endif()

if(NOT jemalloc_FOUND)
  message(FATAL_ERROR "jemalloc not found! Install via vcpkg, Homebrew, or system package manager.")
endif()

message(STATUS "Using jemalloc (preferably Tebako v5.5.0 for ARM64 support)")
set(JEMALLOC_FOUND TRUE)

# Verify jemalloc::jemalloc target exists
if(NOT TARGET jemalloc::jemalloc)
  message(FATAL_ERROR "jemalloc found but jemalloc::jemalloc target missing")
endif()

# CRITICAL: Set CMAKE_PREFIX_PATH and include dirs for Folly to find jemalloc
get_target_property(_jemalloc_includes jemalloc::jemalloc INTERFACE_INCLUDE_DIRECTORIES)
if(_jemalloc_includes)
  # Add to CMAKE_PREFIX_PATH so Folly's checks can find it
  foreach(_inc ${_jemalloc_includes})
    get_filename_component(_jemalloc_prefix "${_inc}" DIRECTORY)
    list(APPEND CMAKE_PREFIX_PATH "${_jemalloc_prefix}")
  endforeach()
  list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
  set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE PATH "Search path" FORCE)
endif()