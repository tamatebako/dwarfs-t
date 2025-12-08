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
if(DEFINED ENV{TEBAKO_BUILD} OR TEBAKO_BUILD)
  cmake_minimum_required(VERSION 3.24.0)
else()
  cmake_minimum_required(VERSION 3.28.0)
endif()

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

    add_executable(sparsedump test/sparsedump.cpp)
    target_link_libraries(treediff PRIVATE dwarfs_common)

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
# Test Targets
# ============================================================================

if(WITH_TESTS)
  if(WITH_LIBDWARFS)
    add_executable(dwarfs_unit_tests
      test/align_advise_range_test.cpp
      test/badfs_test.cpp
      test/backend_compatibility_test.cpp
      test/block_merger_test.cpp
      test/block_range_test.cpp
      test/byte_buffer_test.cpp
      test/checksum_test.cpp
      test/chmod_transformer_test.cpp
      test/compare_directories_test.cpp
      test/entry_test.cpp
      test/error_test.cpp
      test/file_access_test.cpp
      test/file_range_utils_test.cpp
      test/file_utils_test.cpp
      test/file_view_test.cpp
      test/filesystem_test.cpp
      test/filesystem_writer_test.cpp
      test/fragment_category_test.cpp
      test/fsst_test.cpp
      test/glob_matcher_test.cpp
      test/global_metadata_test.cpp
      test/integral_value_parser_test.cpp
      test/lazy_value_test.cpp
      test/lru_cache_test.cpp
      test/mappable_file_test.cpp
      test/io_ops_test.cpp
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
      test/nilsimsa_test.cpp
      test/options_test.cpp
      test/os_access_generic_test.cpp
      test/packed_int_vector_test.cpp
      test/packed_ptr_test.cpp
      test/pcm_sample_transformer_test.cpp
      test/sorted_array_map_test.cpp
      test/sparse_file_seeker_test.cpp
      test/speedometer_test.cpp
      test/string_test.cpp
      test/terminal_test.cpp
      test/test_iolayer_test.cpp
      test/time_resolution_converter_test.cpp
      test/unicode_test.cpp
      test/utils_test.cpp
      test/worker_group_test.cpp
      test/xattr_test.cpp
    )

    add_executable(dwarfs_categorizer_tests
      test/fits_categorizer_test.cpp
      test/incompressible_categorizer_test.cpp
      test/pcmaudio_categorizer_test.cpp
    )

    add_executable(dwarfs_expensive_tests
      test/compat_test.cpp
      test/dwarfs_test.cpp
    )

    list(APPEND DWARFS_TESTS
      dwarfs_categorizer_tests
      dwarfs_expensive_tests
      dwarfs_unit_tests
    )

    if(FLAC_FOUND OR ENABLE_RICEPP)
      add_executable(dwarfs_compressor_tests)
      list(APPEND DWARFS_TESTS dwarfs_compressor_tests)
      if(FLAC_FOUND)
        target_sources(dwarfs_compressor_tests PRIVATE test/flac_compressor_test.cpp)
      endif()
      if(ENABLE_RICEPP)
        target_sources(dwarfs_compressor_tests PRIVATE test/ricepp_compressor_test.cpp)
      endif()
    endif()

    # Comprehensive compression algorithm benchmark
    add_executable(dwarfs_compression_benchmark
      test/compression_algorithm_benchmark.cpp
    )
    list(APPEND DWARFS_TESTS dwarfs_compression_benchmark)
  endif()

  if(WITH_TOOLS)
    add_executable(tool_main_test
      test/tool_main_cmdline_test.cpp
      test/tool_main_logging_test.cpp
      test/tool_main_perfmon_test.cpp
      test/tool_dwarfsck_main_basic_test.cpp
      test/tool_dwarfsextract_main_basic_test.cpp
      test/tool_mkdwarfs_main_basic_test.cpp
      test/tool_mkdwarfs_main_build_test.cpp
      test/tool_mkdwarfs_main_metadata_test.cpp
      test/tool_mkdwarfs_main_rebuild_test.cpp
      test/tool_mkdwarfs_main_recompress_test.cpp
      test/tool_mkdwarfs_main_sparse_test.cpp
      test/tool_mkdwarfs_main_time_resolution_test.cpp
      test/tool_mkdwarfs_integration_test.cpp
    )

    list(APPEND DWARFS_TESTS
      block_cache_test
      tool_main_test
    )
  endif()

  if(WITH_TOOLS OR WITH_FUSE_DRIVER)
    if(NOT WITH_TOOLS)
      find_program(MKDWARFS_EXE mkdwarfs mkdwarfs.exe)
      find_program(DWARFSCK_EXE dwarfsck dwarfsck.exe)
      find_program(DWARFSEXTRACT_EXE dwarfsextract dwarfsextract.exe)
    endif()
    if(WITH_TOOLS OR (MKDWARFS_EXE AND DWARFSCK_EXE AND DWARFSEXTRACT_EXE))
      list(APPEND DWARFS_TESTS
        tools_test
      )
    endif()
  endif()

  if((WITH_TOOLS OR WITH_FUSE_DRIVER) AND WITH_MAN_OPTION)
    list(APPEND DWARFS_TESTS manpage_test)
  endif()

  add_library(dwarfs_test_main OBJECT test/test_main.cpp)
  target_link_libraries(dwarfs_test_main PUBLIC GTest::gtest GTest::gmock dwarfs_common)
  if(DWARFS_STACKTRACE_ENABLED)
    target_link_libraries(dwarfs_test_main PRIVATE cpptrace::cpptrace)
  endif()

  foreach (test ${DWARFS_TESTS})
    if(NOT TARGET ${test})
      add_executable(${test} test/${test}.cpp)
    endif()

    if(CMAKE_CROSSCOMPILING)
      target_compile_definitions(${test} PRIVATE DWARFS_TEST_CROSS_COMPILE)
    endif()

    target_link_libraries(
      ${test} PRIVATE dwarfs_test_helpers dwarfs_test_main
    )

    if(NOT PREFER_SYSTEM_GTEST)
      ### This is a wild hack. At least on macOS, gtest and basically everything
      ### Homebrew is installed in /usr/local, and /usr/local/include can end up
      ### in the compiler's include path *before* the include path of our local
      ### gtest/gmock. The following code tries to ensure that the gtest/gmock
      ### include paths are searched first.
      get_target_property(gmock_include_dirs gmock INTERFACE_INCLUDE_DIRECTORIES)
      get_target_property(gtest_include_dirs gtest INTERFACE_INCLUDE_DIRECTORIES)
      target_include_directories(${test} SYSTEM BEFORE PRIVATE ${gmock_include_dirs} ${gtest_include_dirs})
    endif()

    if(WIN32)
      target_compile_options(${test} PRIVATE /bigobj)
    endif()

    list(APPEND TEST_TARGETS ${test})
  endforeach()

  if(TARGET tool_main_test)
    target_link_libraries(tool_main_test PRIVATE dwarfs_tool_main_tester PkgConfig::LIBARCHIVE)
  endif()

  if(TARGET dwarfs_unit_tests)
    target_link_libraries(dwarfs_unit_tests PRIVATE phmap)
    # Add serialization dependencies for metadata/serialization_test.cpp
    if(TARGET bitsery)
      target_link_libraries(dwarfs_unit_tests PRIVATE bitsery)
    endif()
    if(TARGET cereal::cereal)
      target_link_libraries(dwarfs_unit_tests PRIVATE cereal::cereal)
    endif()
    if(TARGET folly AND TARGET dwarfs_metadata_thrift)
      target_link_libraries(dwarfs_unit_tests PRIVATE folly dwarfs_metadata_thrift)
    endif()
  endif()

  if(TARGET manpage_test)
    if(WITH_TOOLS)
      target_compile_definitions(manpage_test PRIVATE DWARFS_WITH_TOOLS)
      target_link_libraries(manpage_test PRIVATE mkdwarfs_main dwarfsck_main dwarfsextract_main)
    endif()
    if(TARGET dwarfs_main)
      target_compile_definitions(manpage_test PRIVATE DWARFS_WITH_FUSE_DRIVER)
      target_link_libraries(manpage_test PRIVATE dwarfs_main)
    endif()
  endif()

  if(TARGET tools_test)
    target_compile_definitions(tools_test PRIVATE
      $<$<AND:$<BOOL:${WITH_UNIVERSAL_BINARY}>,$<BOOL:${WITH_TOOLS}>>:DWARFS_HAVE_UNIVERSAL_BINARY>
      $<$<BOOL:${WITH_TOOLS}>:DWARFS_WITH_TOOLS>
      $<$<BOOL:${WITH_FUSE_DRIVER}>:DWARFS_WITH_FUSE_DRIVER>
      $<$<BOOL:${MKDWARFS_EXE}>:MKDWARFS_BINARY=\"${MKDWARFS_EXE}\">
      $<$<BOOL:${DWARFSCK_EXE}>:DWARFSCK_BINARY=\"${DWARFSCK_EXE}\">
      $<$<BOOL:${DWARFSEXTRACT_EXE}>:DWARFSEXTRACT_BINARY=\"${DWARFSEXTRACT_EXE}\">
      $<$<BOOL:${CMAKE_CROSSCOMPILING}>:DWARFS_CROSSCOMPILING_EMULATOR=\"${CMAKE_CROSSCOMPILING_EMULATOR}\">
    )
    if(NOT WIN32)
      string(REPLACE ";" ":" DWARFS_CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}")
      target_compile_definitions(tools_test PRIVATE
        DWARFS_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
        $<$<BOOL:${CMAKE_PREFIX_PATH}>:DWARFS_CMAKE_PREFIX_PATH=\"${DWARFS_CMAKE_PREFIX_PATH}\">
      )
    endif()
  endif()

  if(TARGET block_cache_test)
    target_link_libraries(block_cache_test PRIVATE mkdwarfs_main)
  endif()

  foreach(tgt dwarfs_categorizer_tests
              dwarfs_compression_benchmark
              tool_main_test)
    if(TARGET ${tgt})
      target_link_libraries(${tgt} PRIVATE dwarfs_writer)
    endif()
  endforeach()
  
  # Add rewrite and extractor libraries for expensive tests
  if(TARGET dwarfs_expensive_tests)
    target_link_libraries(dwarfs_expensive_tests PRIVATE dwarfs_rewrite dwarfs_extractor)
  endif()
  
  # Additional libraries for compression benchmark
  if(TARGET dwarfs_compression_benchmark)
    target_link_libraries(dwarfs_compression_benchmark PRIVATE
      dwarfs_compressor dwarfs_decompressor)
  endif()

  if(ENABLE_COVERAGE)
    list(APPEND GTEST_COMMON_PROPERTIES
         "ENVIRONMENT" "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/profile/%32m.profraw")
  endif()

  if(ENABLE_TSAN)
    list(APPEND GTEST_COMMON_PROPERTIES
         "ENVIRONMENT" "TSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/tsan.supp")
  endif()

  foreach(tgt ${TEST_TARGETS})
    gtest_discover_tests(${tgt}
      DISCOVERY_TIMEOUT 120
      PROPERTIES ${GTEST_COMMON_PROPERTIES}
    )
  endforeach()

  foreach(tgt dwarfs_categorizer_tests tools_test)
    if(TARGET ${tgt})
      gtest_discover_tests(${tgt}
        DISCOVERY_TIMEOUT 120
        TEST_SUFFIX ".iolayer-read"
        PROPERTIES ${GTEST_COMMON_PROPERTIES}
                   ENVIRONMENT "DWARFS_IOLAYER_OPTS=open_mode=read"
      )
      gtest_discover_tests(${tgt}
        DISCOVERY_TIMEOUT 120
        TEST_SUFFIX ".iolayer-map-size"
        PROPERTIES ${GTEST_COMMON_PROPERTIES}
                   ENVIRONMENT "DWARFS_IOLAYER_OPTS=max_eager_map_size=4k"
      )
    endif()
  endforeach()
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

      add_executable(converter_benchmark test/converter_benchmark.cpp)
      target_link_libraries(converter_benchmark PRIVATE dwarfs_test_helpers benchmark::benchmark)
      list(APPEND BENCHMARK_TARGETS converter_benchmark)

      add_executable(sorted_array_map_benchmark test/sorted_array_map_benchmark.cpp)
      target_link_libraries(sorted_array_map_benchmark PRIVATE benchmark::benchmark)
      list(APPEND BENCHMARK_TARGETS sorted_array_map_benchmark)

      add_executable(nilsimsa_benchmark test/nilsimsa_benchmark.cpp)
      target_link_libraries(nilsimsa_benchmark PRIVATE benchmark::benchmark)
      target_link_libraries(nilsimsa_benchmark PRIVATE dwarfs_writer)
      list(APPEND BENCHMARK_TARGETS nilsimsa_benchmark)
    endif()

    list(APPEND BINARY_TARGETS ${BENCHMARK_TARGETS})
  endif()
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
endif()

# ============================================================================
# Development Tools
# ============================================================================

if(BUILD_FSST_MAIN)
  add_executable(fsst_main fsst/fsst.cpp)
  target_link_libraries(fsst_main PRIVATE dwarfs_fsst)
endif()