#
# Copyright (c) Ribose Inc.
#
# This file is part of dwarfs tebako integration.
#
# SPDX-License-Identifier: MIT
#

# LibArchive Setup Module
#
# Configures libarchive for tebako builds
# This module imports the setup script from TOOLS if available


# Check if TOOLS directory exists
if(NOT EXISTS "${TOOLS}")
  message(WARNING "TOOLS directory not found at: ${TOOLS}")
  message(WARNING "LibArchive setup will be skipped")
  return()
endif()

# Path to the libarchive setup script
set(SETUP_LIBARCHIVE_SCRIPT "${TOOLS}/cmake-scripts/setup-libarchive.cmake")

# Import the setup script if it exists
if(EXISTS "${SETUP_LIBARCHIVE_SCRIPT}")
  message(STATUS "Including libarchive setup from: ${SETUP_LIBARCHIVE_SCRIPT}")
  include("${SETUP_LIBARCHIVE_SCRIPT}")

  # Verify that _LIBARCHIVE target was created
  if(TARGET _LIBARCHIVE)
    message(STATUS "LibArchive target (_LIBARCHIVE) configured successfully")
  else()
    message(WARNING "LibArchive setup script did not create _LIBARCHIVE target")
  endif()
else()
  message(WARNING "LibArchive setup script not found at: ${SETUP_LIBARCHIVE_SCRIPT}")
  message(WARNING "Attempting to use system libarchive")

  # Try to find system libarchive as fallback
  find_package(LibArchive QUIET)

  if(LibArchive_FOUND)
    message(STATUS "Using system libarchive: ${LibArchive_VERSION}")

    # Create alias target for compatibility
    if(NOT TARGET _LIBARCHIVE)
      add_library(_LIBARCHIVE INTERFACE)
      target_link_libraries(_LIBARCHIVE INTERFACE LibArchive::LibArchive)
    endif()
  else()
    message(WARNING "System libarchive not found either")
    message(WARNING "LibArchive functionality may be limited")
  endif()
endif()