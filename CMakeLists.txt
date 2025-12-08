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

# Conditional minimum version for tebako compatibility
if(DEFINED ENV{TEBAKO_BUILD} OR TEBAKO_BUILD)
  cmake_minimum_required(VERSION 3.24.0)
else()
  cmake_minimum_required(VERSION 3.28.0)
endif()

# Enable CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0091 NEW)

# Use Boost's CMake package instead of FindBoost when in tebako mode
if((DEFINED ENV{TEBAKO_BUILD} OR TEBAKO_BUILD) AND POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(dwarfs LANGUAGES CXX C)

include(ExternalProject)
include(CheckCXXSourceCompiles)

option(WITH_LIBDWARFS "build with libdwarfs" ON)
option(WITH_TOOLS "build with tools" ON)
option(WITH_FUSE_DRIVER "build with FUSE driver" ON)
option(WITH_DESKTOP_INTEGRATION "install mimetype and desktop integration" ON)
option(WITH_TESTS "build with tests" OFF)
option(WITH_BENCHMARKS "build with benchmarks" OFF)
option(WITH_ALL_BENCHMARKS "build with *all* benchmarks" OFF)
option(WITH_FUZZ "build with fuzzing binaries" OFF)
option(WITH_MAN_OPTION "build with --man option" ON)
option(ENABLE_PERFMON "enable performance monitor in all tools" ON)
option(TRY_ENABLE_FLAC "build with FLAC support" ON)
option(ENABLE_RICEPP "build with RICEPP compression support" ON)
option(TRY_ENABLE_BROTLI "build with BROTLI compression support" ON)
option(TRY_ENABLE_LZ4 "build with LZ4 compression support" ON)
option(TRY_ENABLE_LZMA "build with LZMA compression support" ON)
option(WITH_UNIVERSAL_BINARY "build with universal binary" OFF)
option(WITH_FUSE_EXTRACT_BINARY "build with fuse-extract binary" OFF)
option(WITH_PXATTR "build with pxattr binary" OFF)
option(WITH_EXAMPLE "build with example binary" OFF)
option(WITH_DEV_TOOLS "build with developer tools" OFF)
option(ENABLE_STACKTRACE "build with stack trace support" OFF)
option(DISABLE_FILESYSTEM_EXTRACTOR_FORMAT "disable filesystem extractor format support" OFF)
option(BUILD_FSST_MAIN "build fsst_main application (dev-only)" OFF)
option(USE_SUBDIR_LIBARCHIVE "use local libarchive subdir (dev-only)" OFF)
if(APPLE)
  option(USE_HOMEBREW_LIBARCHIVE "use libarchive from homebrew" ON)
endif()
if(NOT WIN32)
  option(WITH_LEGACY_FUSE "build fuse2 driver even if we have fuse3" OFF)
  option(WITH_MAN_PAGES "build man pages using ronn" ON)
  option(ENABLE_ASAN "enable address sanitizer" OFF)
  option(ENABLE_TSAN "enable thread sanitizer" OFF)
  option(ENABLE_UBSAN "enable undefined behaviour sanitizer" OFF)
  option(ENABLE_COVERAGE "enable code coverage" OFF)
  option(USE_JEMALLOC "build with jemalloc if available" ON)
  option(USE_MIMALLOC "build with mimalloc if available" OFF)
  option(PREFER_SYSTEM_FAST_FLOAT "use system fast_float if available" OFF)
  option(PREFER_SYSTEM_GTEST "use system gtest if available" OFF)
  option(DISABLE_CCACHE "disable ccache" OFF)
  option(DISABLE_MOLD "disable mold" OFF)
  option(STATIC_BUILD_DO_NOT_USE "try static build (experimental)" OFF)
endif()

if(STATIC_BUILD_DO_NOT_USE AND NOT(WITH_LIBDWARFS AND WITH_TOOLS AND WITH_FUSE_DRIVER))
  message(FATAL_ERROR "STATIC_BUILD_DO_NOT_USE requires WITH_LIBDWARFS, WITH_TOOLS and WITH_FUSE_DRIVER")
endif()

if(USE_JEMALLOC AND USE_MIMALLOC)
  message(FATAL_ERROR "USE_JEMALLOC and USE_MIMALLOC are mutually exclusive")
endif()

if(WITH_ALL_BENCHMARKS)
  set(WITH_BENCHMARKS ON)
endif()

if(NOT WIN32)
  set(BASH_INSTALL_PATH "bash-completion/completions" CACHE STRING "Bash completion install dir")
  set(ZSH_INSTALL_PATH "zsh/site-functions" CACHE STRING "Zsh completion install dir")
endif()

set(DWARFS_CXX_STANDARD 20)

# Libraries that we can fetch on demand if necessary
#
# All of these libraries are header-only and not strictly required once
# libdwarfs is built. Only the range-v3 library is used to implement
# string splitting, but there's plenty of alternatives for that.
#
# We only use these libraries for building the binary targets, we won't
# install them.

set(LIBFMT_REQUIRED_VERSION 10.0)
set(LIBFMT_PREFERRED_VERSION 12.0.0)

set(GOOGLETEST_REQUIRED_VERSION 1.13.0)
set(GOOGLETEST_PREFERRED_VERSION 1.17.0)

set(RANGE_V3_REQUIRED_VERSION 0.12.0)
set(RANGE_V3_PREFERRED_VERSION 0.12.0)

set(PARALLEL_HASHMAP_REQUIRED_VERSION 1.3.8)
set(PARALLEL_HASHMAP_PREFERRED_VERSION 2.0.0)

set(BOOST_REQUIRED_VERSION 1.67.0)
set(LIBCRYPTO_REQUIRED_VERSION 3.0.0)
set(LIBLZ4_REQUIRED_VERSION 1.9.3)
set(LIBLZMA_REQUIRED_VERSION 5.2.5)
set(LIBBROTLI_REQUIRED_VERSION 1.0.9)
set(LIBARCHIVE_REQUIRED_VERSION 3.6.0)
set(LIBMAGIC_REQUIRED_VERSION 5.38)
set(ZSTD_REQUIRED_VERSION 1.4.8)
set(XXHASH_REQUIRED_VERSION 0.8.1)
set(FLAC_REQUIRED_VERSION 1.4.2)
set(JEMALLOC_REQUIRED_VERSION 5.2.1)
set(MIMALLOC_REQUIRED_VERSION 2.0.0)

if(DEFINED ENV{DWARFS_LOCAL_REPO_PATH})
  set(LIBFMT_GIT_REPO $ENV{DWARFS_LOCAL_REPO_PATH}/fmt)
  set(GOOGLETEST_GIT_REPO $ENV{DWARFS_LOCAL_REPO_PATH}/googletest)
  set(RANGE_V3_GIT_REPO $ENV{DWARFS_LOCAL_REPO_PATH}/range-v3)
  set(PARALLEL_HASHMAP_GIT_REPO $ENV{DWARFS_LOCAL_REPO_PATH}/parallel-hashmap)
  set(JEMALLOC_GIT_REPO $ENV{DWARFS_LOCAL_REPO_PATH}/jemalloc)
else()
  set(LIBFMT_GIT_REPO https://github.com/fmtlib/fmt.git)
  set(GOOGLETEST_GIT_REPO https://github.com/google/googletest.git)
  set(RANGE_V3_GIT_REPO https://github.com/ericniebler/range-v3.git)
  set(PARALLEL_HASHMAP_GIT_REPO https://github.com/greg7mdp/parallel-hashmap.git)
  set(JEMALLOC_GIT_REPO https://github.com/tamatebako/jemalloc.git)
endif()

if(APPLE)
  set(HOMEBREW_PREFIX_PATH "/opt/homebrew")
  set(TRY_RUN_INCLUDE_DIRECTORIES ${HOMEBREW_PREFIX_PATH}/include)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/compile.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/version.cmake)

# Tebako integration point
if(DEFINED ENV{TEBAKO_BUILD} OR TEBAKO_BUILD)
  set(TEBAKO_BUILD ON CACHE BOOL "Enable tebako build mode")
  include(${CMAKE_SOURCE_DIR}/cmake/tebako.cmake)
endif()

if(NOT PRJ_VERSION_FULL)
  message(FATAL_ERROR "PRJ_VERSION_FULL is not set")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")

find_package(PkgConfig)

if(STATIC_BUILD_DO_NOT_USE)
  set(CMAKE_FIND_LIBRARY_SUFFIXES
      ".a"
      CACHE STRING "please look for static libs")
  set(Boost_USE_STATIC_LIBS
      ON
      CACHE BOOL "only static boost libs")
  set(BOOST_LINK_STATIC
      "ON"
      CACHE STRING "yes, really")
  set(USE_STATIC_DEPS_ON_UNIX
      ON
      CACHE BOOL "yes")
  set(GFLAGS_SHARED
      OFF
      CACHE BOOL "static")

  set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
  list(APPEND PKG_CONFIG_EXECUTABLE "--static")
endif()

if(WITH_MAN_PAGES)
  include("${CMAKE_SOURCE_DIR}/cmake/manpage.cmake")
  if(WITH_LIBDWARFS)
    list(APPEND DWARFS_MANPAGES dwarfs-format.5 dwarf-env.7)
  endif()
  if(WITH_TOOLS)
    list(APPEND DWARFS_MANPAGES mkdwarfs.1 dwarfsck.1 dwarfsextract.1)
  endif()
  if(WITH_FUSE_DRIVER)
    list(APPEND DWARFS_MANPAGES dwarfs.1)
  endif()
  foreach(man ${DWARFS_MANPAGES})
    add_manpage(${man})
  endforeach()
endif()

include(FetchContent)
include(GNUInstallDirs)

if(WITH_TESTS)
  include(${CMAKE_SOURCE_DIR}/cmake/need_gtest.cmake)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/need_fmt.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/need_range_v3.cmake)

if(WITH_LIBDWARFS)
  include(${CMAKE_SOURCE_DIR}/cmake/need_phmap.cmake)

  find_package(Boost ${BOOST_REQUIRED_VERSION} REQUIRED CONFIG
               COMPONENTS chrono program_options
               OPTIONAL_COMPONENTS process)

  if(APPLE)
    if(USE_HOMEBREW_LIBARCHIVE)
      find_program(HOMEBREW_EXE brew)
      execute_process(
        COMMAND ${HOMEBREW_EXE} --prefix libarchive
        OUTPUT_VARIABLE LIBARCHIVE_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      list(PREPEND CMAKE_PREFIX_PATH ${LIBARCHIVE_PREFIX})
    endif()
  endif()

  if(ENABLE_STACKTRACE)
    find_package(cpptrace REQUIRED CONFIG)
  endif()

  pkg_check_modules(LIBCRYPTO REQUIRED IMPORTED_TARGET libcrypto>=${LIBCRYPTO_REQUIRED_VERSION})
  if(USE_SUBDIR_LIBARCHIVE)
    add_compile_definitions(LIBARCHIVE_STATIC)
    add_subdirectory(libarchive EXCLUDE_FROM_ALL)
    add_library(PkgConfig::LIBARCHIVE ALIAS archive_static)
  else()
    pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive>=${LIBARCHIVE_REQUIRED_VERSION})
  endif()
  pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash>=${XXHASH_REQUIRED_VERSION})
  pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd>=${ZSTD_REQUIRED_VERSION})
  if(TRY_ENABLE_LZ4)
    pkg_check_modules(LIBLZ4 IMPORTED_TARGET liblz4>=${LIBLZ4_REQUIRED_VERSION})
  endif()
  if(TRY_ENABLE_LZMA)
    pkg_check_modules(LIBLZMA IMPORTED_TARGET liblzma>=${LIBLZMA_REQUIRED_VERSION})
  endif()
  if(TRY_ENABLE_BROTLI)
    pkg_check_modules(LIBBROTLIDEC IMPORTED_TARGET libbrotlidec>=${LIBBROTLI_REQUIRED_VERSION})
    pkg_check_modules(LIBBROTLIENC IMPORTED_TARGET libbrotlienc>=${LIBBROTLI_REQUIRED_VERSION})
  endif()
  # pkg_check_modules(LIBMAGIC IMPORTED_TARGET libmagic>=${LIBMAGIC_REQUIRED_VERSION})
  if(TRY_ENABLE_FLAC)
    pkg_check_modules(FLAC IMPORTED_TARGET flac++>=${FLAC_REQUIRED_VERSION})
  endif()

  if(USE_JEMALLOC)
    # Use tamatebako jemalloc fork which supports Windows ARM64, musl, and more platforms
    include(${CMAKE_SOURCE_DIR}/cmake/need_jemalloc.cmake)
  endif()

  if(USE_MIMALLOC)
    find_package(mimalloc ${MIMALLOC_REQUIRED_VERSION} REQUIRED CONFIG)
  endif()

  # folly and fbthrift - include FIRST so targets exist
  # These are optional but must be available before metadata_serialization checks them
  if(DWARFS_GIT_BUILD AND (NOT DEFINED DWARFS_WITH_THRIFT OR DWARFS_WITH_THRIFT))
    # In git builds, we can build folly/fbthrift from submodules
    # Only include if Thrift hasn't been explicitly disabled
    include(${CMAKE_SOURCE_DIR}/cmake/folly.cmake)
    include(${CMAKE_SOURCE_DIR}/cmake/thrift_library.cmake)
    include(${CMAKE_SOURCE_DIR}/cmake/thrift.cmake)
  endif()

  # Metadata serialization configuration
  # NOW checks for folly/thrift targets (created above)
  include(${CMAKE_SOURCE_DIR}/cmake/metadata_serialization.cmake)

  # If Thrift was enabled but targets don't exist yet (non-git build),
  # try to build from submodules as fallback
  if(DWARFS_WITH_THRIFT AND NOT TARGET folly)
    include(${CMAKE_SOURCE_DIR}/cmake/folly.cmake)
    include(${CMAKE_SOURCE_DIR}/cmake/thrift_library.cmake)
    include(${CMAKE_SOURCE_DIR}/cmake/thrift.cmake)
    # Re-check if Thrift is now available
    include(${CMAKE_SOURCE_DIR}/cmake/metadata_serialization.cmake)
  endif()

  if(ENABLE_RICEPP)
    # TODO: support FetchContent
    add_subdirectory(ricepp)
  endif()

  include(${CMAKE_SOURCE_DIR}/cmake/libdwarfs.cmake)

  set(DWARFS_HAVE_CPPTRACE ${cpptrace_FOUND})
  set(DWARFS_HAVE_LIBZSTD ON)
  set(DWARFS_HAVE_RICEPP ${ENABLE_RICEPP})
  set(DWARFS_HAVE_LIBMAGIC ${LIBMAGIC_FOUND})
  set(DWARFS_HAVE_LIBLZ4 ${LIBLZ4_FOUND})
  set(DWARFS_HAVE_LIBLZMA ${LIBLZMA_FOUND})
  if(${LIBBROTLIDEC_FOUND} AND ${LIBBROTLIENC_FOUND})
    set(DWARFS_HAVE_LIBBROTLI ON)
  else()
    set(DWARFS_HAVE_LIBBROTLI OFF)
  endif()
  set(DWARFS_HAVE_FLAC ${FLAC_FOUND})
  set(DWARFS_BUILTIN_MANPAGE ${WITH_MAN_OPTION})
  set(DWARFS_PERFMON_ENABLED ${ENABLE_PERFMON})
  set(DWARFS_STACKTRACE_ENABLED ${ENABLE_STACKTRACE})
  set(DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT ${DISABLE_FILESYSTEM_EXTRACTOR_FORMAT})

  # Set FUSE-T flag if using FUSE-T on macOS
  if(APPLE AND FUSE_IMPLEMENTATION STREQUAL "fuse-t")
    set(DWARFS_USE_FUSE_T ON)
  endif()

  configure_file(cmake/config.h.in include/dwarfs/config.h @ONLY)
else()
  find_package(dwarfs ${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR}.${PRJ_VERSION_PATCH} EXACT REQUIRED CONFIG)
  add_library(dwarfs_common ALIAS dwarfs::dwarfs_common)
  add_library(dwarfs_reader ALIAS dwarfs::dwarfs_reader)
  add_library(dwarfs_writer ALIAS dwarfs::dwarfs_writer)
  if(DWARFS_HAVE_THRIFT)
    add_library(dwarfs_rewrite ALIAS dwarfs::dwarfs_rewrite)
  endif()
  add_library(dwarfs_extractor ALIAS dwarfs::dwarfs_extractor)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/libdwarfs_tool.cmake)

# ============================================================================
# Include Modular CMake Files
# ============================================================================

if(WITH_TOOLS OR WITH_FUSE_DRIVER OR WITH_PXATTR OR WITH_EXAMPLE)
  include(${CMAKE_SOURCE_DIR}/cmake/tools.cmake)
endif()

if(WITH_TESTS OR WITH_BENCHMARKS OR WITH_FUZZ)
  include(${CMAKE_SOURCE_DIR}/cmake/tests.cmake)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/compiler_flags.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/packaging.cmake)
