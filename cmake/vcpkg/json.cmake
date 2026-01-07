# nlohmann-json dependency configuration
# JSON for Modern C++ (header-only)

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(nlohmann_json REQUIRED CONFIG)
  message(STATUS "Using nlohmann-json from vcpkg: ${nlohmann_json_VERSION}")
  return()
endif()

# Try system package first
find_package(nlohmann_json QUIET CONFIG)
if(nlohmann_json_FOUND)
  message(STATUS "Using system nlohmann-json: ${nlohmann_json_VERSION}")
  return()
endif()

# Fallback to FetchContent (only in non-vcpkg builds)
message(STATUS "nlohmann-json not found, using FetchContent...")
include(FetchContent)

FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(nlohmann_json)
message(STATUS "Using nlohmann-json from FetchContent: v3.11.3")