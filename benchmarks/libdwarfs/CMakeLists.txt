# libdwarfs API Benchmarks

# Ensure we have required libraries
if(NOT TARGET dwarfs_reader)
  message(FATAL_ERROR "dwarfs_reader library not available. Build with -DWITH_LIBDWARFS=ON")
endif()

# Find required dependencies
find_package(nlohmann_json 3.0 QUIET)
if(NOT nlohmann_json_FOUND)
  message(STATUS "nlohmann_json not found, using FetchContent")
  include(FetchContent)
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

# Common include directories
set(BENCHMARK_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
)

# Common link libraries
set(BENCHMARK_LINK_LIBS
  dwarfs_reader
  dwarfs_common
  nlohmann_json::nlohmann_json
)

# Add platform-specific libraries
if(UNIX AND NOT APPLE)
  list(APPEND BENCHMARK_LINK_LIBS pthread)
elseif(WIN32)
  list(APPEND BENCHMARK_LINK_LIBS psapi)
endif()

# Macro to add a benchmark executable
macro(add_libdwarfs_benchmark name source)
  add_executable(${name} ${source})
  
  target_include_directories(${name}
    PRIVATE ${BENCHMARK_INCLUDE_DIRS}
  )
  
  target_link_libraries(${name}
    PRIVATE ${BENCHMARK_LINK_LIBS}
  )
  
  # C++20 required
  set_target_properties(${name} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
  )
  
  # Install to bin directory
  install(TARGETS ${name}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endmacro()

# Add all benchmark executables
add_libdwarfs_benchmark(single_file_bench single_file_bench.cpp)
add_libdwarfs_benchmark(multiple_files_bench multiple_files_bench.cpp)
add_libdwarfs_benchmark(full_extract_bench full_extract_bench.cpp)
add_libdwarfs_benchmark(random_access_bench random_access_bench.cpp)

# Create a target that builds all benchmarks
add_custom_target(libdwarfs_benchmarks
  DEPENDS
    single_file_bench
    multiple_files_bench
    full_extract_bench
    random_access_bench
)

# Add helpful message
message(STATUS "libdwarfs API benchmarks configured:")
message(STATUS "  - single_file_bench: Extract single file benchmark")
message(STATUS "  - multiple_files_bench: Extract multiple files benchmark")
message(STATUS "  - full_extract_bench: Full filesystem extraction benchmark")
message(STATUS "  - random_access_bench: Random access patterns benchmark")
message(STATUS "")
message(STATUS "Build all benchmarks with: cmake --build . --target libdwarfs_benchmarks")