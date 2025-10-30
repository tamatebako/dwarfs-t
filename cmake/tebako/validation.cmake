# Tebako validation module
# Validates DEPS directory structure, required tools, and platform compatibility

# Only run validation if tebako is enabled
if(NOT USE_TEBAKO)
  return()
endif()

message(STATUS "Tebako: Running validation checks...")

# ==============================================================================
# Platform compatibility check
# ==============================================================================

tebako_detect_platform()

if(NOT TEBAKO_PLATFORM_SUPPORTED)
  message(FATAL_ERROR "Tebako is not supported on this platform: ${CMAKE_SYSTEM_NAME}")
endif()

message(STATUS "Tebako: Platform ${TEBAKO_PLATFORM} is supported")

# ==============================================================================
# Build scope validation
# ==============================================================================

if(NOT DEFINED TEBAKO_BUILD_SCOPE)
  message(FATAL_ERROR "TEBAKO_BUILD_SCOPE must be set (ALL, MKD, or LIB)")
endif()

set(_valid_scopes "ALL" "MKD" "LIB")
if(NOT TEBAKO_BUILD_SCOPE IN_LIST _valid_scopes)
  message(FATAL_ERROR
    "Invalid TEBAKO_BUILD_SCOPE: ${TEBAKO_BUILD_SCOPE}. "
    "Must be one of: ${_valid_scopes}")
endif()

message(STATUS "Tebako: Build scope is ${TEBAKO_BUILD_SCOPE}")

# ==============================================================================
# DEPS directory validation
# ==============================================================================

if(NOT DEFINED TEBAKO_DEPS_DIR)
  message(WARNING
    "TEBAKO_DEPS_DIR is not set. "
    "Using default: ${CMAKE_SOURCE_DIR}/deps-${TEBAKO_BUILD_SCOPE}")
  set(TEBAKO_DEPS_DIR "${CMAKE_SOURCE_DIR}/deps-${TEBAKO_BUILD_SCOPE}")
endif()

# Convert to absolute path
get_filename_component(TEBAKO_DEPS_DIR "${TEBAKO_DEPS_DIR}" ABSOLUTE)

message(STATUS "Tebako: DEPS directory is ${TEBAKO_DEPS_DIR}")

# Check if DEPS directory exists
if(NOT EXISTS "${TEBAKO_DEPS_DIR}")
  message(WARNING
    "DEPS directory does not exist: ${TEBAKO_DEPS_DIR}\n"
    "This is normal for first-time builds.\n"
    "Dependencies will be built automatically.")
else()
  message(STATUS "Tebako: DEPS directory found")

  # Validate DEPS directory structure
  set(_required_subdirs "include" "lib")
  set(_missing_subdirs "")

  foreach(_subdir ${_required_subdirs})
    if(NOT EXISTS "${TEBAKO_DEPS_DIR}/${_subdir}")
      list(APPEND _missing_subdirs "${_subdir}")
    endif()
  endforeach()

  if(_missing_subdirs)
    message(WARNING
      "DEPS directory is incomplete. Missing subdirectories: ${_missing_subdirs}\n"
      "The directory may need to be rebuilt.")
  else()
    message(STATUS "Tebako: DEPS directory structure is valid")
  endif()
endif()

# ==============================================================================
# Required tools check
# ==============================================================================

message(STATUS "Tebako: Checking for required build tools...")

# Check for CMake minimum version
cmake_minimum_required(VERSION 3.24 FATAL_ERROR)
message(STATUS "Tebako: CMake version ${CMAKE_VERSION} >= 3.24 ✓")

# Check for C++ compiler
if(NOT CMAKE_CXX_COMPILER)
  message(FATAL_ERROR "C++ compiler not found")
endif()
message(STATUS "Tebako: C++ compiler: ${CMAKE_CXX_COMPILER}")

# Check for C compiler
if(NOT CMAKE_C_COMPILER)
  message(FATAL_ERROR "C compiler not found")
endif()
message(STATUS "Tebako: C compiler: ${CMAKE_C_COMPILER}")

# Check for pkg-config (helpful but not fatal)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  message(STATUS "Tebako: pkg-config found: ${PKG_CONFIG_EXECUTABLE}")
else()
  message(WARNING "Tebako: pkg-config not found (may need manual configuration)")
endif()

# Platform-specific tool checks
if(TEBAKO_PLATFORM STREQUAL "linux")
  # Check for patchelf on Linux (used by tebako)
  find_program(PATCHELF_EXECUTABLE patchelf)
  if(PATCHELF_EXECUTABLE)
    message(STATUS "Tebako: patchelf found: ${PATCHELF_EXECUTABLE}")
  else()
    message(WARNING
      "Tebako: patchelf not found. "
      "This may be required for some tebako operations on Linux.")
  endif()
elseif(TEBAKO_PLATFORM STREQUAL "macos")
  # Check for otool on macOS
  find_program(OTOOL_EXECUTABLE otool)
  if(OTOOL_EXECUTABLE)
    message(STATUS "Tebako: otool found: ${OTOOL_EXECUTABLE}")
  else()
    message(WARNING "Tebako: otool not found (usually provided by Xcode)")
  endif()
endif()

# ==============================================================================
# Compiler version checks
# ==============================================================================

message(STATUS "Tebako: Validating compiler versions...")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    message(WARNING
      "GCC version ${CMAKE_CXX_COMPILER_VERSION} is older than recommended (9.0+). "
      "Build may fail or have issues.")
  else()
    message(STATUS "Tebako: GCC version ${CMAKE_CXX_COMPILER_VERSION} ✓")
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
    message(WARNING
      "Clang version ${CMAKE_CXX_COMPILER_VERSION} is older than recommended (10.0+). "
      "Build may fail or have issues.")
  else()
    message(STATUS "Tebako: Clang version ${CMAKE_CXX_COMPILER_VERSION} ✓")
  endif()
endif()

# ==============================================================================
# C++ standard check
# ==============================================================================

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()

if(CMAKE_CXX_STANDARD LESS 20)
  message(WARNING
    "C++ standard is set to ${CMAKE_CXX_STANDARD}, but C++20 is recommended. "
    "Some features may not be available.")
else()
  message(STATUS "Tebako: C++ standard: C++${CMAKE_CXX_STANDARD} ✓")
endif()

# ==============================================================================
# System library availability checks
# ==============================================================================

message(STATUS "Tebako: Checking for system libraries...")

# These checks are informational - tebako will build its own if needed
set(_optional_libs
  "OpenSSL"
  "ZLIB"
  "BZip2"
  "LibLZMA"
  "ZSTD"
)

foreach(_lib ${_optional_libs})
  find_package(${_lib} QUIET)
  if(${_lib}_FOUND)
    message(STATUS "Tebako: System ${_lib} found (may be used or overridden)")
  else()
    message(STATUS "Tebako: System ${_lib} not found (will be built from source)")
  endif()
endforeach()

# ==============================================================================
# Diagnostic information
# ==============================================================================

function(tebako_print_diagnostics)
  message(STATUS "")
  message(STATUS "========================================")
  message(STATUS "Tebako Configuration Summary")
  message(STATUS "========================================")
  message(STATUS "Platform:           ${TEBAKO_PLATFORM}")
  message(STATUS "Build scope:        ${TEBAKO_BUILD_SCOPE}")
  message(STATUS "DEPS directory:     ${TEBAKO_DEPS_DIR}")
  message(STATUS "CMake version:      ${CMAKE_VERSION}")
  message(STATUS "C++ compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
  message(STATUS "C compiler:         ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
  message(STATUS "C++ standard:       C++${CMAKE_CXX_STANDARD}")
  message(STATUS "Build type:         ${CMAKE_BUILD_TYPE}")

  if(EXISTS "${TEBAKO_DEPS_DIR}")
    message(STATUS "DEPS status:        Exists")
  else()
    message(STATUS "DEPS status:        Will be created")
  endif()

  message(STATUS "========================================")
  message(STATUS "")
endfunction()

# ==============================================================================
# Validation helper functions
# ==============================================================================

function(tebako_require_program)
  set(options FATAL)
  set(oneValueArgs NAME VARIABLE)
  set(multiValueArgs HINTS)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  find_program(${ARG_VARIABLE} ${ARG_NAME} HINTS ${ARG_HINTS})

  if(${ARG_VARIABLE})
    message(STATUS "Tebako: Found ${ARG_NAME}: ${${ARG_VARIABLE}}")
  else()
    if(ARG_FATAL)
      message(FATAL_ERROR "Tebako: Required program not found: ${ARG_NAME}")
    else()
      message(WARNING "Tebako: Recommended program not found: ${ARG_NAME}")
    endif()
  endif()
endfunction()

function(tebako_check_deps_library)
  set(oneValueArgs NAME)
  cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

  if(NOT EXISTS "${TEBAKO_DEPS_DIR}")
    return()
  endif()

  # Check for library in DEPS
  file(GLOB _lib_files
    "${TEBAKO_DEPS_DIR}/lib/lib${ARG_NAME}.*"
    "${TEBAKO_DEPS_DIR}/lib64/lib${ARG_NAME}.*"
  )

  if(_lib_files)
    message(STATUS "Tebako: Library ${ARG_NAME} found in DEPS")
  else()
    message(STATUS "Tebako: Library ${ARG_NAME} not found in DEPS (will be built)")
  endif()
endfunction()

# ==============================================================================
# Export validation results
# ==============================================================================

set(TEBAKO_VALIDATION_PASSED TRUE CACHE BOOL "Tebako validation status" FORCE)

# Print diagnostics
tebako_print_diagnostics()

message(STATUS "Tebako: Validation complete ✓")