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

#
# VCPKG-ONLY FBTHRIFT CONFIGURATION
#
# This file enforces use of FBThrift from vcpkg overlay ports (Tebako fork).
# Submodule-based builds are NO LONGER SUPPORTED.
#

# Enforce vcpkg toolchain
if(NOT VCPKG_TOOLCHAIN AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR
    "FBThrift MUST be provided by vcpkg. "
    "Set CMAKE_TOOLCHAIN_FILE to vcpkg.cmake or use -DCMAKE_TOOLCHAIN_FILE=\${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  )
endif()

# Apple-specific: Ensure OpenSSL is found
if(APPLE)
  find_package(OpenSSL 1.1.1 MODULE REQUIRED)
endif()

# Find FBThrift from vcpkg
find_package(FBThrift CONFIG REQUIRED)

message(STATUS "Using vcpkg-provided FBThrift library")

# Verify FBThrift::thrift target exists
if(NOT TARGET FBThrift::thrift)
  message(FATAL_ERROR "FBThrift::thrift target not found. Ensure vcpkg overlay ports are configured correctly.")
endif()

# Locate thrift1 compiler for code generation
# Priority: vcpkg tools → Homebrew → system PATH
find_program(THRIFT1_COMPILER
  NAMES thrift1
  PATHS
    # vcpkg locations (highest priority)
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/fbthrift
    ${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/fbthrift
    # Homebrew locations (macOS)
    /opt/homebrew/bin
    /usr/local/bin
  NO_DEFAULT_PATH
)

# Fallback to system PATH if not found above
if(NOT THRIFT1_COMPILER)
  find_program(THRIFT1_COMPILER thrift1)
endif()

if(THRIFT1_COMPILER)
  message(STATUS "Found thrift1 compiler: ${THRIFT1_COMPILER}")
else()
  message(FATAL_ERROR
    "thrift1 compiler not found! "
    "Ensure fbthrift is installed via vcpkg overlay ports or available in PATH."
  )
endif()

# Export for use in thrift_library.cmake
set(THRIFT1_COMPILER ${THRIFT1_COMPILER} CACHE FILEPATH "Path to thrift1 compiler" FORCE)
