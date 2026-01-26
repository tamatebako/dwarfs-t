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

# Option to disable FUSE support (useful for CI/builds without FUSE)
option(DWARFS_WITH_FUSE "Enable FUSE support for filesystem mounting" ON)

if(NOT DWARFS_WITH_FUSE)
  message(STATUS "FUSE support disabled by DWARFS_WITH_FUSE=OFF")
  return()
endif()

# Initialize FUSE_FOUND and FUSE_IMPLEMENTATION
set(FUSE_FOUND FALSE)
set(FUSE_IMPLEMENTATION "")

if(WIN32)
  if(NOT WINFSP_PATH)
    set(WINFSP_PATH "C:/Program Files (x86)/WinFsp")
  endif()
  find_library(WINFSP winfsp-x64.lib "${WINFSP_PATH}/lib")
  if (NOT WINFSP)
    message(STATUS "WinFsp not found - FUSE driver will not be built")
    set(FUSE_FOUND FALSE)
  else()
    set(FUSE_FOUND TRUE)
    set(FUSE_IMPLEMENTATION "winfsp")
  endif()
else()
  if(APPLE)
    # On macOS, we ONLY use FUSE-T
    # NOTE: FUSE-T does NOT provide pkg-config, so we search directly
    # FUSE-T installs headers to multiple possible locations:
    # - /usr/local/include/fuse (Homebrew default)
    # - /opt/homebrew/include/fuse (Apple Silicon Homebrew)
    # - /Library/Application Support/fuse-t/include/fuse (Official installer)
    # The code includes <fuse.h>, so we need to find the parent include dir

    # Search for the fuse/ directory in multiple locations
    find_path(FUSE_T_INCLUDE_BASE_DIR
      NAMES fuse.h
      PATHS
        /usr/local/include/fuse
        /opt/homebrew/include/fuse
        /Library/Application Support/fuse-t/include/fuse
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH
    )
    if(FUSE_T_INCLUDE_BASE_DIR)
      # FUSE_T_INCLUDE_BASE_DIR is the fuse/ directory containing fuse.h
      # Export it directly for use in tools.cmake and tool_support.cmake
      set(FUSE_T_INCLUDE_DIR "${FUSE_T_INCLUDE_BASE_DIR}" CACHE INTERNAL "FUSE-T include directory")
      set(FUSE_IMPLEMENTATION "fuse-t")
      set(FUSE_FOUND TRUE)
      message(STATUS "Found FUSE-T headers at: ${FUSE_T_INCLUDE_DIR}")
    else()
      message(FATAL_ERROR "FUSE-T headers not found. Searched in: /usr/local/include/fuse, /opt/homebrew/include/fuse, /Library/Application Support/fuse-t/include/fuse. Install FUSE-T: brew install fuse-t. Set DWARFS_WITH_FUSE=OFF to disable FUSE support.")
    endif()
  else()
    # Linux: Try FUSE3 first, then FUSE2
    pkg_check_modules(FUSE3 IMPORTED_TARGET fuse3>=3.10.5)
    pkg_check_modules(FUSE IMPORTED_TARGET fuse>=2.9.9)

    if(FUSE3_FOUND)
      set(FUSE_IMPLEMENTATION "fuse3")
    elseif(FUSE_FOUND)
      set(FUSE_IMPLEMENTATION "fuse2")
    else()
      message(FATAL_ERROR "No FUSE or FUSE3 library found. Install FUSE development packages for FUSE driver support. Set DWARFS_WITH_FUSE=OFF to disable FUSE support.")
    endif()
  endif()
endif()

# Export implementation for use in main CMakeLists.txt
set(FUSE_IMPLEMENTATION ${FUSE_IMPLEMENTATION} CACHE STRING "FUSE implementation in use")
