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
          # Filter include directories to keep only ones that exist (CMake 4.x compatible)
          set(_filtered_include_dirs "")
          foreach(_dir IN LISTS FUSE_T_INCLUDE_DIRS)
            if(EXISTS "${_dir}")
              list(APPEND _filtered_include_dirs "${_dir}")
            endif()
          endforeach()

          # Always update the target properties, even if empty (removes bad paths)
          if(NOT _filtered_include_dirs)
            # No valid directories found, search manually
            # FUSE-T installs headers to fuse-t/ subdirectory
            find_path(FUSE_T_INCLUDE_BASE_DIR
              NAMES fuse/fuse.h
              PATHS
                /usr/local/include/fuse-t
                /opt/homebrew/include/fuse-t
                /usr/local/include
                /opt/homebrew/include
              NO_DEFAULT_PATH
              NO_CMAKE_FIND_ROOT_PATH
            )
            if(FUSE_T_INCLUDE_BASE_DIR)
              # get_filename_component returns the dir containing fuse.h, which is what we want
              set(_filtered_include_dirs "${FUSE_T_INCLUDE_BASE_DIR}")
            endif()
          endif()

          # Update the imported target's include directories
          if(TARGET PkgConfig::FUSE_T)
            set_target_properties(PkgConfig::FUSE_T PROPERTIES
              INTERFACE_INCLUDE_DIRECTORIES "${_filtered_include_dirs}")
          endif()
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
      message(WARNING "No FUSE implementation found on macOS. Install FUSE-T (brew install fuse-t) or macFUSE (brew install --cask macfuse) for FUSE driver support. Set DWARFS_WITH_FUSE=OFF to suppress this warning.")
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
      message(WARNING "No FUSE or FUSE3 library found. Install FUSE development packages for FUSE driver support. Set DWARFS_WITH_FUSE=OFF to suppress this warning.")
    endif()
  endif()
endif()

# Export implementation for use in main CMakeLists.txt
set(FUSE_IMPLEMENTATION ${FUSE_IMPLEMENTATION} CACHE STRING "FUSE implementation in use")
