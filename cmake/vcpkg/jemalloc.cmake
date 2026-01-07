# jemalloc dependency configuration
# Memory allocator - MANDATORY for static builds (Tebako fork)

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  # jemalloc comes from overlay port (tamatebako/jemalloc)

  # Add global vcpkg to search paths if local vcpkg_installed doesn't have it
  if(DEFINED ENV{VCPKG_ROOT})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")
  endif()

  find_package(jemalloc ${JEMALLOC_REQUIRED_VERSION} REQUIRED CONFIG)

  set(JEMALLOC_FOUND TRUE)
  message(STATUS "Using jemalloc from vcpkg overlay: ${jemalloc_VERSION}")
else()
  # Fallback to pkg-config (non-vcpkg builds)
  pkg_check_modules(JEMALLOC REQUIRED IMPORTED_TARGET jemalloc>=${JEMALLOC_REQUIRED_VERSION})

  if(NOT JEMALLOC_FOUND)
    message(FATAL_ERROR "jemalloc is MANDATORY but not found. Install libjemalloc-dev or set VCPKG_BUILD=ON with overlay port")
  endif()

  # Create alias target for consistent interface
  if(NOT TARGET jemalloc::jemalloc)
    add_library(jemalloc::jemalloc ALIAS PkgConfig::JEMALLOC)
  endif()

  message(STATUS "Using jemalloc from pkg-config: ${JEMALLOC_VERSION}")
endif()