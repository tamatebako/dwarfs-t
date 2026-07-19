#
# Copyright (c) Ribose Inc.
#
# This file is part of dwarfs tebako integration.
#
# SPDX-License-Identifier: MIT
#

# Compiler Flags and Definitions
#
# Sets tebako-specific compile definitions based on platform


# Global tebako compile definitions
# These are required for proper operation with tebako's dependencies
add_compile_definitions(
  GLOG_USE_GLOG_EXPORT
  FOLLY_ASSUME_NO_TCMALLOC
)

message(STATUS "Added tebako compile definitions: GLOG_USE_GLOG_EXPORT, FOLLY_ASSUME_NO_TCMALLOC")

# Platform-specific compile definitions
if(IS_MSYS)
  add_compile_definitions(GFLAGS_IS_A_DLL=0)
  message(STATUS "MSYS: Added GFLAGS_IS_A_DLL=0")
endif()

if(IS_ALPINE)
  add_compile_definitions(__musl__)
  message(STATUS "Alpine: Added __musl__ definition")
endif()

# C++ filesystem compatibility check
# Some platforms need special handling for std::filesystem with std::u8string
include(CheckCXXSourceCompiles)

check_cxx_source_compiles(
  "#include <filesystem>
   int main() {
     std::filesystem::path p = \"\";
     std::u8string s;
     p = p / s;
     return 0;
   }"
  U8STRING_AND_PATH_OK
)

if(U8STRING_AND_PATH_OK)
  add_compile_definitions(U8STRING_AND_PATH_OK)
  message(STATUS "C++ filesystem supports u8string path operations")
endif()

# Additional platform-specific configurations
if(IS_MACOS)
  message(STATUS "macOS: No additional compile definitions needed")
endif()