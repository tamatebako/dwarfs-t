#
# Copyright (c) Marcus Holland-Moritz
#
# This file is part of dwarfs.
#
# dwarfs is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# dwarfs is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# dwarfs.  If not, see <https://www.gnu.org/licenses/>.
#

# Conditional minimum version for tebako compatibility

# ============================================================================
# FUSE-T RPATH Helper for Test Targets
# ============================================================================

# Function to add FUSE-T RPATH to test targets
# This is needed because static libraries don't propagate RPATH to dependent executables
# Usage: add_fuse_rpath_to_tests(target1 target2 ...)
function(add_fuse_rpath_to_tests)
  # RPATH is only needed on macOS/Unix, not Windows
  if(APPLE AND FUSE_IMPLEMENTATION STREQUAL "fuse-t" AND FUSE_T_LIBRARY_DIRS)
    foreach(TARGET_NAME ${ARGV})
      if(TARGET ${TARGET_NAME})
        target_link_options(${TARGET_NAME} PRIVATE
          "LINKER:-rpath,${FUSE_T_LIBRARY_DIRS}"
        )
      endif()
    endforeach()
  endif()
endfunction()

# ============================================================================
# Test Helpers Library
# ============================================================================

if(WITH_TESTS OR WITH_BENCHMARKS OR WITH_FUZZ)
  add_library(dwarfs_test_helpers
    test/compare_directories.cpp
    test/mmap_mock.cpp
    test/sparse_file_builder.cpp
    test/test_helpers.cpp
    test/test_iolayer.cpp
    test/loremipsum.cpp
    test/test_dirtree.cpp
    test/filter_test_data.cpp
    test/test_file_data.cpp
    test/test_fixtures.cpp
    test/test_common.cpp
  )
  if(WITH_BENCHMARKS)
    target_sources(dwarfs_test_helpers PRIVATE test/test_strings.cpp)
  endif()
  target_link_libraries(dwarfs_test_helpers PUBLIC dwarfs_reader dwarfs_writer dwarfs_tool PkgConfig::XXHASH)
  set_property(TARGET dwarfs_test_helpers PROPERTY CXX_STANDARD ${DWARFS_CXX_STANDARD})
  target_compile_definitions(dwarfs_test_helpers
       PUBLIC TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/test"
              TOOLS_BIN_DIR="${CMAKE_CURRENT_BINARY_DIR}")
  if(WITH_DEV_TOOLS)
    add_executable(treediff test/treediff.cpp)
    target_link_libraries(treediff PRIVATE dwarfs_test_helpers)
    # MinGW/MSYS: Disable ENABLE_EXPORTS
    if(COMMAND fix_mingw_exe_exports)
      fix_mingw_exe_exports(treediff)
    endif()

    add_executable(sparsedump test/sparsedump.cpp)
    target_link_libraries(sparsedump PRIVATE dwarfs_common)
    # MinGW/MSYS: Disable ENABLE_EXPORTS
    if(COMMAND fix_mingw_exe_exports)
      fix_mingw_exe_exports(sparsedump)
    endif()

    list(APPEND BINARY_TARGETS treediff sparsedump)
  endif()
endif()

if(WITH_TESTS AND WITH_TOOLS)
  add_library(dwarfs_tool_main_tester test/test_tool_main_tester.cpp)
  target_link_libraries(dwarfs_tool_main_tester PUBLIC dwarfs_test_helpers GTest::gtest)
  target_link_libraries(dwarfs_tool_main_tester PRIVATE mkdwarfs_main dwarfsck_main dwarfsextract_main)
  set_property(TARGET dwarfs_tool_main_tester PROPERTY CXX_STANDARD ${DWARFS_CXX_STANDARD})
endif()

# ============================================================================
# Test Fixtures Library (OOP Architecture)
# ============================================================================

if(WITH_TESTS)
  add_library(dwarfs_test_fixtures
    test/fixtures/dwarfs_test_fixture.cpp
    test/fixtures/filesystem_test_fixture.cpp
  )
  target_link_libraries(dwarfs_test_fixtures
    PUBLIC
      dwarfs_test_helpers
      GTest::gtest
      dwarfs_reader
      dwarfs_writer
  )
  set_property(TARGET dwarfs_test_fixtures PROPERTY CXX_STANDARD ${DWARFS_CXX_STANDARD})
endif()

# ============================================================================
# Test Targets
# ============================================================================

if(WITH_TESTS)
  # Include GoogleTest module for gtest_discover_tests
  include(GoogleTest)

  # Modular test executables for specific subsystems
  # These test specific functionality in focused test suites

  # Filesystem tests - Core filesystem operations
  add_executable(dwarfs_filesystem_tests
    test/filesystem/filesystem_uid_gid_test.cpp
    test/filesystem/filesystem_basic_test.cpp
    test/filesystem/filesystem_operations_test.cpp
  )
  target_link_libraries(dwarfs_filesystem_tests
    PRIVATE dwarfs_test_fixtures GTest::gtest_main
  )
  # MinGW/MSYS: Disable ENABLE_EXPORTS
  if(COMMAND fix_mingw_exe_exports)
    fix_mingw_exe_exports(dwarfs_filesystem_tests)
  endif()
  target_compile_definitions(dwarfs_filesystem_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test"
    TOOLS_BIN_DIR="${CMAKE_CURRENT_BINARY_DIR}"
  )
  # Add FUSE-T RPATH for test targets that link to dwarfs_reader
  add_fuse_rpath_to_tests(dwarfs_filesystem_tests)
  gtest_discover_tests(dwarfs_filesystem_tests)
endif()

# Additional libraries for compression benchmark
if(TARGET dwarfs_compression_benchmark)
  target_link_libraries(dwarfs_compression_benchmark PRIVATE
    dwarfs_compressor dwarfs_decompressor)
endif()

# Development tools
if(BUILD_FSST_MAIN)
  add_executable(fsst_main fsst/fsst.cpp)
  target_link_libraries(fsst_main PRIVATE dwarfs_fsst)
  # MinGW/MSYS: Disable ENABLE_EXPORTS
  if(COMMAND fix_mingw_exe_exports)
    fix_mingw_exe_exports(fsst_main)
  endif()
endif()

# Compatibility target deleted. The following tests are here for reference.
# Main monolithic test suite (to be deprecated)

if(WITH_TESTS)
  # Modular test suites extracted from dwarfs_test.cpp
  add_executable(dwarfs_segmenter_tests
    test/segmenter/segmenter_test.cpp
  )
  target_link_libraries(dwarfs_segmenter_tests
    PRIVATE dwarfs_test_helpers GTest::gtest_main
  )

  add_executable(dwarfs_filter_tests
    test/filter/filter_test.cpp
  )
  target_link_libraries(dwarfs_filter_tests
    PRIVATE dwarfs_test_helpers GTest::gtest_main
  )

  add_executable(dwarfs_compression_tests
    test/compression/compression_test.cpp
    test/compression/compression_regression_test.cpp
  )
  target_link_libraries(dwarfs_compression_tests
    PRIVATE dwarfs_test_helpers GTest::gtest_main
  )

  # Scanner and metadata tests removed - they depend on functions in dwarfs_test.cpp anonymous namespace
  # TODO: Refactor to make these extractable

  # Tests that require folly
  set(FOLLY_TESTS
    test/compat_test.cpp
    test/block_merger_test.cpp
    test/filesystem_test.cpp
    test/integral_value_parser_test.cpp
    test/io_ops_test.cpp
    test/os_access_generic_test.cpp
    test/pcm_sample_transformer_test.cpp
    test/utils_test.cpp
  )

  add_executable(dwarfs_unit_tests
    test/align_advise_range_test.cpp
    test/badfs_test.cpp
    test/backend_compatibility_test.cpp
    test/block_range_test.cpp
    test/byte_buffer_test.cpp
    test/checksum_test.cpp
    test/chmod_transformer_test.cpp
    test/compare_directories_test.cpp
    test/entry_test.cpp
    test/environment_variables_test.cpp
    test/error_test.cpp
    test/file_access_test.cpp
    test/file_range_utils_test.cpp
    test/file_utils_test.cpp
    test/file_view_test.cpp
    test/filesystem_writer_test.cpp
    test/fragment_category_test.cpp
    test/fsst_test.cpp
    test/glob_matcher_test.cpp
    test/global_metadata_test.cpp
    test/lazy_value_test.cpp
    test/lru_cache_test.cpp
    test/mappable_file_test.cpp
    test/metadata_factory_test.cpp
    test/metadata_requirements_test.cpp
 test/metadata_test.cpp
    test/metadata_view_interface_test.cpp
    test/metadata/serialization_test.cpp
    test/metadata/serialization/serialization_facade_test.cpp
    test/metadata/serialization_benchmark_test.cpp
    test/metadata/format_conversion_test.cpp
    test/metadata/converters/round_trip_string_table_test.cpp
    test/metadata/converters/thrift_metadata_converter_test.cpp
    test/metadata/converters/flatbuffers_converter_test.cpp
    test/metadata/converter_roundtrip_test.cpp
    test/nilsimsa_test.cpp
    test/options_test.cpp
    test/packed_int_vector_test.cpp
    test/packed_ptr_test.cpp
    test/sorted_array_map_test.cpp
    test/sparse_file_seeker_test.cpp
    test/speedometer_test.cpp
    test/string_test.cpp
    test/terminal_test.cpp
    test/test_iolayer_test.cpp
    test/time_resolution_converter_test.cpp
    test/unicode_test.cpp
    test/worker_group_test.cpp
    test/xattr_test.cpp
    # Homebrew compatibility tests
    test/homebrew_compatibility_test.cpp
    # Modern Thrift metadata tests
    test/metadata/modern_thrift_serialization_test.cpp
    test/metadata/modern/converter_test.cpp
  )

  # Add folly-dependent tests only if folly is available
  if(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
    target_sources(dwarfs_unit_tests PRIVATE ${FOLLY_TESTS})
  endif()
  target_link_libraries(dwarfs_unit_tests
    PRIVATE dwarfs_test_helpers GTest::gtest_main GTest::gmock
  )

  # Link Modern Thrift library if available
  if(DWARFS_HAVE_EXPERIMENTAL_THRIFT AND TARGET dwarfs_metadata_modern_thrift)
    target_link_libraries(dwarfs_unit_tests PRIVATE dwarfs_metadata_modern_thrift)
  endif()

  # Categorizer tests require folly
  if(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
    add_executable(dwarfs_categorizer_tests
      test/fits_categorizer_test.cpp
      test/incompressible_categorizer_test.cpp
      test/pcmaudio_categorizer_test.cpp
    )
    target_link_libraries(dwarfs_categorizer_tests
      PRIVATE dwarfs_test_helpers GTest::gtest_main
    )
  endif()

  # Expensive tests (dwarfs_test.cpp) uses folly
  if(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
    add_executable(dwarfs_expensive_tests
      test/dwarfs_test.cpp
    )
    target_link_libraries(dwarfs_expensive_tests
      PRIVATE dwarfs_test_helpers GTest::gtest_main
    )
  endif()

  if(FLAC_FOUND OR ENABLE_RICEPP)
    add_executable(dwarfs_compressor_tests)
    list(APPEND DWARFS_TESTS dwarfs_compressor_tests)
    if(FLAC_FOUND)
      target_sources(dwarfs_compressor_tests PRIVATE test/flac_compressor_test.cpp)
    endif()
    # ricepp_compressor_test.cpp uses folly/lang/Bits.h
    if(ENABLE_RICEPP AND DWARFS_HAVE_EXPERIMENTAL_THRIFT)
      target_sources(dwarfs_compressor_tests PRIVATE test/ricepp_compressor_test.cpp)
    endif()
    target_link_libraries(dwarfs_compressor_tests
      PRIVATE dwarfs_test_helpers GTest::gtest_main
    )
  endif()

  # Comprehensive compression algorithm benchmark
  add_executable(dwarfs_compression_benchmark
    test/compression_algorithm_benchmark.cpp
  )
  target_link_libraries(dwarfs_compression_benchmark
    PRIVATE dwarfs_test_helpers GTest::gtest_main
  )
  list(APPEND DWARFS_TESTS dwarfs_compression_benchmark)

  # Add FUSE-T RPATH to all test targets that link to dwarfs_reader
  # This is needed because static libraries don't propagate RPATH to executables
  add_fuse_rpath_to_tests(
    dwarfs_filesystem_tests
    dwarfs_segmenter_tests
    dwarfs_filter_tests
    dwarfs_compression_tests
    dwarfs_unit_tests
    dwarfs_categorizer_tests
    dwarfs_expensive_tests
    dwarfs_compressor_tests
    dwarfs_compression_benchmark
  )
endif()

# ============================================================================
# Benchmark Targets
# ============================================================================

if(WITH_LIBDWARFS AND WITH_BENCHMARKS)
  find_package(benchmark 1.8)
  if(benchmark_FOUND)
    add_executable(dwarfs_benchmark test/dwarfs_benchmark.cpp)
    target_link_libraries(dwarfs_benchmark PRIVATE dwarfs_test_helpers benchmark::benchmark)
    target_link_libraries(dwarfs_benchmark PRIVATE dwarfs_reader dwarfs_writer)
    list(APPEND BENCHMARK_TARGETS dwarfs_benchmark)

    add_executable(segmenter_benchmark test/segmenter_benchmark.cpp)
    target_link_libraries(segmenter_benchmark PRIVATE dwarfs_test_helpers benchmark::benchmark)
    target_link_libraries(segmenter_benchmark PRIVATE dwarfs_writer)
    list(APPEND BENCHMARK_TARGETS segmenter_benchmark)

    if(WITH_ALL_BENCHMARKS)
      add_executable(multiversioning_benchmark test/multiversioning_benchmark.cpp)
      target_link_libraries(multiversioning_benchmark PRIVATE benchmark::benchmark)
      target_link_libraries(multiversioning_benchmark PRIVATE dwarfs_writer)
      list(APPEND BENCHMARK_TARGETS multiversioning_benchmark)

      # converter_benchmark uses folly, only build if available
      if(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
        add_executable(converter_benchmark test/converter_benchmark.cpp)
        target_link_libraries(converter_benchmark PRIVATE dwarfs_test_helpers benchmark::benchmark)
        list(APPEND BENCHMARK_TARGETS converter_benchmark)
      endif()

      add_executable(sorted_array_map_benchmark test/sorted_array_map_benchmark.cpp)
      target_link_libraries(sorted_array_map_benchmark PRIVATE benchmark::benchmark)
      list(APPEND BENCHMARK_TARGETS sorted_array_map_benchmark)

      add_executable(nilsimsa_benchmark test/nilsimsa_benchmark.cpp)
      target_link_libraries(nilsimsa_benchmark PRIVATE benchmark::benchmark)
      target_link_libraries(nilsimsa_benchmark PRIVATE dwarfs_writer)
      list(APPEND BENCHMARK_TARGETS nilsimsa_benchmark)
    endif()

    list(APPEND BENCHMARK_TARGETS ${BENCHMARK_TARGETS})

    # Add FUSE-T RPATH for benchmark targets that link to dwarfs_reader
    add_fuse_rpath_to_tests(
      dwarfs_benchmark
      segmenter_benchmark
      multiversioning_benchmark
      converter_benchmark
      nilsimsa_benchmark
    )
  endif()
endif()

# ============================================================================
# libdwarfs API Benchmarks
# ============================================================================

if(WITH_LIBDWARFS AND WITH_BENCHMARKS)
  # Add libdwarfs API benchmarks subdirectory
  add_subdirectory(benchmarks/libdwarfs)

  # Add targets to benchmark list (for convenience)
  list(APPEND BENCHMARK_TARGETS
    single_file_bench
    multiple_files_bench
    full_extract_bench
    random_access_bench
  )

  # Add FUSE-T RPATH for libdwarfs API benchmarks that link to dwarfs_reader
  add_fuse_rpath_to_tests(
    single_file_bench
    multiple_files_bench
    full_extract_bench
    random_access_bench
  )
endif()

# ============================================================================
# Fuzz Targets
# ============================================================================

if(WITH_LIBDWARFS AND WITH_FUZZ)
  add_executable(fuzz_categorizers test/fuzz_categorizers.cpp)
  target_link_libraries(fuzz_categorizers PRIVATE dwarfs_writer)
  list(APPEND BINARY_TARGETS fuzz_categorizers)

  add_executable(fuzz_mkdwarfs test/fuzz_mkdwarfs.cpp)
  target_link_libraries(fuzz_mkdwarfs PRIVATE mkdwarfs_main dwarfs_test_helpers)
  list(APPEND BINARY_TARGETS fuzz_mkdwarfs)

  add_executable(fuzz_reader test/fuzz_reader.cpp)
  target_link_libraries(fuzz_reader PRIVATE dwarfs_reader)
  list(APPEND BINARY_TARGETS fuzz_reader)

  # Add FUSE-T RPATH for fuzz targets that link to dwarfs_reader
  add_fuse_rpath_to_tests(
    fuzz_mkdwarfs
    fuzz_reader
  )
endif()

# ============================================================================
# Development Tools
# ============================================================================

if(BUILD_FSST_MAIN)
  add_executable(fsst_main fsst/fsst.cpp)
  target_link_libraries(fsst_main PRIVATE dwarfs_fsst)
endif()