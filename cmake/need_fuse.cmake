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

# Conditional minimum version for tebako compatibility

if(WIN32)
  if(NOT WINFSP_PATH)
    set(WINFSP_PATH "C:/Program Files (x86)/WinFsp")
  endif()
  find_library(WINFSP winfsp-x64.lib "${WINFSP_PATH}/lib")
  if (NOT WINFSP)
    message(FATAL_ERROR "No WinFsp library found")
  endif()
else()
  if(APPLE)
    # On macOS, try FUSE-T first (default), then macFUSE/osxfuse
    option(USE_FUSE_T "Prefer FUSE-T over macFUSE on macOS" ON)

    if(USE_FUSE_T)
      # Try FUSE-T first
      pkg_check_modules(FUSE_T IMPORTED_TARGET fuse-t>=1.0.0)
      if(FUSE_T_FOUND)
        set(FUSE_IMPLEMENTATION "fuse-t")
        set(FUSE_FOUND TRUE)
        # Fix include directories for FUSE-T (pkg-config may point to wrong path)
        # FUSE-T installs to /usr/local/include/fuse-t/ not /usr/local/include/fuse/
        if(FUSE_T_INCLUDE_DIRS)
          list(FILTER FUSE_T_INCLUDE_DIRS EXISTS REGEX ".*/fuse-t$" INCLUDES_EXIST)
          if(INCLUDES_EXIST)
            set(FUSE_T_INCLUDE_DIRS "${INCLUDES_EXIST}")
          else()
            # Fallback: try to find the actual fuse-t include directory
            find_path(FUSE_T_INCLUDE_DIR fuse/fuse.h
              PATHS
                /usr/local/include/fuse-t
                /usr/local/include
              NO_DEFAULT_PATH
              NO_CMAKE_FIND_ROOT_PATH
            )
            if(FUSE_T_INCLUDE_DIR)
              get_filename_component(FUSE_T_INCLUDE_DIR "${FUSE_T_INCLUDE_DIR}" DIRECTORY)
              set(FUSE_T_INCLUDE_DIRS "${FUSE_T_INCLUDE_DIR}")
            endif()
          endif()
        endif()
        # Create alias for consistency with other platforms (only if not already exists)
        if(NOT TARGET PkgConfig::FUSE)
          add_library(PkgConfig::FUSE ALIAS PkgConfig::FUSE_T)
        endif()
        message(STATUS "Using FUSE-T on macOS (version ${FUSE_T_VERSION})")
      endif()
    endif()

    # Fallback to macFUSE/osxfuse if FUSE-T not found or not preferred
    if(NOT FUSE_FOUND)
      pkg_check_modules(FUSE IMPORTED_TARGET fuse>=2.9.9)
      if(FUSE_FOUND)
        set(FUSE_IMPLEMENTATION "macos-fuse")
        message(STATUS "Using macFUSE/osxfuse on macOS (version ${FUSE_VERSION})")
      endif()
    endif()

    if(NOT FUSE_FOUND)
      message(FATAL_ERROR "No FUSE implementation found on macOS. Install FUSE-T (brew install fuse-t) or macFUSE (brew install --cask macfuse), or set DWARFS_WITH_FUSE=OFF to disable FUSE support.")
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
      message(FATAL_ERROR "No FUSE or FUSE3 library found. Install FUSE development packages, or set DWARFS_WITH_FUSE=OFF to disable FUSE support.")
    endif()
  endif()
endif()

# Export implementation for use in main CMakeLists.txt
set(FUSE_IMPLEMENTATION ${FUSE_IMPLEMENTATION} CACHE STRING "FUSE implementation in use")
