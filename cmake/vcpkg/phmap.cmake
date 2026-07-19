# parallel-hashmap dependency configuration
# Efficient hash maps (header-only)

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: parallel-hashmap is header-only, use find_path
  find_path(PARALLEL_HASHMAP_INCLUDE_DIR
    NAMES parallel_hashmap/phmap_config.h
    REQUIRED
  )
  if(PARALLEL_HASHMAP_INCLUDE_DIR)
    add_library(parallel-hashmap INTERFACE IMPORTED)
    target_include_directories(parallel-hashmap INTERFACE ${PARALLEL_HASHMAP_INCLUDE_DIR})

    # Create phmap alias for compatibility
    if(NOT TARGET phmap)
      add_library(phmap INTERFACE IMPORTED)
      target_include_directories(phmap INTERFACE ${PARALLEL_HASHMAP_INCLUDE_DIR})
    endif()

    message(STATUS "Using parallel-hashmap from vcpkg (header-only)")
    return()
  endif()
endif()

# Try system package first
find_package(parallel-hashmap QUIET CONFIG)
if(parallel-hashmap_FOUND)
  message(STATUS "Using system parallel-hashmap: ${parallel-hashmap_VERSION}")

  # Create phmap alias if it doesn't exist
  if(NOT TARGET phmap AND TARGET parallel-hashmap)
    add_library(phmap ALIAS parallel-hashmap)
  endif()

  return()
endif()

# Fallback: Try to detect system version, otherwise use FetchContent
try_run(
  PHMAP_RUN_RESULT
  PHMAP_COMPILE_RESULT
  SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/checks/phmap-version.cpp
  CMAKE_FLAGS -DINCLUDE_DIRECTORIES=${TRY_RUN_INCLUDE_DIRECTORIES}
  CXX_STANDARD ${DWARFS_CXX_STANDARD}
  RUN_OUTPUT_VARIABLE PHMAP_VERSION
  COMPILE_OUTPUT_VARIABLE PHMAP_COMPILE_OUTPUT
)

if(PHMAP_RUN_RESULT EQUAL 0)
  if(PHMAP_VERSION VERSION_LESS ${PARALLEL_HASHMAP_REQUIRED_VERSION})
    string(STRIP "${PHMAP_VERSION}" PHMAP_VERSION)
    message(STATUS "System-installed parallel-hashmap version ${PHMAP_VERSION} is less than required version ${PARALLEL_HASHMAP_REQUIRED_VERSION}")
  endif()
else()
  message(STATUS "failed to check parallel-hashmap version")
  message(VERBOSE "${PHMAP_COMPILE_OUTPUT}")
endif()

if(PHMAP_RUN_RESULT EQUAL 0 AND PHMAP_VERSION VERSION_GREATER_EQUAL ${PARALLEL_HASHMAP_REQUIRED_VERSION})
  message(STATUS "Using system parallel-hashmap: ${PHMAP_VERSION}")
  add_library(phmap INTERFACE)
else()
  message(STATUS "Fetching parallel-hashmap via FetchContent...")
  FetchContent_Declare(
    parallel-hashmap
    GIT_REPOSITORY ${PARALLEL_HASHMAP_GIT_REPO}
    GIT_TAG ${PARALLEL_HASHMAP_PREFERRED_VERSION}
    EXCLUDE_FROM_ALL
    SYSTEM
  )

  # Use Populate instead of MakeAvailable to avoid creating targets with INTERFACE_SOURCES
  # We only need the header files for this header-only library
  FetchContent_Populate(parallel-hashmap)

  # Create a simple INTERFACE library with just include directories
  add_library(phmap INTERFACE)
  target_include_directories(phmap INTERFACE
    $<BUILD_INTERFACE:${parallel-hashmap_SOURCE_DIR}>
  )

  message(STATUS "Using parallel-hashmap from FetchContent: ${PARALLEL_HASHMAP_PREFERRED_VERSION}")
endif()