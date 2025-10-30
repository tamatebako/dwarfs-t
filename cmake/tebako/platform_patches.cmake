#
# Copyright (c) Ribose Inc.
#
# This file is part of dwarfs tebako integration.
#
# SPDX-License-Identifier: MIT
#

# Platform Patches Module
#
# Applies platform-specific patches to folly, fbthrift, and zstd
# when building with tebako. These patches are needed for:
# - Alpine/musl compatibility
# - MSYS/Windows compatibility
# - macOS compatibility

cmake_minimum_required(VERSION 3.24.0)

# Check if NO_PATCH flag is set
if(DEFINED NO_PATCH AND NO_PATCH)
  message(STATUS "Platform patches disabled by NO_PATCH flag")
  return()
endif()

# Verify TOOLS directory exists
if(NOT EXISTS "${TOOLS}")
  message(WARNING "TOOLS directory not found at: ${TOOLS}")
  message(WARNING "Platform patches will be skipped")
  return()
endif()

# Set up patch script paths
set(PATCH_SCRIPTS_DIR "${TOOLS}/ci-scripts")
set(PATCH_FOLLY_SCRIPT "${PATCH_SCRIPTS_DIR}/patch-folly.sh")
set(PATCH_FBTHRIFT_SCRIPT "${PATCH_SCRIPTS_DIR}/patch-fbthrift.sh")
set(PATCH_ZSTD_SCRIPT "${PATCH_SCRIPTS_DIR}/patch-zstd.sh")

# Determine which patches to apply
# By default, apply all patches unless already applied
if(NOT DEFINED PATCH_FOLLY)
  set(PATCH_FOLLY ON)
endif()

if(NOT DEFINED PATCH_FBTHRIFT)
  set(PATCH_FBTHRIFT ON)
endif()

if(NOT DEFINED PATCH_ZSTD)
  set(PATCH_ZSTD ON)
endif()

# Get bash executable
if(NOT DEFINED GNU_BASH)
  set(GNU_BASH "bash" CACHE STRING "Path to bash executable")
endif()

# Apply folly patches
if(PATCH_FOLLY)
  if(EXISTS "${PATCH_FOLLY_SCRIPT}" AND EXISTS "${FOLLY_ROOT}")
    message(STATUS "Applying folly patches: ${GNU_BASH} ${PATCH_FOLLY_SCRIPT} ${FOLLY_ROOT}")
    execute_process(
      COMMAND "${GNU_BASH}"
              "${PATCH_FOLLY_SCRIPT}"
              "${FOLLY_ROOT}"
      RESULT_VARIABLE PATCH_FOLLY_RES
      OUTPUT_VARIABLE PATCH_FOLLY_TXT
      ERROR_VARIABLE PATCH_FOLLY_TXT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(PATCH_FOLLY_RES EQUAL 0)
      message(STATUS "Folly patches applied successfully")
    else()
      # Don't fail if patches already applied or not needed
      message(WARNING "Folly patch script returned non-zero: ${PATCH_FOLLY_TXT}")
      message(WARNING "This may be expected if patches are already applied")
    endif()
  else()
    if(NOT EXISTS "${PATCH_FOLLY_SCRIPT}")
      message(WARNING "Folly patch script not found: ${PATCH_FOLLY_SCRIPT}")
    endif()
    if(NOT EXISTS "${FOLLY_ROOT}")
      message(WARNING "Folly source directory not found: ${FOLLY_ROOT}")
    endif()
  endif()
endif()

# Apply fbthrift patches
if(PATCH_FBTHRIFT)
  if(EXISTS "${PATCH_FBTHRIFT_SCRIPT}" AND EXISTS "${FBTHRIFT_ROOT}")
    message(STATUS "Applying fbthrift patches: ${GNU_BASH} ${PATCH_FBTHRIFT_SCRIPT} ${FBTHRIFT_ROOT}")
    execute_process(
      COMMAND "${GNU_BASH}"
              "${PATCH_FBTHRIFT_SCRIPT}"
              "${FBTHRIFT_ROOT}"
      RESULT_VARIABLE PATCH_FBTHRIFT_RES
      OUTPUT_VARIABLE PATCH_FBTHRIFT_TXT
      ERROR_VARIABLE PATCH_FBTHRIFT_TXT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(PATCH_FBTHRIFT_RES EQUAL 0)
      message(STATUS "Fbthrift patches applied successfully")
    else()
      # Don't fail if patches already applied or not needed
      message(WARNING "Fbthrift patch script returned non-zero: ${PATCH_FBTHRIFT_TXT}")
      message(WARNING "This may be expected if patches are already applied")
    endif()
  else()
    if(NOT EXISTS "${PATCH_FBTHRIFT_SCRIPT}")
      message(WARNING "Fbthrift patch script not found: ${PATCH_FBTHRIFT_SCRIPT}")
    endif()
    if(NOT EXISTS "${FBTHRIFT_ROOT}")
      message(WARNING "Fbthrift source directory not found: ${FBTHRIFT_ROOT}")
    endif()
  endif()
endif()

# Apply zstd patches
if(PATCH_ZSTD)
  if(EXISTS "${PATCH_ZSTD_SCRIPT}" AND EXISTS "${ZSTD_ROOT}")
    message(STATUS "Applying zstd patches: ${GNU_BASH} ${PATCH_ZSTD_SCRIPT} ${ZSTD_ROOT}")
    execute_process(
      COMMAND "${GNU_BASH}"
              "${PATCH_ZSTD_SCRIPT}"
              "${ZSTD_ROOT}"
      RESULT_VARIABLE PATCH_ZSTD_RES
      OUTPUT_VARIABLE PATCH_ZSTD_TXT
      ERROR_VARIABLE PATCH_ZSTD_TXT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(PATCH_ZSTD_RES EQUAL 0)
      message(STATUS "Zstd patches applied successfully")
    else()
      # Don't fail if patches already applied or not needed
      message(WARNING "Zstd patch script returned non-zero: ${PATCH_ZSTD_TXT}")
      message(WARNING "This may be expected if patches are already applied")
    endif()
  else()
    if(NOT EXISTS "${PATCH_ZSTD_SCRIPT}")
      message(WARNING "Zstd patch script not found: ${PATCH_ZSTD_SCRIPT}")
    endif()
    if(NOT EXISTS "${ZSTD_ROOT}")
      message(WARNING "Zstd source directory not found: ${ZSTD_ROOT}")
    endif()
  endif()
endif()