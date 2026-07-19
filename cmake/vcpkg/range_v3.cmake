# range-v3 dependency configuration
# Header-only ranges library

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(range-v3 ${RANGE_V3_REQUIRED_VERSION} REQUIRED CONFIG)
  message(STATUS "Using range-v3 from vcpkg: ${range-v3_VERSION}")
  return()
endif()

# Try system package first
find_package(range-v3 ${RANGE_V3_REQUIRED_VERSION} QUIET CONFIG)
if(range-v3_FOUND)
  message(STATUS "Using system range-v3: ${range-v3_VERSION}")
  return()
endif()

# Fallback to FetchContent (only in non-vcpkg builds)
message(STATUS "range-v3 not found, using FetchContent...")
include(FetchContent)

if(NOT DEFINED RANGE_V3_TAG)
  set(RANGE_V3_TAG ${RANGE_V3_PREFERRED_VERSION})
endif()

FetchContent_Declare(range-v3
  GIT_REPOSITORY ${RANGE_V3_GIT_REPO}
  GIT_TAG ${RANGE_V3_TAG}
)

FetchContent_MakeAvailable(range-v3)
message(STATUS "Using range-v3 from FetchContent: ${RANGE_V3_TAG}")