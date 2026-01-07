# GoogleTest dependency configuration
# Unit testing framework

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(GTest REQUIRED CONFIG)
  message(STATUS "Using GoogleTest from vcpkg: ${GTest_VERSION}")
  return()
endif()

# Try system package first (unless PREFER_SYSTEM_GTEST is explicitly OFF)
if(NOT DEFINED PREFER_SYSTEM_GTEST OR PREFER_SYSTEM_GTEST)
  find_package(GTest QUIET CONFIG)
  if(GTest_FOUND)
    # Check if version supports C++20 char8_t (requires 1.15.0+)
    # System GoogleTest 1.17.0 from Homebrew lacks PrintU8StringTo in some configurations
    if(GTest_VERSION VERSION_LESS "1.15.0")
      message(STATUS "System GoogleTest ${GTest_VERSION} too old, using FetchContent instead")
      unset(GTest_FOUND)
    else()
      message(STATUS "Using system GoogleTest: ${GTest_VERSION}")
      return()
    endif()
  endif()
endif()

# Fallback to FetchContent (only in non-vcpkg builds)
message(STATUS "GoogleTest not found or too old, using FetchContent...")
include(FetchContent)

if(NOT DEFINED GOOGLETEST_TAG)
  set(GOOGLETEST_TAG ${GOOGLETEST_PREFERRED_VERSION})
endif()

FetchContent_Declare(googletest
  GIT_REPOSITORY ${GOOGLETEST_GIT_REPO}
  GIT_TAG ${GOOGLETEST_TAG}
  EXCLUDE_FROM_ALL
  SYSTEM
)

FetchContent_MakeAvailable(googletest)
message(STATUS "Using GoogleTest from FetchContent: ${GOOGLETEST_TAG}")