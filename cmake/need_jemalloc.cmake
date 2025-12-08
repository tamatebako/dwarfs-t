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

# Conditional minimum version for tebako compatibility
if(DEFINED ENV{TEBAKO_BUILD} OR TEBAKO_BUILD)
  cmake_minimum_required(VERSION 3.24.0)
else()
  cmake_minimum_required(VERSION 3.28.0)
endif()

# Try to find jemalloc via pkg-config first (system package)
pkg_check_modules(JEMALLOC IMPORTED_TARGET jemalloc>=${JEMALLOC_REQUIRED_VERSION})

if(JEMALLOC_FOUND)
  message(STATUS "Using system jemalloc: ${JEMALLOC_VERSION}")
  # Create alias for consistent interface
  if(NOT TARGET jemalloc::jemalloc)
    add_library(jemalloc::jemalloc ALIAS PkgConfig::JEMALLOC)
  endif()
else()
  # Fall back to tamatebako fork which supports more platforms (including Windows ARM64, musl)
  message(STATUS "System jemalloc not found, fetching tamatebako fork...")

  if(DEFINED ENV{DWARFS_LOCAL_REPO_PATH})
    set(JEMALLOC_GIT_REPO $ENV{DWARFS_LOCAL_REPO_PATH}/jemalloc)
  else()
    set(JEMALLOC_GIT_REPO https://github.com/tamatebako/jemalloc.git)
  endif()

  FetchContent_Declare(
    jemalloc
    GIT_REPOSITORY ${JEMALLOC_GIT_REPO}
    GIT_TAG 5.5.0
    EXCLUDE_FROM_ALL
    SYSTEM
  )

  # jemalloc build options
  set(JEMALLOC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(JEMALLOC_BUILD_DOC OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(jemalloc)

  message(STATUS "jemalloc fetched successfully from tamatebako fork (v5.5.0)")

  # The tamatebako fork provides jemalloc::jemalloc target
  if(NOT TARGET jemalloc::jemalloc)
    message(FATAL_ERROR "jemalloc::jemalloc target not found after fetch")
  endif()
endif()