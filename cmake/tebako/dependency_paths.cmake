#
# Copyright (c) Ribose Inc.
#
# This file is part of dwarfs tebako integration.
#
# SPDX-License-Identifier: MIT
#

# Dependency Paths Configuration
#
# Sets up DEPS and TOOLS directory paths for tebako builds
# DEPS: External dependencies (libraries, headers)
# TOOLS: Build tools and scripts (cmake scripts, patches, etc.)

cmake_minimum_required(VERSION 3.24.0)

# DEPS Directory Configuration
if(NOT DEFINED DEPS)
  set(DEPS "${CMAKE_CURRENT_SOURCE_DIR}/deps" CACHE PATH "Dependencies directory")
  set(EXTERNAL_DEPS OFF)
  message(STATUS "Using local DEPS directory: ${DEPS}")
else()
  set(EXTERNAL_DEPS ON)
  message(STATUS "Using external DEPS directory: ${DEPS}")
endif()

# DEPS subdirectories
set(DEPS_INCLUDE_DIR "${DEPS}/include" CACHE PATH "Dependencies include directory")
set(DEPS_LIB_DIR "${DEPS}/lib" CACHE PATH "Dependencies lib directory")
set(DEPS_BIN_DIR "${DEPS}/bin" CACHE PATH "Dependencies bin directory")
set(DEPS_SBIN_DIR "${DEPS}/sbin" CACHE PATH "Dependencies sbin directory")

# TOOLS Directory Configuration
if(NOT DEFINED TOOLS)
  set(TOOLS "${CMAKE_CURRENT_SOURCE_DIR}/tools" CACHE PATH "Tools directory")
  message(STATUS "Using local TOOLS directory: ${TOOLS}")
else()
  message(STATUS "Using external TOOLS directory: ${TOOLS}")
endif()

# Configure include and library paths
# Use BEFORE to ensure DEPS paths are searched first
include_directories(BEFORE SYSTEM ${DEPS_INCLUDE_DIR})
link_directories(BEFORE ${DEPS_LIB_DIR})

# Include folly and fbthrift from source (submodules)
# These paths are needed even though we want to remove them eventually
set(FOLLY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/folly")
set(FBTHRIFT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/fbthrift")
set(ZSTD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/zstd")

if(EXISTS ${FOLLY_ROOT})
  include_directories(BEFORE ${FOLLY_ROOT})
endif()

if(EXISTS ${FBTHRIFT_ROOT})
  include_directories(BEFORE ${FBTHRIFT_ROOT})
endif()

# Summary
message(STATUS "Tebako dependency paths configured:")
message(STATUS "  DEPS: ${DEPS}")
message(STATUS "  DEPS_INCLUDE_DIR: ${DEPS_INCLUDE_DIR}")
message(STATUS "  DEPS_LIB_DIR: ${DEPS_LIB_DIR}")
message(STATUS "  TOOLS: ${TOOLS}")
message(STATUS "  EXTERNAL_DEPS: ${EXTERNAL_DEPS}")