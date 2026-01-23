# Folly and fbthrift dependency configuration
# OPTIONAL - Only needed for Experimental Thrift metadata format support

# Only attempt to find folly/fbthrift if Experimental Thrift support is enabled
if(NOT DWARFS_WITH_EXPERIMENTAL_THRIFT)
  message(STATUS "Experimental Thrift support disabled, skipping folly/fbthrift")
  return()
endif()

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  # folly and fbthrift come from overlay ports (tamatebako forks)
  find_package(folly REQUIRED CONFIG)
  find_package(FBThrift REQUIRED CONFIG)
  
  message(STATUS "Using folly from vcpkg overlay: ${folly_VERSION}")
  message(STATUS "Using fbthrift from vcpkg overlay: ${FBThrift_VERSION}")
else()
  # Non-vcpkg mode: folly/fbthrift should be built from submodules
  # This is handled by cmake/folly.cmake and cmake/thrift.cmake
  if(NOT TARGET folly)
    message(STATUS "Thrift enabled but folly target not found - will be built from submodules")
  endif()
endif()