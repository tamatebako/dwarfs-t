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
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# dwarfs.  If not, see <https://www.gnu.org/licenses/>.
#

# ============================================================================
# libdwarfs_c - Stable C ABI for the DwarFS reader
# ============================================================================
#
# A thin extern "C" layer over the C++ reader (dwarfs_reader). Consumers see
# only include/dwarfs_c.h; the C++ runtime and all dwarfs C++ symbols stay
# inside the binding. This target is built as a static library and exported
# as dwarfs::dwarfs_c through the regular dwarfs CMake package config.

add_library(dwarfs_c src/dwarfs_c.cpp)

# dwarfs_reader for the reader API, dwarfs_writer for the writer API;
# both drag in the transitive closure (common, compressor, decompressor).
target_link_libraries(dwarfs_c PUBLIC dwarfs_reader dwarfs_writer)

target_include_directories(dwarfs_c PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(dwarfs_c PROPERTIES
  CXX_STANDARD ${DWARFS_CXX_STANDARD}
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  # Only the dwarfs_c_* API (marked DWARFS_C_API) is visible; all C++
  # internals dragged in from dwarfs/std headers are hidden.
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  VERSION ${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR}.${PRJ_VERSION_PATCH}
)

if(NOT STATIC_BUILD_DO_NOT_USE)
  install(
    TARGETS dwarfs_c
    EXPORT dwarfs-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

  # The ONLY consumer-facing header of the binding.
  install(FILES include/dwarfs_c.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
