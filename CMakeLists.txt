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

cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

# ============================================================================
# Boost: Use boost-cmake via CPM for ALL platforms
# ============================================================================
# vcpkg's boost-* ports don't work properly. We use ClausKlein/boost-cmake
# via CPM with GitHub shorthand format as documented in boost-cmake README.
# See: https://github.com/ClausKlein/boost-cmake
#
# Usage pattern from boost-cmake README:
#   set(BOOST_INCLUDE_LIBRARIES filesystem headers)
#   CPMAddPackage("gh:ClausKlein/boost-cmake#v1.87.0-rc6")
#   target_link_libraries(... PUBLIC Boost::headers)
include(cmake/CPM.cmake)

# Set Boost libraries before calling CPMAddPackage
set(BOOST_INCLUDE_LIBRARIES
    chrono
    context
    filesystem
    program_options
    regex
    system
    thread
    headers
)

# Use CPM with GitHub shorthand to add boost-cmake
# This will provide Boost::headers, Boost::filesystem, etc. targets
CPMAddPackage("gh:ClausKlein/boost-cmake#v1.87.0-rc6")

# Note: CMP0111 (imported targets from different directories) is now always NEW
# The old setting was needed for vcpkg + Folly compatibility but is deprecated

# ============================================================================
# vcpkg Integration for Static Builds
# ============================================================================

# Detect if vcpkg toolchain is active
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
  set(VCPKG_BUILD ON)
  message(STATUS "vcpkg build detected: ${CMAKE_TOOLCHAIN_FILE}")

  # Force static linking in vcpkg mode
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" CACHE STRING "Static library suffix" FORCE)

  message(STATUS "Static linking enabled (vcpkg mode)")
elseif(NOT DEFINED VCPKG_BUILD)
  # Only set VCPKG_BUILD to OFF if not already set via command line
  set(VCPKG_BUILD OFF)
  message(STATUS "Non-vcpkg build: using pkg-config or FetchContent")
endif()

option(WITH_LIBDWARFS "build with libdwarfs" ON)

# ============================================================================
# MinGW/MSYS-specific fixes
# ============================================================================

# ============================================================================
# MinGW/MSYS Deferred build.ninja Patching Function
# ============================================================================
# This function is called AFTER CMake finishes generating build.ninja
# It removes the --out-implib flag that causes WinMain linker errors.
# The flag must be removed from build.ninja because CMake's MinGW platform
# module adds it despite our settings, and it comes LAST in the link command,
# overriding earlier console subsystem flags.
function(deferred_mingw_patch_build_ninja)
  set(_build_ninja "${CMAKE_CURRENT_BINARY_DIR}/build.ninja")
  if(EXISTS "${_build_ninja}")
    message(STATUS "DEFERRED: Patching build.ninja to remove --out-implib flags...")
    file(READ "${_build_ninja}" _ninja_content)
    # Remove --out-implib flags (multiple patterns for robustness)
    string(REGEX REPLACE "-Wl,--out-implib,[^ \n]*" "" _ninja_content "${_ninja_content}")
    string(REGEX REPLACE "-Wl,--out-implib,[^\n]*" "" _ninja_content "${_ninja_content}")
    file(WRITE "${_build_ninja}" "${_ninja_content}")
    message(STATUS "DEFERRED: Successfully patched build.ninja (--out-implib flags removed)")
  else()
    message(WARNING "DEFERRED: build.ninja not found for patching")
  endif()
endfunction()

# For MinGW/MSYS, prevent CMake from adding --out-implib flag to linker
# This flag causes MinGW to use GUI startup files (crtexewin.o) expecting
# WinMain instead of console startup files expecting main()
#
# We do two things:
# 1. Add -mconsole to force console subsystem
# 2. Disable ENABLE_EXPORTS for all executable targets to prevent --out-implib
# See: https://stackoverflow.com/questions/34575066/how-to-prevent-cmake-from-issuing-implib
# Check if we're building for MinGW/MSYS (multiple detection methods for robustness)
# 1. Check if compiler is GCC and we're building for Windows
# 2. Check if vcpkg triplet is set and matches mingw/msys (CMAKE variable)
# 3. Check if vcpkg default triplet is set and matches mingw/msys (ENV variable)
set(_is_mingw_msys_build FALSE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_is_mingw_msys_build TRUE)
endif()
if(DEFINED VCPKG_TARGET_TRIPLET AND "${VCPKG_TARGET_TRIPLET}" MATCHES "mingw|msys")
  set(_is_mingw_msys_build TRUE)
endif()
if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND "$ENV{VCPKG_DEFAULT_TRIPLET}" MATCHES "mingw|msys")
  set(_is_mingw_msys_build TRUE)
endif()

if(_is_mingw_msys_build)
  message(STATUS "MinGW/MSYS build detected: applying MinGW-specific fixes...")
  message(STATUS "  CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
  message(STATUS "  CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
  message(STATUS "  VCPKG_TARGET_TRIPLET: ${VCPKG_TARGET_TRIPLET}")
  message(STATUS "  ENV{VCPKG_DEFAULT_TRIPLET}: $ENV{VCPKG_DEFAULT_TRIPLET}")

  # Add -mconsole to force console subsystem (prevents WinMain errors)
  # IMPORTANT: Must come BEFORE other flags to override GUI mode
  set(CMAKE_EXE_LINKER_FLAGS "-mconsole ${CMAKE_EXE_LINKER_FLAGS}" CACHE STRING "Executable linker flags" FORCE)
  message(STATUS "MinGW/MSYS build: added -mconsole to force console subsystem")

  # Set CMAKE_IMPORT_LIBRARY_SUFFIX to empty to prevent --out-implib flag
  # The --out-implib flag causes MinGW to use GUI startup files expecting WinMain
  # Must be set (not unset) to override vcpkg toolchain defaults
  set(CMAKE_IMPORT_LIBRARY_SUFFIX "" CACHE STRING "Disable import library generation for executables" FORCE)
  message(STATUS "MinGW/MSYS build: set CMAKE_IMPORT_LIBRARY_SUFFIX to empty to prevent --out-implib")

  # Disable automatic DLL import library creation
  set(CMAKE_GNULDLLTOOL TRUE CACHE BOOL "Use GNU DLL tool" FORCE)

  # Disable ENABLE_EXPORTS for all executable targets to prevent --out-implib
  # This must be done in a scope that runs after targets are defined
  function(fix_mingw_exe_exports target)
    # Disable ENABLE_EXPORTS to prevent CMake from adding --out-implib
    set_target_properties(${target} PROPERTIES ENABLE_EXPORTS FALSE)
    # Explicitly set WIN32_EXECUTABLE to FALSE to force console mode
    # This prevents CMake from treating the executable as a Windows GUI app
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE FALSE)
    # Explicitly set console subsystem and entry point to prevent WinMain errors
    target_link_options(${target} PRIVATE "LINKER:--subsystem,console" "LINKER:-e,mainCRTStartup")
  endfunction()

  # MSYS-specific: Iconv is builtin but FindIconv can't find it
  # Set Iconv variables to skip FindIconv check for MSYS
  if(MINGW AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Check if we're in MSYS environment (not pure MinGW)
    # Use multiple detection methods for robustness
    set(_is_msys FALSE)
    if(DEFINED ENV{MSYSTEM} AND "$ENV{MSYSTEM}" STREQUAL "MSYS")
      set(_is_msys TRUE)
    elseif(DEFINED VCPKG_CMAKE_SYSTEM_NAME AND VCPKG_CMAKE_SYSTEM_NAME STREQUAL "MSYS")
      set(_is_msys TRUE)
    endif()

    if(_is_msys)
      message(STATUS "MSYS environment detected: setting Iconv as builtin")
      # Tell CMake that Iconv is available in the C standard library
      set(Iconv_FOUND TRUE CACHE BOOL "Iconv found in MSYS C library" FORCE)
      set(Iconv_INCLUDE_DIR "" CACHE STRING "Iconv headers in standard include path" FORCE)
      set(Iconv_LIBRARY "" CACHE STRING "Iconv in C standard library" FORCE)
    endif()
  endif()
endif()

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
set(GOOGLETEST_PREFERRED_VERSION v1.17.0)

set(RANGE_V3_REQUIRED_VERSION 0.12.0)
set(RANGE_V3_PREFERRED_VERSION 0.12.0)

set(PARALLEL_HASHMAP_REQUIRED_VERSION 1.3.8)
set(PARALLEL_HASHMAP_PREFERRED_VERSION v1.3.12)

set(BOOST_REQUIRED_VERSION 1.80.0)
set(LIBCRYPTO_REQUIRED_VERSION 3.0.0)
set(LIBLZ4_REQUIRED_VERSION 1.9.3)
set(LIBLZMA_REQUIRED_VERSION 5.2.5)
set(LIBBROTLI_REQUIRED_VERSION 1.0.9)
set(LIBARCHIVE_REQUIRED_VERSION 3.6.0)
set(LIBMAGIC_REQUIRED_VERSION 5.38)
set(ZSTD_REQUIRED_VERSION 1.4.8)
set(XXHASH_REQUIRED_VERSION 0.8.1)
set(FLAC_REQUIRED_VERSION 1.4.2)
set(JEMALLOC_REQUIRED_VERSION 5.5.0)
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

include(cmake/compile.cmake)
include(cmake/version.cmake)

# Tebako integration point
if(DEFINED ENV{TEBAKO_BUILD} OR TEBAKO_BUILD)
  set(TEBAKO_BUILD ON CACHE BOOL "Enable tebako build mode")
  include(cmake/tebako.cmake)
endif()

if(NOT PRJ_VERSION_FULL)
  message(FATAL_ERROR "PRJ_VERSION_FULL is not set")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "cmake/modules")

find_package(PkgConfig)
include(cmake/need_fuse.cmake)

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
  include("cmake/ensure_mistletoe.cmake")
  include("cmake/manpage.cmake")
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

# Header-only libraries - vcpkg-aware
if(WITH_TESTS)
  include(cmake/vcpkg/gtest.cmake)
endif()

include(cmake/vcpkg/fmt.cmake)
include(cmake/vcpkg/range_v3.cmake)
include(cmake/vcpkg/json.cmake)
include(cmake/vcpkg/utf8cpp.cmake)

if(WITH_LIBDWARFS)
  include(cmake/vcpkg/phmap.cmake)

  # BZip2 must be found BEFORE Boost (required by boost-iostreams)
  include(cmake/vcpkg/bzip2.cmake)

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

  # ============================================================================
  # Dependency Discovery - Modular vcpkg-aware
  # ============================================================================

  # Standard libraries - always required
  include(cmake/vcpkg/openssl.cmake)
  include(cmake/vcpkg/libarchive.cmake)
  include(cmake/vcpkg/xxhash.cmake)
  include(cmake/vcpkg/zstd.cmake)

  # Compression libraries - MANDATORY for static builds
  include(cmake/vcpkg/lz4.cmake)
  include(cmake/vcpkg/lzma.cmake)
  include(cmake/vcpkg/brotli.cmake)
  include(cmake/vcpkg/flac.cmake)

  # Memory allocator - MANDATORY for static builds
  include(cmake/vcpkg/jemalloc.cmake)

  # mimalloc is alternative (mutually exclusive with jemalloc)
  if(USE_MIMALLOC)
    find_package(mimalloc ${MIMALLOC_REQUIRED_VERSION} REQUIRED CONFIG)
  endif()

  # Metadata serialization configuration - MUST BE FIRST
  # This defines DWARFS_WITH_EXPERIMENTAL_THRIFT option before we check it
  include(cmake/metadata_serialization.cmake)

  # folly and fbthrift - include AFTER metadata_serialization
  # These are only included when DWARFS_WITH_EXPERIMENTAL_THRIFT is enabled
  if(DWARFS_GIT_BUILD AND DWARFS_WITH_EXPERIMENTAL_THRIFT)
    # In git builds, we can build folly/fbthrift from submodules
    include(cmake/folly.cmake)
    include(cmake/thrift_library.cmake)
    include(cmake/thrift.cmake)
  endif()

  # If Experimental Thrift was enabled but targets don't exist yet (non-git build),
  # try to build from submodules as fallback
  if(DWARFS_WITH_EXPERIMENTAL_THRIFT AND NOT TARGET folly)
    include(cmake/folly.cmake)
    include(cmake/thrift_library.cmake)
    include(cmake/thrift.cmake)
    # Note: metadata_serialization.cmake already included above at line 306
    # No need to re-include it here as the detection logic will pick up the newly available targets
  endif()

  # ============================================================================
  # WORKAROUND: Fix CMake 4.x + vcpkg generator expression bug
  # ============================================================================
  # CMake 4.1.2 + vcpkg toolchain doesn't evaluate $<LINK_ONLY:Threads::Threads>
  # properly in link commands, causing literal generator expressions in link.txt
  # files. This workaround removes them after Threads is found (via Folly/FBThrift).
  #
  # See: Session 94 - Modern Thrift Testing
  # TODO: Remove when CMake/vcpkg is fixed upstream
  # NOTE: This must come AFTER the Folly/Thrift includes above (lines 302-304 or 339-341)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0" AND VCPKG_BUILD)
    message(STATUS "Applying CMake 4.x + vcpkg generator expression workaround...")
    # The issue is that vcpkg targets use $<LINK_ONLY:Threads::Threads> in their
    # INTERFACE_LINK_LIBRARIES, but CMake 4.x doesn't evaluate these properly.
    # We need to find and fix all vcpkg targets that have this issue.
    set(_fixed_count 0)
    get_cmake_property(_all_targets ALL_TARGETS)
    foreach(_target ${_all_targets})
      if(TARGET ${_target})
        get_target_property(_target_type ${_target} TYPE)
        if("${_target_type}" STREQUAL "INTERFACE_LIBRARY" OR "${_target_type}" STREQUAL "STATIC_LIBRARY")
          get_target_property(_link_libs ${_target} INTERFACE_LINK_LIBRARIES)
          if(_link_libs AND "${_link_libs}" MATCHES "\\$<LINK_ONLY:Threads::Threads>")
            # Remove generator expressions from INTERFACE_LINK_LIBRARIES
            string(REGEX REPLACE "\\$<LINK_ONLY:Threads::Threads>" "" _link_libs_clean "${_link_libs}")
            set_target_properties(${_target} PROPERTIES INTERFACE_LINK_LIBRARIES "${_link_libs_clean}")
            math(EXPR _fixed_count "${_fixed_count} + 1")
            message(STATUS "  Fixed: ${_target}")
          endif()
        endif()
      endif()
    endforeach()
    message(STATUS "Applied CMake 4.x generator expression workaround: fixed ${_fixed_count} targets")
  endif()
  # ============================================================================

  if(ENABLE_RICEPP)
    # TODO: support FetchContent
    add_subdirectory(ricepp)
  endif()

  include(cmake/libdwarfs.cmake)

  # Tool support library (CLI utilities for all tools)
  include(cmake/tool_support.cmake)

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
  if(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
    add_library(dwarfs_rewrite ALIAS dwarfs::dwarfs_rewrite)
  endif()
  add_library(dwarfs_extractor ALIAS dwarfs::dwarfs_extractor)
endif()

include(cmake/libdwarfs_tool.cmake)

# ============================================================================
# Include Modular CMake Files
# ============================================================================

if(WITH_TOOLS OR WITH_FUSE_DRIVER OR WITH_PXATTR OR WITH_EXAMPLE)
  include(cmake/tools.cmake)
endif()

if(WITH_TESTS OR WITH_BENCHMARKS OR WITH_FUZZ)
  include(cmake/tests.cmake)
endif()

# ============================================================================
# Homebrew Compatibility Tests
# ============================================================================

if(WITH_TESTS)
  option(WITH_COMPAT_TESTS "Enable Homebrew compatibility tests" ON)
  if(WITH_COMPAT_TESTS AND NOT WIN32)
    add_subdirectory(test/compat)
  endif()
endif()

include(cmake/compiler_flags.cmake)
include(cmake/packaging.cmake)

# ============================================================================
# FINAL WORKAROUND: Fix CMake 4.x + vcpkg generator expression bug
# ============================================================================
# This runs at the very end of configuration, after all vcpkg targets are created
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0" AND VCPKG_BUILD)
  message(STATUS "Applying FINAL CMake 4.x + vcpkg generator expression workaround...")
  set(_fixed_count 0)
  # List of known vcpkg targets that have generator expressions
  # (from grep results: libxml2, glog, liblzma, boost_atomic, boost_thread, etc.)
  set(_known_targets
    Boost::atomic Boost::thread Boost::chrono Boost::context
    glog::glog
    liblzma::liblzma
    LibXml2::LibXml2 Iconv::Iconv
    BZip2::BZip2 ZLIB::ZLIB
  )
  foreach(_target ${_known_targets})
    if(TARGET ${_target})
      # Skip ALIAS targets - they cannot have their properties modified
      get_target_property(_target_type ${_target} ALIAS_TARGET)
      if(_target_type)
        continue()
      endif()
      get_target_property(_link_libs ${_target} INTERFACE_LINK_LIBRARIES)
      if(_link_libs AND "${_link_libs}" MATCHES "\\$<LINK_ONLY:Threads::Threads>")
        # Remove generator expressions
        string(REGEX REPLACE "\\$<LINK_ONLY:Threads::Threads>" "" _link_libs_clean "${_link_libs}")
        set_target_properties(${_target} PROPERTIES INTERFACE_LINK_LIBRARIES "${_link_libs_clean}")
        math(EXPR _fixed_count "${_fixed_count} + 1")
        message(STATUS "  Fixed: ${_target}")
      endif()
    endif()
  endforeach()
  message(STATUS "FINAL workaround: fixed ${_fixed_count} targets")

  # Also patch the build.ninja file to remove problematic flags
  # This is needed because CMake 4.x doesn't evaluate generator expressions properly
  # and MinGW/MSYS need special handling to prevent WinMain errors
  set(_build_ninja "${CMAKE_CURRENT_BINARY_DIR}/build.ninja")
  if(EXISTS "${_build_ninja}")
    message(STATUS "Patching build.ninja to remove generator expressions and --out-implib...")
    execute_process(
      COMMAND sed -i.bak "s/\\$<LINK_ONLY:Threads::Threads>//g; s/-Wl,--out-implib,[^ ]*//g" "${_build_ninja}"
      RESULT_VARIABLE _sed_result
      ERROR_QUIET
    )
    if(_sed_result EQUAL 0)
      message(STATUS "  Successfully patched build.ninja")
    endif()
  endif()
endif()

# ============================================================================
# FINAL MinGW/MSYS FIX: Disable ENABLE_EXPORTS and force console subsystem
# ============================================================================
# This runs at the very end, after all targets are defined, to ensure
# ENABLE_EXPORTS is FALSE for all executable targets. This prevents CMake
# from adding --out-implib flag which causes MinGW to use GUI startup files.
# Check if we're building for MinGW/MSYS (multiple detection methods for robustness)
# Same detection logic as early section to ensure consistency
set(_is_final_mingw_msys_build FALSE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_is_final_mingw_msys_build TRUE)
endif()
if(DEFINED VCPKG_TARGET_TRIPLET AND "${VCPKG_TARGET_TRIPLET}" MATCHES "mingw|msys")
  set(_is_final_mingw_msys_build TRUE)
endif()
if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND "$ENV{VCPKG_DEFAULT_TRIPLET}" MATCHES "mingw|msys")
  set(_is_final_mingw_msys_build TRUE)
endif()

if(_is_final_mingw_msys_build)
  message(STATUS "Applying FINAL MinGW/MSYS ENABLE_EXPORTS fix...")
  message(STATUS "  CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
  message(STATUS "  CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
  message(STATUS "  VCPKG_TARGET_TRIPLET: ${VCPKG_TARGET_TRIPLET}")
  message(STATUS "  ENV{VCPKG_DEFAULT_TRIPLET}: $ENV{VCPKG_DEFAULT_TRIPLET}")
  set(_fixed_count 0)
  # Use BUILDSYSTEM_TARGETS to get all targets in the build system
  # ALL_TARGETS only gets targets in the current directory scope
  get_cmake_property(_all_targets BUILDSYSTEM_TARGETS)
  foreach(_target ${_all_targets})
    if(TARGET ${_target})
      get_target_property(_target_type ${_target} TYPE)
      if("${_target_type}" STREQUAL "EXECUTABLE")
        # Disable ENABLE_EXPORTS to prevent --out-implib
        set_target_properties(${_target} PROPERTIES ENABLE_EXPORTS FALSE)
        # Set WIN32_EXECUTABLE to FALSE to force console mode (not Windows GUI app)
        # This prevents CMake from treating the executable as a Windows GUI app
        set_target_properties(${_target} PROPERTIES WIN32_EXECUTABLE FALSE)
        # Add -mconsole as a compile option to force console mode
        target_compile_options(${_target} PRIVATE "-mconsole")
        # Explicitly set console subsystem at linker level
        target_link_options(${_target} PRIVATE "LINKER:--subsystem,console" "LINKER:-e,mainCRTStartup")
        math(EXPR _fixed_count "${_fixed_count} + 1")
      endif()
    endif()
  endforeach()
  message(STATUS "FINAL MinGW/MSYS fix: disabled ENABLE_EXPORTS and set console subsystem on ${_fixed_count} executable targets")

  # CRITICAL: Schedule deferred patching of build.ninja to remove --out-implib flag
  # CMake's MinGW platform module adds --out-implib despite ENABLE_EXPORTS=FALSE
  # This flag comes LAST in the link command and overrides console flags, causing WinMain errors
  # We use cmake_language(DEFER) to patch build.ninja AFTER CMake finishes generating it
  message(STATUS "Scheduling deferred build.ninja patching for MinGW/MSYS...")
  cmake_language(DEFER DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" CALL deferred_mingw_patch_build_ninja)
  message(STATUS "  Deferred patching will execute after CMake generates build.ninja")
endif()
