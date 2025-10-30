#
# Copyright (c) Ribose Inc.
#
# This file is part of dwarfs tebako integration.
#
# SPDX-License-Identifier: MIT
#

# Platform Detection for Tebako
#
# Detects the operating system and sets platform-specific flags
# Uses bash to query $OSTYPE for Unix-like systems

cmake_minimum_required(VERSION 3.24.0)

# Default bash path
set(GNU_BASH "bash" CACHE STRING "Path to bash executable")

# Initialize platform flags
set(IS_MSYS OFF CACHE BOOL "Building on MSYS/MinGW")
set(IS_ALPINE OFF CACHE BOOL "Building on Alpine Linux (musl)")
set(IS_MACOS OFF CACHE BOOL "Building on macOS")

# Platform detection
if(MSVC)
  set(OSTYPE_TXT "Windows")
  message(STATUS "Platform: Windows (MSVC)")

elseif(MINGW)
  set(OSTYPE_TXT "msys")
  set(IS_MSYS ON CACHE BOOL "Building on MSYS/MinGW" FORCE)
  message(STATUS "Platform: Windows (MSYS/MinGW)")

else()
  # Unix-like system - query $OSTYPE via bash
  execute_process(
    COMMAND "${GNU_BASH}" "-c" "echo $OSTYPE"
    RESULT_VARIABLE OSTYPE_RES
    OUTPUT_VARIABLE OSTYPE_TXT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )

  if(NOT OSTYPE_RES EQUAL 0)
    message(WARNING "Failed to detect OSTYPE via bash, using CMAKE_SYSTEM_NAME")
    set(OSTYPE_TXT ${CMAKE_SYSTEM_NAME})
  endif()

  message(STATUS "Platform OSTYPE: ${OSTYPE_TXT}")

  # Set specific platform flags based on OSTYPE
  if(OSTYPE_TXT MATCHES "^linux-musl.*")
    set(IS_ALPINE ON CACHE BOOL "Building on Alpine Linux" FORCE)
    message(STATUS "Platform: Alpine Linux (musl libc)")

  elseif(OSTYPE_TXT MATCHES "^darwin.*")
    set(IS_MACOS ON CACHE BOOL "Building on macOS" FORCE)
    message(STATUS "Platform: macOS")

  elseif(OSTYPE_TXT MATCHES "^linux.*")
    message(STATUS "Platform: Linux (glibc)")

  else()
    message(STATUS "Platform: ${OSTYPE_TXT}")
  endif()
endif()

# Export OSTYPE_TXT for other modules
set(OSTYPE_TXT "${OSTYPE_TXT}" CACHE STRING "Operating system type" FORCE)