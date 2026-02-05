# Overlay port for boost-assert with MSYS circular dependency fix
# boost-assert is a header-only library that's part of Boost
# For MSYS, vcpkg has issues with the boost-cmake wrapper requiring
# BoostConfig.cmake before it's available

# Use vcpkg's empty package policy - boost-assert will be provided
# by the boost-cmake wrapper which is installed as a dependency
set(VCPKG_POLICY_EMPTY_PACKAGE enabled)

message(STATUS "boost-assert: using empty package policy (header-only, provided by boost-cmake wrapper)")
