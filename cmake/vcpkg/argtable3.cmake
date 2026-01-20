# argtable3 dependency configuration
# Command-line argument parsing - REQUIRED for all tools

include_guard(GLOBAL)

set(ARGTABLE3_REQUIRED_VERSION 3.2.0)

# Check if we're in vcpkg manifest mode (using vcpkg_installed)
# VCPKG_MANIFEST_INSTALL is set when using manifest-based vcpkg
if(DEFINED VCPKG_MANIFEST_INSTALL AND VCPKG_MANIFEST_INSTALL)
  # vcpkg manifest mode: use find_package without version constraint
  # vcpkg will have installed the correct version
  # Note: vcpkg's argtable3 port provides "Argtable3" (capital A), not "argtable3"
  find_package(Argtable3 CONFIG)
  if(TARGET Argtable3::argtable3)
    # Create lowercase alias for consistency
    if(NOT TARGET argtable3::argtable3)
      add_library(argtable3::argtable3 ALIAS Argtable3::argtable3)
    endif()
    set(ARGTABLE3_FOUND TRUE)
    message(STATUS "Using argtable3 from vcpkg manifest")
  else()
    # Try lowercase as fallback
    find_package(argtable3 CONFIG)
    if(TARGET argtable3::argtable3)
      set(ARGTABLE3_FOUND TRUE)
      message(STATUS "Using argtable3 from vcpkg (fallback lowercase)")
    else()
      message(STATUS "argtable3 not found in vcpkg, will be installed as dependency")
    endif()
  endif()
elseif(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg port build mode
  # Note: vcpkg's argtable3 port provides "Argtable3" (capital A), not "argtable3"
  find_package(Argtable3 ${ARGTABLE3_REQUIRED_VERSION} CONFIG)
  if(TARGET Argtable3::argtable3)
    # Create lowercase alias for consistency
    if(NOT TARGET argtable3::argtable3)
      add_library(argtable3::argtable3 ALIAS Argtable3::argtable3)
    endif()
    set(ARGTABLE3_FOUND TRUE)
    message(STATUS "Using argtable3 from vcpkg port build: ${Argtable3_VERSION}")
  else()
    # Try lowercase as fallback
    find_package(argtable3 ${ARGTABLE3_REQUIRED_VERSION} CONFIG)
    if(NOT TARGET argtable3::argtable3)
      add_library(argtable3::argtable3 ALIAS argtable3)
    endif()
    set(ARGTABLE3_FOUND TRUE)
    message(STATUS "Using argtable3 from vcpkg port build: ${argtable3_VERSION}")
  endif()
else()
  # Try pkg-config first (non-vcpkg builds)
  pkg_check_modules(argtable3 argtable3>=${ARGTABLE3_REQUIRED_VERSION})

  if(argtable3_FOUND)
    # Create imported target from pkg-config
    add_library(argtable3::argtable3 INTERFACE IMPORTED)
    set_target_properties(argtable3::argtable3 PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${argtable3_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "${argtable3_LIBRARIES}"
      INTERFACE_LINK_DIRECTORIES "${argtable3_LIBRARY_DIRS}"
    )
    message(STATUS "Using argtable3 from pkg-config: ${argtable3_VERSION}")
  else()
    # FetchContent as fallback
    message(STATUS "argtable3 not found via pkg-config, using FetchContent")

    include(FetchContent)
    FetchContent_Declare(
      argtable3
      GIT_REPOSITORY https://github.com/argtable/argtable3.git
      GIT_TAG v3.3.1
      GIT_SHALLOW TRUE
    )

    # Configure argtable3 options
    set(ARGTABLE3_ENABLE_TESTS OFF CACHE BOOL "Disable argtable3 tests" FORCE)
    set(ARGTABLE3_ENABLE_EXAMPLES OFF CACHE BOOL "Disable argtable3 examples" FORCE)

    FetchContent_MakeAvailable(argtable3)

    # Create alias if needed
    if(NOT TARGET argtable3::argtable3)
      add_library(argtable3::argtable3 ALIAS argtable3)
    endif()

    set(ARGTABLE3_FOUND TRUE)
    message(STATUS "Using argtable3 from FetchContent: v3.3.1")
  endif()
endif()

if(NOT ARGTABLE3_FOUND)
  message(FATAL_ERROR "argtable3 is REQUIRED but not found. Install libargtable3-dev or set VCPKG_BUILD=ON")
endif()