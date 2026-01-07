# fmt (fmtlib) dependency configuration
# String formatting library (header-only or compiled)

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(fmt ${LIBFMT_REQUIRED_VERSION} REQUIRED CONFIG)
  message(STATUS "Using fmt from vcpkg: ${fmt_VERSION}")
  return()
endif()

# Try system package first
find_package(fmt ${LIBFMT_REQUIRED_VERSION} QUIET CONFIG)
if(fmt_FOUND)
  message(STATUS "Using system fmt: ${fmt_VERSION}")
  return()
endif()

# Fallback to FetchContent (only in non-vcpkg builds)
message(STATUS "fmt not found, using FetchContent...")
include(FetchContent)

if(NOT DEFINED LIBFMT_TAG)
  set(LIBFMT_TAG ${LIBFMT_PREFERRED_VERSION})
endif()

FetchContent_Declare(fmt
  GIT_REPOSITORY ${LIBFMT_GIT_REPO}
  GIT_TAG ${LIBFMT_TAG}
)

FetchContent_MakeAvailable(fmt)
message(STATUS "Using fmt from FetchContent: ${LIBFMT_TAG}")