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

# DwarFS Tool Support Library
# CLI utilities and handlers for mkdwarfs, dwarfs, dwarfsck, dwarfsextract

message(STATUS "Configuring tool support library...")

# Include argtable3 dependency
include(${CMAKE_SOURCE_DIR}/cmake/vcpkg/argtable3.cmake)

# Only build if WITH_LIBDWARFS is enabled
if(NOT WITH_LIBDWARFS)
  message(STATUS "Tool support library: DISABLED (WITH_LIBDWARFS=OFF)")
  return()
endif()

# Define the tool support library
add_library(dwarfs_tool_support STATIC
  # Core tool utilities (always required)
  ${CMAKE_SOURCE_DIR}/tools/src/tool/main_adapter.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/safe_main.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/iolayer.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/sys_char.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/sysinfo.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/tool.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/pager.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/render_manpage.cpp

  # Session 50: argtable3 infrastructure
  ${CMAKE_SOURCE_DIR}/tools/src/tool/version_info.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/argtable3_base_parser.cpp

  # mkdwarfs-specific handlers (always included)
  ${CMAKE_SOURCE_DIR}/tools/src/mkdwarfs/argtable3_options_parser.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/mkdwarfs/handler_factory.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/mkdwarfs/create_handler.cpp

  # Recompress handler - only if Thrift available
  $<$<BOOL:${DWARFS_HAVE_EXPERIMENTAL_THRIFT}>:${CMAKE_SOURCE_DIR}/tools/src/mkdwarfs/recompress_handler.cpp>

  # dwarfsck-specific handlers (always included)
  ${CMAKE_SOURCE_DIR}/tools/src/dwarfsck/argtable3_options_parser.cpp

  # dwarfsextract-specific handlers (always included)
  ${CMAKE_SOURCE_DIR}/tools/src/dwarfsextract/argtable3_options_parser.cpp

  # dwarfs FUSE driver handlers - only if FUSE driver enabled
  $<$<BOOL:${WITH_FUSE_DRIVER}>:${CMAKE_SOURCE_DIR}/tools/src/dwarfs/argtable3_options_parser.cpp>
  $<$<BOOL:${WITH_FUSE_DRIVER}>:${CMAKE_SOURCE_DIR}/tools/src/dwarfs/mount_handler.cpp>
)

# Public headers for tools
target_include_directories(dwarfs_tool_support
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tools/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Link required libraries
target_link_libraries(dwarfs_tool_support
  PUBLIC
    dwarfs_common
    dwarfs_reader
    dwarfs_writer
    argtable3::argtable3
)

# Link optional dependencies if available
if(TARGET Boost::process)
  target_link_libraries(dwarfs_tool_support PUBLIC Boost::process)
endif()

if(DWARFS_HAVE_EXPERIMENTAL_THRIFT AND TARGET dwarfs_rewrite)
  target_link_libraries(dwarfs_tool_support PUBLIC dwarfs_rewrite)
endif()

# Set target properties
if(NOT STATIC_BUILD_DO_NOT_USE)
  set_target_properties(dwarfs_tool_support
    PROPERTIES VERSION ${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR}.${PRJ_VERSION_PATCH})
endif()

# Compiler definitions (matching dwarfs_tool target configuration)
target_compile_definitions(dwarfs_tool_support PRIVATE
  DWARFS_BUILD_ID="${CMAKE_SYSTEM_PROCESSOR} ${CMAKE_SYSTEM_NAME} using ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)

# Memory allocator definitions (conditional, matching dwarfs_tool)
if(USE_JEMALLOC AND TARGET jemalloc::jemalloc)
  target_link_libraries(dwarfs_tool_support PRIVATE jemalloc::jemalloc)
  target_compile_definitions(dwarfs_tool_support PRIVATE DWARFS_USE_JEMALLOC)
endif()

if(USE_MIMALLOC AND mimalloc_FOUND)
  target_link_libraries(dwarfs_tool_support PRIVATE mimalloc-static)
  target_compile_definitions(dwarfs_tool_support PRIVATE DWARFS_USE_MIMALLOC)
endif()

# FUSE driver compile definitions (if FUSE driver handlers are included)
if(WITH_FUSE_DRIVER)
  # Ensure FUSE is detected if not already done
  if(NOT FUSE_FOUND AND NOT FUSE3_FOUND AND NOT WINFSP)
    include(${CMAKE_SOURCE_DIR}/cmake/need_fuse.cmake)
  endif()

  target_compile_definitions(dwarfs_tool_support PRIVATE _FILE_OFFSET_BITS=64)

  # Detect FUSE implementation and set appropriate version
  if(APPLE AND FUSE_IMPLEMENTATION STREQUAL "fuse-t")
    # FUSE-T uses FUSE2 API with headers at fuse/fuse.h
    # The DWARFS_USE_FUSE_T define triggers special include path handling in fuse_driver.cpp
    target_compile_definitions(dwarfs_tool_support PRIVATE
      FUSE_USE_VERSION=29
      DWARFS_USE_FUSE_T
    )
    # FUSE_T_INCLUDE_DIR is set by need_fuse.cmake
    if(FUSE_T_INCLUDE_DIR)
      target_include_directories(dwarfs_tool_support BEFORE PRIVATE "${FUSE_T_INCLUDE_DIR}")
    endif()
  elseif(FUSE3_FOUND)
    target_compile_definitions(dwarfs_tool_support PRIVATE FUSE_USE_VERSION=35)
  elseif(FUSE_FOUND)
    target_compile_definitions(dwarfs_tool_support PRIVATE FUSE_USE_VERSION=29)
  elseif(WIN32 AND WINFSP)
    target_compile_definitions(dwarfs_tool_support PRIVATE
      FUSE_USE_VERSION=32
      DWARFS_FUSE_LOWLEVEL=0
    )
  endif()
endif()

# Installation
if(NOT STATIC_BUILD_DO_NOT_USE)
  install(
    TARGETS dwarfs_tool_support
    EXPORT dwarfs-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

  # Install tool support headers (already handled by libdwarfs.cmake install rules)
  # Headers are installed via: install(DIRECTORY tools/include/dwarfs ...)
endif()

message(STATUS "Tool support library: ENABLED")
message(STATUS "  - Core utilities: YES")
message(STATUS "  - mkdwarfs handlers: YES")
message(STATUS "  - Recompress handler: ${DWARFS_HAVE_EXPERIMENTAL_THRIFT}")
message(STATUS "  - FUSE driver handlers: ${WITH_FUSE_DRIVER}")

# ============================================================================
# Manpage Generation (if building from git repo)
# ============================================================================

if(WITH_MAN_OPTION AND DWARFS_GIT_BUILD)
  message(STATUS "Generating manpages for tool_support library...")

  # Ensure Python and mistletoe are available
  include(${CMAKE_SOURCE_DIR}/cmake/ensure_mistletoe.cmake)

  include(${CMAKE_SOURCE_DIR}/cmake/render_manpage.cmake)

  # Create output directory for generated manpages
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tools/src")

  # Generate manpages for all tools
  foreach(man mkdwarfs dwarfs dwarfsextract)
    if(EXISTS ${CMAKE_SOURCE_DIR}/doc/${man}.md)
      add_manpage_source(doc/${man}.md
        NAME ${man}
        OUTPUT ${CMAKE_BINARY_DIR}/tools/src/${man}_manpage.cpp
      )

      # Add generated manpage to library sources
      target_sources(dwarfs_tool_support PRIVATE
        ${CMAKE_BINARY_DIR}/tools/src/${man}_manpage.cpp
      )

      message(STATUS "  - Generated: ${man}_manpage.cpp")
    endif()
  endforeach()

  message(STATUS "Manpage generation: COMPLETE")
elseif(WITH_MAN_OPTION AND NOT DWARFS_GIT_BUILD)
  message(STATUS "Manpage generation: SKIPPED (tarball build, using pre-generated)")

  # Use pre-generated manpages from source tree
  foreach(man mkdwarfs dwarfs dwarfsck dwarfsextract)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tools/src/${man}_manpage.cpp)
      target_sources(dwarfs_tool_support PRIVATE
        ${CMAKE_SOURCE_DIR}/tools/src/${man}_manpage.cpp
      )
      message(STATUS "  - Using pre-generated: ${man}_manpage.cpp")
    endif()
  endforeach()
else()
  message(STATUS "Manpage generation: DISABLED (WITH_MAN_OPTION=OFF)")
endif()