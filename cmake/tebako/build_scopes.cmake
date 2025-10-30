#
# Copyright (c) Ribose Inc.
#
# This file is part of dwarfs tebako integration.
#
# SPDX-License-Identifier: MIT
#

# Tebako Build Scope System
#
# Defines three build scopes:
# - ALL: Build library + all tools + FUSE driver (full build)
# - MKD: Build library + mkdwarfs only (tebako default)
# - LIB: Build library only (for cross-compilation)

cmake_minimum_required(VERSION 3.24.0)

# Define build scope variable
if(NOT DEFINED TEBAKO_BUILD_SCOPE)
  set(TEBAKO_BUILD_SCOPE "ALL" CACHE STRING "Tebako build scope (ALL/MKD/LIB)")
  set_property(CACHE TEBAKO_BUILD_SCOPE PROPERTY STRINGS "ALL" "MKD" "LIB")
endif()

# Validate scope
if(NOT TEBAKO_BUILD_SCOPE MATCHES "^(ALL|MKD|LIB)$")
  message(FATAL_ERROR "Invalid TEBAKO_BUILD_SCOPE: ${TEBAKO_BUILD_SCOPE}. Must be ALL, MKD, or LIB")
endif()

# Map tebako scope to upstream WITH_* options
if(TEBAKO_BUILD_SCOPE STREQUAL "LIB")
  # Library only - minimal build for cross-compilation
  set(WITH_LIBDWARFS ON CACHE BOOL "Build libdwarfs" FORCE)
  set(WITH_TOOLS OFF CACHE BOOL "Build tools" FORCE)
  set(WITH_FUSE_DRIVER OFF CACHE BOOL "Build FUSE driver" FORCE)
  message(STATUS "Tebako scope LIB: Building library only")

elseif(TEBAKO_BUILD_SCOPE STREQUAL "MKD")
  # Library + mkdwarfs - standard tebako configuration
  set(WITH_LIBDWARFS ON CACHE BOOL "Build libdwarfs" FORCE)
  set(WITH_TOOLS ON CACHE BOOL "Build tools" FORCE)
  set(WITH_FUSE_DRIVER OFF CACHE BOOL "Build FUSE driver" FORCE)
  message(STATUS "Tebako scope MKD: Building library + mkdwarfs")

else() # ALL
  # Full build - library + all tools + FUSE
  set(WITH_LIBDWARFS ON CACHE BOOL "Build libdwarfs" FORCE)
  set(WITH_TOOLS ON CACHE BOOL "Build tools" FORCE)
  set(WITH_FUSE_DRIVER ON CACHE BOOL "Build FUSE driver" FORCE)
  message(STATUS "Tebako scope ALL: Building everything")
endif()

# Ensure tests and benchmarks respect scope
if(TEBAKO_BUILD_SCOPE STREQUAL "LIB")
  set(WITH_TESTS OFF CACHE BOOL "Build tests" FORCE)
  set(WITH_BENCHMARKS OFF CACHE BOOL "Build benchmarks" FORCE)
endif()

message(STATUS "Tebako build scope: ${TEBAKO_BUILD_SCOPE}")