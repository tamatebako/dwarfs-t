#
# Copyright (c) 2025 Ribose Inc.
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

# Try to find nlohmann_json via find_package (vcpkg or system)
find_package(nlohmann_json CONFIG QUIET)

if(nlohmann_json_FOUND)
  message(STATUS "Using nlohmann_json: ${nlohmann_json_VERSION}")
  # Target nlohmann_json::nlohmann_json already exists
else()
  # Fall back to FetchContent
  message(STATUS "Fetching nlohmann_json from source...")

  if(DEFINED ENV{DWARFS_LOCAL_REPO_PATH})
    set(NLOHMANN_JSON_GIT_REPO $ENV{DWARFS_LOCAL_REPO_PATH}/nlohmann_json)
  else()
    set(NLOHMANN_JSON_GIT_REPO https://github.com/nlohmann/json.git)
  endif()

  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY ${NLOHMANN_JSON_GIT_REPO}
    GIT_TAG v3.11.3
    EXCLUDE_FROM_ALL
    SYSTEM
  )

  # nlohmann_json options
  set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
  set(JSON_Install OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(nlohmann_json)

  message(STATUS "nlohmann_json fetched successfully (v3.11.3)")

  # Verify target exists
  if(NOT TARGET nlohmann_json::nlohmann_json)
    message(FATAL_ERROR "nlohmann_json::nlohmann_json target not found after fetch")
  endif()
endif()
