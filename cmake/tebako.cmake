#
# Copyright (c) Ribose Inc.
#
# This file is part of dwarfs tebako integration.
#
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.24.0)

message(STATUS ">>>>> Tebako build mode enabled")

# Tebako module directory
set(TEBAKO_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tebako")

# Include sub-modules in dependency order
include(${TEBAKO_CMAKE_DIR}/build_scopes.cmake)
include(${TEBAKO_CMAKE_DIR}/platform_detection.cmake)
include(${TEBAKO_CMAKE_DIR}/dependency_paths.cmake)
include(${TEBAKO_CMAKE_DIR}/compiler_flags.cmake)
include(${TEBAKO_CMAKE_DIR}/validation.cmake)

# Platform-specific modules
if(IS_MSYS)
  if(EXISTS ${TEBAKO_CMAKE_DIR}/msys_support.cmake)
    include(${TEBAKO_CMAKE_DIR}/msys_support.cmake)
  endif()
endif()

if(NOT MSVC)
  if(EXISTS ${TEBAKO_CMAKE_DIR}/libarchive_setup.cmake)
    include(${TEBAKO_CMAKE_DIR}/libarchive_setup.cmake)
  endif()
endif()

message(STATUS "<<<<< Tebako configuration complete")
message(STATUS "Tebako build scope: ${TEBAKO_BUILD_SCOPE}")
message(STATUS "Tebako DEPS: ${DEPS}")
message(STATUS "Tebako TOOLS: ${TOOLS}")