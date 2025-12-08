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

add_library(
  dwarfs_common

  src/byte_buffer.cpp
  src/checksum.cpp
  src/compression_registry.cpp
  src/conv.cpp
  src/detail/file_extent_info.cpp
  src/detail/scoped_env.cpp
  src/error.cpp
  src/extent_kind.cpp
  src/file_access_generic.cpp
  src/file_extent.cpp
  src/file_extents_iterable.cpp
  src/file_range_utils.cpp
  src/file_segments_iterable.cpp
  src/file_stat.cpp
  src/file_util.cpp
  src/fstypes.cpp
  src/glob_matcher.cpp
  src/history.cpp
  src/library_dependencies.cpp
  src/logger.cpp
  src/malloc_byte_buffer.cpp
  src/mapped_byte_buffer.cpp
  src/option_map.cpp
  src/os_access_generic.cpp
  src/pcm_sample_transformer.cpp
  $<IF:$<BOOL:${ENABLE_PERFMON}>,src/performance_monitor.cpp,>
  src/terminal_ansi.cpp
  src/thread_pool.cpp
  src/util.cpp
  src/varint.cpp
  src/xattr.cpp

  src/internal/features.cpp
  src/internal/file_status_conv.cpp
  src/internal/fs_section.cpp
  src/internal/fs_section_checker.cpp
  src/internal/fsst.cpp
  src/internal/glob_to_regex.cpp
  src/internal/malloc_buffer.cpp
  src/internal/mappable_file.cpp
  src/internal/io_ops_$<IF:$<BOOL:${WIN32}>,win,posix>.cpp
  src/internal/io_ops_helpers.cpp
  $<$<OR:$<BOOL:${DWARFS_HAVE_THRIFT}>,$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>>:src/internal/metadata_utils.cpp>
  src/internal/mmap_file_view.cpp
  src/internal/option_parser.cpp
  src/internal/os_access_generic_data.cpp
  src/internal/read_file_view.cpp
  $<$<OR:$<BOOL:${DWARFS_HAVE_THRIFT}>,$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>>:src/internal/string_table.cpp>
  src/internal/thread_util.cpp
  src/internal/unicode_case_folding.cpp
  src/internal/wcwidth.c
  src/internal/worker_group.cpp

  src/xattr_$<IF:$<BOOL:${WIN32}>,win,posix>.cpp

  $<IF:${DWARFS_GIT_BUILD},${CMAKE_CURRENT_BINARY_DIR},${CMAKE_CURRENT_SOURCE_DIR}>/src/version.cpp

  src/compression/base.cpp
  src/compression/null.cpp
  src/compression/zstd.cpp
  $<$<BOOL:${LIBLZMA_FOUND}>:src/compression/lzma.cpp>
  $<$<BOOL:${LIBLZ4_FOUND}>:src/compression/lz4.cpp>
  $<$<AND:$<BOOL:${LIBBROTLIDEC_FOUND}>,$<BOOL:${LIBBROTLIENC_FOUND}>>:src/compression/brotli.cpp>
  $<$<BOOL:${FLAC_FOUND}>:src/compression/flac.cpp>
  $<$<BOOL:${ENABLE_RICEPP}>:src/compression/ricepp.cpp>

  # Metadata serialization sources
  ${DWARFS_SERIALIZATION_SOURCES}
)

add_library(
  dwarfs_compressor

  src/block_compressor.cpp
  src/block_compressor_parser.cpp
  src/compressor_registry.cpp
)

add_library(
  dwarfs_decompressor

  src/block_decompressor.cpp
  src/decompressor_registry.cpp
)

add_library(
  dwarfs_reader

  src/reader/block_cache_options.cpp
  src/reader/block_range.cpp
  src/reader/detail/file_reader.cpp
  src/reader/filesystem_options.cpp
  src/reader/filesystem_v2.cpp
  src/reader/fsinfo_features.cpp
  src/reader/mlock_mode.cpp
  src/reader/metadata_types.cpp
  src/reader/filesystem_loader.cpp
  $<$<AND:$<BOOL:${WITH_LIBDWARFS}>,$<BOOL:${WITH_FUSE_DRIVER}>>:src/reader/fuse_driver.cpp>

  # Interface layer (always available, format-agnostic)
  include/dwarfs/reader/internal/metadata_factory.h

  include/dwarfs/reader/internal/metadata_types_flatbuffers.h

  src/reader/internal/block_cache.cpp
  src/reader/internal/block_cache_byte_buffer_factory.cpp
  src/reader/internal/cached_block.cpp
  src/reader/internal/filesystem_parser.cpp
  src/reader/internal/inode_reader_v2.cpp
  src/reader/internal/metadata_analyzer.cpp
  src/reader/internal/metadata_factory.cpp
  $<$<BOOL:${DWARFS_HAVE_THRIFT}>:src/reader/internal/metadata_types_thrift.cpp>
  $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/reader/internal/metadata_types_flatbuffers.cpp>
  src/reader/internal/periodic_executor.cpp
  
  # metadata_v2 implementation (always needed)
  src/reader/internal/metadata_v2.cpp

  # Factory - only in dual-format builds
  $<$<AND:$<BOOL:${DWARFS_HAVE_THRIFT}>,$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>>:src/reader/internal/metadata_v2_factory.cpp>
)

# In dual-format mode, ensure FlatBuffers implementation uses FlatBuffers types
if(DWARFS_HAVE_THRIFT AND DWARFS_HAVE_FLATBUFFERS)
  set_source_files_properties(
    src/reader/internal/metadata_types_flatbuffers.cpp
    PROPERTIES
    COMPILE_OPTIONS "-DDWARFS_FLATBUFFERS_BACKEND"
  )
endif()

# In dual-format mode, ensure FlatBuffers implementation uses FlatBuffers types
if(DWARFS_HAVE_THRIFT AND DWARFS_HAVE_FLATBUFFERS)
  set_source_files_properties(
    src/reader/internal/metadata_types_flatbuffers.cpp
    src/reader/internal/metadata_v2_flatbuffers.cpp
    PROPERTIES
    COMPILE_OPTIONS "-DDWARFS_FLATBUFFERS_BACKEND"
  )
endif()

target_link_libraries(dwarfs_reader
  PRIVATE
  $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:flatbuffers>
  PUBLIC
  dwarfs_common
  dwarfs_decompressor
)

add_library(
  dwarfs_writer

  src/writer/categorizer.cpp
  src/writer/category_parser.cpp
  src/writer/compression_metadata_requirements.cpp
  src/writer/console_writer.cpp
  src/writer/entry_factory.cpp
  src/writer/fragment_order_options.cpp
  src/writer/filesystem_block_category_resolver.cpp
  src/writer/filesystem_writer.cpp
  src/writer/filter_debug.cpp
  src/writer/fragment_category.cpp
  src/writer/fragment_order_parser.cpp
  src/writer/inode_fragments.cpp
  src/writer/metadata_options.cpp
  src/writer/rule_based_entry_filter.cpp
  src/writer/scanner.cpp
  src/writer/segmenter.cpp
  src/writer/segmenter_factory.cpp
  src/writer/writer_progress.cpp

  src/writer/internal/block_manager.cpp
  src/writer/internal/chmod_transformer.cpp
  src/writer/internal/entry.cpp
  src/writer/internal/file_scanner.cpp
  src/writer/internal/fragment_chunkable.cpp
  src/writer/internal/global_entry_data.cpp
  src/writer/internal/inode_element_view.cpp
  src/writer/internal/inode_hole_mapper.cpp
  src/writer/internal/inode_manager.cpp
  $<$<BOOL:${DWARFS_HAVE_THRIFT}>:src/writer/internal/inode_hole_mapper.cpp>
  $<$<BOOL:${DWARFS_HAVE_THRIFT}>:src/writer/internal/inode_manager.cpp>
  src/writer/internal/inode_ordering.cpp

  src/writer/internal/metadata_builder.cpp  # Constructor implementations (always needed)
  # Strategy Pattern - separate implementations (both conditional)
  src/writer/internal/metadata_builder_factory.cpp  # Factory (always needed)
  $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_metadata_builder.cpp>  # FlatBuffers strategy
  $<$<BOOL:${DWARFS_HAVE_THRIFT}>:src/writer/internal/thrift_metadata_builder.cpp>  # Thrift strategy
  
  # Utility classes (always compiled)
  src/writer/internal/inode_size_calculator.cpp
  src/writer/internal/metadata_validator.cpp
  
  # FlatBuffers processors (conditional)
  $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_chunk_processor.cpp>
  $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_entry_processor.cpp>
  $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_packing_processor.cpp>
  $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_upgrade_processor.cpp>

  src/writer/internal/metadata_freezer.cpp
  src/writer/internal/nilsimsa.cpp
  src/writer/internal/progress.cpp
  src/writer/internal/scanner_progress.cpp
  src/writer/internal/similarity.cpp
  src/writer/internal/similarity_ordering.cpp
  src/writer/internal/time_resolution_converter.cpp

  # src/writer/categorizer/binary_categorizer.cpp
  src/writer/categorizer/fits_categorizer.cpp
  src/writer/categorizer/hotness_categorizer.cpp
  src/writer/categorizer/incompressible_categorizer.cpp
  src/writer/categorizer/pcmaudio_categorizer.cpp

  # $<$<BOOL:${LIBMAGIC_FOUND}>:src/writer/categorizer/libmagic_categorizer.cpp>
)

if(DWARFS_HAVE_THRIFT)
  add_library(
    dwarfs_rewrite

    src/utility/rewrite_filesystem.cpp
  )
endif()

add_library(
  dwarfs_extractor

  src/utility/filesystem_extractor.cpp
)

add_library(
  dwarfs_fsst OBJECT
  fsst/libfsst.cpp
  fsst/fsst_avx512.cpp
  fsst/fsst_avx512_unroll1.inc
  fsst/fsst_avx512_unroll2.inc
  fsst/fsst_avx512_unroll3.inc
  fsst/fsst_avx512_unroll4.inc
)

# Thrift libraries - only create if Thrift is enabled
# (metadata_serialization.cmake will set DWARFS_HAVE_THRIFT)
if(NOT DEFINED DWARFS_WITH_THRIFT OR DWARFS_WITH_THRIFT)
  add_cpp2_thrift_library(thrift/metadata.thrift FROZEN
                          TARGET dwarfs_metadata_thrift OUTPUT_PATH dwarfs)
  add_cpp2_thrift_library(thrift/compression.thrift
                          TARGET dwarfs_compression_thrift OUTPUT_PATH dwarfs)
  add_cpp2_thrift_library(thrift/history.thrift
                          TARGET dwarfs_history_thrift OUTPUT_PATH dwarfs)
  add_cpp2_thrift_library(thrift/features.thrift
                          TARGET dwarfs_features_thrift OUTPUT_PATH dwarfs)
endif()

target_link_libraries(dwarfs_common PRIVATE PkgConfig::LIBCRYPTO PkgConfig::XXHASH PkgConfig::ZSTD)
# Link folly_lite only if Thrift support is enabled (folly.cmake was included)
if(DWARFS_HAVE_THRIFT AND TARGET dwarfs_folly_lite)
  target_link_libraries(dwarfs_common PRIVATE dwarfs_folly_lite)
endif()
target_link_libraries(dwarfs_compressor PRIVATE dwarfs_common)
target_link_libraries(dwarfs_decompressor PRIVATE dwarfs_common)
target_link_libraries(dwarfs_reader PUBLIC dwarfs_common dwarfs_decompressor)
target_link_libraries(dwarfs_writer PUBLIC dwarfs_common dwarfs_compressor dwarfs_decompressor)
target_link_libraries(dwarfs_writer PRIVATE PkgConfig::ZSTD)
target_link_libraries(dwarfs_extractor PUBLIC dwarfs_reader)
if(DWARFS_HAVE_THRIFT)
  target_link_libraries(dwarfs_rewrite PUBLIC dwarfs_reader dwarfs_writer)
endif()

target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)

# Add FlatBuffers generated headers include directory when FlatBuffers is enabled
if(DWARFS_HAVE_FLATBUFFERS)
  target_include_directories(dwarfs_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  )
endif()

target_compile_definitions(
  dwarfs_common PRIVATE
  DWARFS_SYSTEM_ID="${CMAKE_SYSTEM_NAME} [${CMAKE_SYSTEM_PROCESSOR}]"
  DWARFS_COMPILER_ID="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)

# Add serialization format definitions with PUBLIC visibility
# so they propagate to dependent targets
if(DWARFS_HAVE_CEREAL)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_CEREAL)
endif()

if(DWARFS_HAVE_BITSERY)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_BITSERY)
endif()

if(DWARFS_HAVE_THRIFT)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_THRIFT)
endif()

# FlatBuffers include directories are handled via dwarfs_metadata_flatbuffers target
# No need to set them explicitly here

if(ENABLE_RICEPP)
  target_link_libraries(dwarfs_common PRIVATE ${RICEPP_OBJECT_TARGETS})
endif()

# Link thrift_lite only if Thrift support is enabled
if(DWARFS_HAVE_THRIFT AND TARGET dwarfs_thrift_lite)
  target_link_libraries(dwarfs_common PRIVATE dwarfs_thrift_lite)
endif()

# Link serialization dependencies
if(DWARFS_HAVE_CEREAL)
  target_link_libraries(dwarfs_common PRIVATE cereal::cereal)
endif()

if(DWARFS_HAVE_BITSERY)
  target_link_libraries(dwarfs_common PRIVATE bitsery)
endif()

# Link full folly library when Thrift support is enabled
# (thrift headers pull in folly code requiring allocator symbols)
# PUBLIC linkage needed because dwarfs_reader uses folly symbols
if(DWARFS_HAVE_THRIFT)
  target_link_libraries(dwarfs_common PUBLIC folly)
endif()

# Link to FlatBuffers if enabled (REQUIRED)
if(TARGET dwarfs_metadata_flatbuffers)
  target_link_libraries(dwarfs_common PUBLIC dwarfs_metadata_flatbuffers)
  add_dependencies(dwarfs_common dwarfs_metadata_flatbuffers_generate)

  # Add FlatBuffers include directory to dwarfs_common as well
  target_include_directories(dwarfs_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  )
endif()

# Select metadata backend implementation based on availability
if(DWARFS_HAVE_THRIFT)
  target_sources(dwarfs_reader PRIVATE
    src/reader/internal/metadata_types_thrift.cpp
    src/reader/internal/metadata_v2_thrift.cpp
    src/reader/internal/time_resolution_handler.cpp)
endif()

# FlatBuffers backend compilation mode
if(DWARFS_HAVE_FLATBUFFERS)
  # Always compile types and views (needed for factory)
  target_sources(dwarfs_reader PRIVATE
    src/reader/internal/metadata_types_flatbuffers.cpp
    src/reader/internal/flatbuffer_metadata_views.cpp)
  
  if(DWARFS_HAVE_THRIFT)
    # Dual-format: compile minimal factory stub only (Thrift is primary backend)
    target_sources(dwarfs_reader PRIVATE
      src/reader/internal/metadata_v2_flatbuffers_factory.cpp)
  else()
    # FlatBuffers-only: compile full backend implementation
    target_sources(dwarfs_reader PRIVATE
      src/reader/internal/metadata_v2_flatbuffers.cpp
      src/reader/internal/time_resolution_handler.cpp)
  endif()

  # Add dependency on generated FlatBuffers headers
  add_dependencies(dwarfs_reader dwarfs_metadata_flatbuffers_generate)
  target_include_directories(dwarfs_reader PRIVATE ${CMAKE_BINARY_DIR}/include)
  # Note: FlatBuffers library linking is handled above in target_link_libraries()
endif()

# Ensure at least one backend is available
if(NOT DWARFS_HAVE_THRIFT AND NOT DWARFS_HAVE_FLATBUFFERS)
  message(FATAL_ERROR "No metadata implementation available (need THRIFT or FLATBUFFERS)")
endif()

if(WIN32)
  target_link_libraries(dwarfs_common PRIVATE bcrypt.lib)
endif()

if(LIBLZ4_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::LIBLZ4)
endif()

if(LIBLZMA_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::LIBLZMA)
endif()

if(FLAC_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::FLAC)
endif()

if(LIBBROTLIDEC_FOUND AND LIBBROTLIENC_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::LIBBROTLIDEC PkgConfig::LIBBROTLIENC)
endif()

if(ENABLE_STACKTRACE)
  target_link_libraries(dwarfs_common PUBLIC cpptrace::cpptrace)
endif()

target_link_libraries(dwarfs_extractor PRIVATE PkgConfig::LIBARCHIVE)

target_include_directories(dwarfs_common SYSTEM PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/fsst>)
set_property(TARGET dwarfs_fsst PROPERTY CXX_STANDARD ${DWARFS_CXX_STANDARD})
set_property(TARGET dwarfs_fsst PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET dwarfs_fsst PROPERTY CXX_EXTENSIONS OFF)

target_link_libraries(
  dwarfs_common
  PUBLIC
  Boost::boost
  Boost::chrono
  Boost::program_options
  dwarfs_fsst
)

# Link thrift libraries only if they exist
if(TARGET dwarfs_compression_thrift)
  target_link_libraries(dwarfs_common PUBLIC
    dwarfs_compression_thrift
    dwarfs_metadata_thrift
    dwarfs_history_thrift
    dwarfs_features_thrift)
endif()

if(TARGET Boost::process)
  target_link_libraries(dwarfs_common PUBLIC Boost::process)
endif()

list(APPEND LIBDWARFS_TARGETS
  dwarfs_common
  dwarfs_compressor
  dwarfs_decompressor
  dwarfs_reader
  dwarfs_writer
  dwarfs_extractor
)

if(DWARFS_HAVE_THRIFT)
  list(APPEND LIBDWARFS_TARGETS dwarfs_rewrite)
endif()

list(APPEND LIBDWARFS_OBJECT_TARGETS
  dwarfs_fsst
)

# Add thrift/folly targets only if they exist
if(TARGET dwarfs_folly_lite)
  list(APPEND LIBDWARFS_OBJECT_TARGETS dwarfs_folly_lite)
endif()
if(TARGET dwarfs_thrift_lite)
  list(APPEND LIBDWARFS_OBJECT_TARGETS dwarfs_thrift_lite)
endif()
if(TARGET dwarfs_compression_thrift)
  list(APPEND LIBDWARFS_OBJECT_TARGETS
    dwarfs_compression_thrift
    dwarfs_metadata_thrift
    dwarfs_history_thrift
    dwarfs_features_thrift)
endif()

# Add serialization targets to exports if they exist
if(TARGET bitsery)
  list(APPEND LIBDWARFS_OBJECT_TARGETS bitsery)
endif()

if(TARGET dwarfs_metadata_flatbuffers)
  list(APPEND LIBDWARFS_OBJECT_TARGETS dwarfs_metadata_flatbuffers)
endif()

if(NOT STATIC_BUILD_DO_NOT_USE)
  foreach(tgt ${LIBDWARFS_TARGETS})
    set_target_properties(${tgt} PROPERTIES VERSION ${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR}.${PRJ_VERSION_PATCH})
  endforeach()

  include(CMakePackageConfigHelpers)

  set(DWARFS_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/dwarfs CACHE STRING
      "CMake package config files install location")

  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dwarfs-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/dwarfs-config.cmake
    INSTALL_DESTINATION ${DWARFS_CMAKE_INSTALL_DIR}
    PATH_VARS
      CMAKE_INSTALL_INCLUDEDIR
      DWARFS_CMAKE_INSTALL_DIR
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/dwarfs-config-version.cmake
    VERSION ${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR}.${PRJ_VERSION_PATCH}
    COMPATIBILITY AnyNewerVersion
  )

  # Build install targets list conditionally
  set(_INSTALL_TARGETS ${LIBDWARFS_TARGETS} ${LIBDWARFS_OBJECT_TARGETS} ${RICEPP_OBJECT_TARGETS})
  if(TARGET folly_deps)
    list(APPEND _INSTALL_TARGETS folly_deps)
  endif()

  install(
    TARGETS ${_INSTALL_TARGETS}
    EXPORT dwarfs-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

  install(
    DIRECTORY include/dwarfs
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PATTERN include/dwarfs/internal EXCLUDE
    PATTERN include/dwarfs/*/internal EXCLUDE
  )

  if(DWARFS_GIT_BUILD)
    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/include/dwarfs/version.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dwarfs
    )
  endif()

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/include/dwarfs/config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dwarfs
  )

  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/dwarfs-config.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/dwarfs-config-version.cmake
    DESTINATION ${DWARFS_CMAKE_INSTALL_DIR}
  )

  install(
    EXPORT dwarfs-targets
    FILE dwarfs-targets.cmake
    NAMESPACE dwarfs::
    DESTINATION ${DWARFS_CMAKE_INSTALL_DIR}
  )
endif()
