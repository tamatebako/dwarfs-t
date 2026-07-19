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

set(FLATBUFFERS_REQUIRED_VERSION 23.5.26)
set(FLATBUFFERS_PREFERRED_VERSION 24.3.25)

# Try to find FlatBuffers package
find_package(flatbuffers ${FLATBUFFERS_REQUIRED_VERSION} CONFIG QUIET)

if(NOT flatbuffers_FOUND)
  message(STATUS "FlatBuffers not found locally, fetching via FetchContent...")

  if(DEFINED ENV{DWARFS_LOCAL_REPO_PATH})
    set(FLATBUFFERS_GIT_REPO $ENV{DWARFS_LOCAL_REPO_PATH}/flatbuffers)
  else()
    set(FLATBUFFERS_GIT_REPO https://github.com/google/flatbuffers.git)
  endif()

  FetchContent_Declare(
    flatbuffers
    GIT_REPOSITORY ${FLATBUFFERS_GIT_REPO}
    GIT_TAG v${FLATBUFFERS_PREFERRED_VERSION}
    EXCLUDE_FROM_ALL
    SYSTEM
  )

  # FlatBuffers options
  set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(FLATBUFFERS_BUILD_FLATC ON CACHE BOOL "" FORCE)
  set(FLATBUFFERS_BUILD_FLATHASH OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(flatbuffers)

  message(STATUS "FlatBuffers fetched successfully (version ${FLATBUFFERS_PREFERRED_VERSION})")
else()
  message(STATUS "Found FlatBuffers: ${flatbuffers_VERSION}")
endif()

# Ensure we have the flatc compiler
if(NOT TARGET flatbuffers::flatc)
  if(TARGET flatc)
    add_executable(flatbuffers::flatc ALIAS flatc)
  else()
    find_program(FLATC_EXECUTABLE NAMES flatc
      HINTS ${flatbuffers_DIR}/../../../bin
            ${flatbuffers_DIR}/../../bin
      DOC "flatc compiler")

    if(NOT FLATC_EXECUTABLE)
      message(FATAL_ERROR "flatc compiler not found")
    endif()

    add_executable(flatbuffers::flatc IMPORTED)
    set_target_properties(flatbuffers::flatc PROPERTIES
      IMPORTED_LOCATION ${FLATC_EXECUTABLE}
    )
  endif()
endif()